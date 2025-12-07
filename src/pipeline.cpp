#include "pipeline.h"
#include "gps_embed.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <windows.h>

namespace fs = std::filesystem;

std::string getExecutableDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    fs::path exePath(buffer);
    return exePath.parent_path().string();
}

// Convert path to Windows short path name (8.3 format) to avoid space issues
std::string getShortPathName(const std::string& longPath) {
    char shortPath[MAX_PATH];
    DWORD length = GetShortPathNameA(longPath.c_str(), shortPath, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::string(shortPath);
    }
    // If conversion fails, return original path
    return longPath;
}

bool checkVendorFiles(LogCallback logCallback) {
    std::string exeDir = getExecutableDir();
    fs::path ffmpegPath = fs::path(exeDir) / "vendor" / "ffmpeg" / "bin" / "ffmpeg.exe";
    fs::path colmapPath = fs::path(exeDir) / "vendor" / "colmap" / "bin" / "colmap.bat";
    
    if (!fs::exists(ffmpegPath)) {
        logCallback("ERROR: FFmpeg not found at: " + ffmpegPath.string());
        return false;
    }
    
    if (!fs::exists(colmapPath)) {
        logCallback("ERROR: COLMAP not found at: " + colmapPath.string());
        return false;
    }
    
    logCallback("Vendor files OK: FFmpeg and COLMAP found");
    return true;
}

// Run command with hidden console window and capture output to GUI log
int runCommandHidden(const std::string& command, LogCallback logCallback) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    // Create pipes for stdout/stderr
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        logCallback("ERROR: Failed to create pipe");
        return -1;
    }
    
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // Hide the console window!
    
    PROCESS_INFORMATION pi = {};
    
    // Wrap command in cmd.exe
    std::string fullCommand = "cmd.exe /c " + command;
    char* cmdLine = _strdup(fullCommand.c_str());
    
    BOOL success = CreateProcessA(
        NULL, cmdLine, NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );
    
    free(cmdLine);
    CloseHandle(hWritePipe);
    
    if (!success) {
        CloseHandle(hReadPipe);
        DWORD error = GetLastError();
        logCallback("ERROR: Failed to start process (error code: " + std::to_string(error) + ")");
        return -1;
    }
    
    // Read output in chunks and log it
    char buffer[4096];
    DWORD bytesRead;
    std::string output;
    
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
        
        // Log output line by line
        size_t pos = 0;
        while ((pos = output.find('\n')) != std::string::npos) {
            std::string line = output.substr(0, pos);
            if (!line.empty() && line != "\r") {
                logCallback(line);
            }
            output.erase(0, pos + 1);
        }
    }
    
    // Log any remaining output
    if (!output.empty()) {
        logCallback(output);
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    
    return exitCode;
}

int runCommand(const std::string& command, LogCallback logCallback) {
    return runCommandHidden(command, logCallback);
}

bool extractFrames(const std::string& videoPath, const std::string& outputDir, 
                  double fps, LogCallback logCallback) {
    std::string exeDir = getExecutableDir();
    fs::path ffmpegPath = fs::path(exeDir) / "vendor" / "ffmpeg" / "bin" / "ffmpeg.exe";
    
    if (!fs::exists(ffmpegPath)) {
        logCallback("ERROR: FFmpeg not found");
        return false;
    }
    
    logCallback("Using FFmpeg: " + ffmpegPath.string());
    
    fs::path videoFilePath(videoPath);
    std::string videoStem = videoFilePath.stem().string();
    fs::path videoOutputDir = fs::path(outputDir) / videoStem;
    
    try {
        fs::create_directories(videoOutputDir);
    }
    catch (const std::exception& e) {
        logCallback("ERROR creating output directory: " + std::string(e.what()));
        return false;
    }
    
    std::string outputPattern = (videoOutputDir / (videoStem + "_frame_%04d.jpg")).string();
    
    // Build FFmpeg command with proper quoting for cmd.exe /c
    // Need to wrap the entire command in outer quotes for paths with spaces
    std::ostringstream cmdStream;
    cmdStream << "\"\"" << ffmpegPath.string() << "\" -i \"" << videoPath 
              << "\" -vf fps=" << fps << " -q:v 2 \"" << outputPattern << "\"\"";
    
    logCallback("Extracting frames at " + std::to_string(fps) + " fps...");
    
    int result = runCommand(cmdStream.str(), logCallback);
    
    if (result != 0) {
        logCallback("ERROR: FFmpeg failed with exit code " + std::to_string(result));
        return false;
    }
    
    // Count extracted frames
    int frameCount = 0;
    for (const auto& entry : fs::directory_iterator(videoOutputDir)) {
        if (entry.path().extension() == ".jpg") {
            frameCount++;
        }
    }
    
    logCallback("Extracted " + std::to_string(frameCount) + " frames to: " + videoOutputDir.string());
    
    // Try to embed GPS data from SRT into extracted frames using bundled exiftool
    try {
        fs::path srtPathUpper = videoFilePath;
        srtPathUpper.replace_extension(".SRT");
        fs::path srtPathLower = videoFilePath;
        srtPathLower.replace_extension(".srt");
        
        fs::path srtPath;
        if (fs::exists(srtPathUpper)) {
            srtPath = srtPathUpper;
        } else if (fs::exists(srtPathLower)) {
            srtPath = srtPathLower;
        }
        
        if (!srtPath.empty()) {
            // SRT file exists - check for bundled exiftool
            std::string exeDir = getExecutableDir();
            fs::path exiftoolPath = fs::path(exeDir) / "vendor" / "exiftool" / "exiftool.exe";
            
            if (fs::exists(exiftoolPath)) {
                logCallback("Found SRT file: " + srtPath.filename().string());
                logCallback("Parsing GPS data...");
                
                // Parse SRT file
                std::vector<GPSData> gpsFrames = parseSRT(srtPath.string());
                
                if (!gpsFrames.empty()) {
                    logCallback("Parsed " + std::to_string(gpsFrames.size()) + " GPS entries from SRT");
                    logCallback("Embedding GPS EXIF data into frames using exiftool...");
                    
                    // Get list of extracted frames
                    std::vector<fs::path> frameFiles;
                    for (const auto& entry : fs::directory_iterator(videoOutputDir)) {
                        if (entry.path().extension() == ".jpg") {
                            frameFiles.push_back(entry.path());
                        }
                    }
                    
                    std::sort(frameFiles.begin(), frameFiles.end());
                    
                    int embedded = 0;
                    for (size_t i = 0; i < frameFiles.size(); i++) {
                        double timestamp = i / fps;
                        GPSData gps = getGPSForTimestamp(gpsFrames, timestamp);
                        
                        if (gps.valid) {
                            std::string cmd = generateExiftoolCommand(
                                exiftoolPath.string(),
                                frameFiles[i].string(),
                                gps.latitude,
                                gps.longitude,
                                gps.altitude
                            );
                            
                            if (runCommandHidden(cmd, [](const std::string&){}) == 0) {
                                embedded++;
                            }
                        }
                    }
                    
                    logCallback("✅ Embedded GPS data into " + std::to_string(embedded) + "/" + 
                               std::to_string(frameFiles.size()) + " frames");
                } else {
                    logCallback("⚠ WARNING: No GPS data found in SRT file");
                }
            } else {
                logCallback("⚠ No exiftool found - skipping GPS embedding");
                logCallback("  To enable GPS embedding, exiftool should be at:");
                logCallback("  " + exiftoolPath.string());
            }
        } else {
            logCallback("ℹ No SRT file found for this video - skipping GPS embedding");
        }
    } catch (const std::exception& e) {
        logCallback("⚠ WARNING: GPS embedding failed: " + std::string(e.what()));
    }
    
    return frameCount > 0;
}

bool runColmap(const std::string& framesDir, const std::string& outputDir, 
              LogCallback logCallback) {
    std::string exeDir = getExecutableDir();
    fs::path colmapPath = fs::path(exeDir) / "vendor" / "colmap" / "bin" / "colmap.bat";
    
    if (!fs::exists(colmapPath)) {
        logCallback("ERROR: COLMAP not found");
        return false;
    }
    
    // Check for spaces in paths - COLMAP has issues with them
    if (framesDir.find(' ') != std::string::npos || outputDir.find(' ') != std::string::npos) {
        logCallback("");
        logCallback("⚠⚠⚠ WARNING: SPACES IN PATHS DETECTED ⚠⚠⚠");
        logCallback("COLMAP does not work reliably with spaces in file paths.");
        logCallback("Please use paths without spaces, for example:");
        logCallback("  Good: C:\\DroneOutput or C:\\Projects\\Output");
        logCallback("  Bad:  C:\\Drone videos or C:\\My Projects\\Output");
        logCallback("");
        logCallback("Processing will likely FAIL. Please change your paths and try again.");
        logCallback("");
    }
    
    logCallback("Using COLMAP: " + colmapPath.string());
    logCallback("Input frames: " + framesDir);
    logCallback("Output: " + outputDir);
    
    // Convert backslashes to forward slashes for COLMAP compatibility
    std::string fixedFramesDir = framesDir;
    std::string fixedOutputDir = outputDir;
    std::replace(fixedFramesDir.begin(), fixedFramesDir.end(), '\\', '/');
    std::replace(fixedOutputDir.begin(), fixedOutputDir.end(), '\\', '/');
    
    fs::path projectDir(outputDir);
    fs::path dbPath = projectDir / "database" / "database.db";
    fs::path sparseDir = projectDir / "sparse";
    fs::path imagesDir = projectDir / "images";
    
    try {
        fs::create_directories(dbPath.parent_path());
        fs::create_directories(sparseDir);
        fs::create_directories(imagesDir);
    }
    catch (const std::exception& e) {
        logCallback("ERROR creating COLMAP directories: " + std::string(e.what()));
        return false;
    }
    
    // Convert database and sparse paths to forward slashes
    std::string fixedDbPath = dbPath.string();
    std::string fixedSparseDir = sparseDir.string();
    std::replace(fixedDbPath.begin(), fixedDbPath.end(), '\\', '/');
    std::replace(fixedSparseDir.begin(), fixedSparseDir.end(), '\\', '/');
    
    // Step 1: Feature extraction
    logCallback("Step 1/4: Feature Extraction...");
    std::string cmd = "\"\"" + colmapPath.string() + "\" feature_extractor --database_path \"" + 
                     fixedDbPath + "\" --image_path \"" + fixedFramesDir + 
                     "\" --ImageReader.single_camera 1\"";
    logCallback("DEBUG: Full command: " + cmd);
    if (runCommand(cmd, logCallback) != 0) {
        logCallback("ERROR: Feature extraction failed");
        return false;
    }
    
    // Step 2: Feature matching
    logCallback("Step 2/4: Feature Matching...");
    cmd = "\"\"" + colmapPath.string() + "\" exhaustive_matcher --database_path \"" + 
          fixedDbPath + "\"\"";
    if (runCommand(cmd, logCallback) != 0) {
        logCallback("ERROR: Feature matching failed");
        return false;
    }
    
    // Step 3: Sparse reconstruction
    logCallback("Step 3/4: Sparse Reconstruction...");
    cmd = "\"\"" + colmapPath.string() + "\" mapper --database_path \"" + fixedDbPath + 
          "\" --image_path \"" + fixedFramesDir + "\" --output_path \"" + fixedSparseDir + "\"\"";
    if (runCommand(cmd, logCallback) != 0) {
        logCallback("ERROR: Sparse reconstruction failed");
        return false;
    }
    
    // Step 4: Image undistortion (outputs to images/ directory)
    logCallback("Step 4/4: Image Undistortion...");
    std::string fixedSparse0 = (sparseDir / "0").string();
    std::replace(fixedSparse0.begin(), fixedSparse0.end(), '\\', '/');
    cmd = "\"\"" + colmapPath.string() + "\" image_undistorter --image_path \"" + fixedFramesDir + 
          "\" --input_path \"" + fixedSparse0 + 
          "\" --output_path \"" + fixedOutputDir + "\" --output_type COLMAP\"";
    if (runCommand(cmd, logCallback) != 0) {
        logCallback("WARNING: Image undistortion failed, but sparse reconstruction succeeded");
    }
    
    logCallback("COLMAP reconstruction complete!");
    logCallback("Output structure:");
    logCallback("  " + outputDir + "/images/ - Undistorted images");
    logCallback("  " + outputDir + "/sparse/0/ - Camera poses and points");
    
    return true;
}

bool runMetashape(const std::string& framesDir, const std::string& outputDir, 
                 const std::string& metashapeExe, LogCallback logCallback) {
    if (metashapeExe.empty() || !fs::exists(metashapeExe)) {
        logCallback("ERROR: Metashape executable not found: " + metashapeExe);
        return false;
    }
    
    logCallback("Using Agisoft Metashape: " + metashapeExe);
    logCallback("Input frames: " + framesDir);
    logCallback("Output: " + outputDir);
    
    fs::path outputPath(outputDir);
    fs::path sparseDir = outputPath / "sparse" / "0";
    fs::path imagesDir = outputPath / "images";
    
    try {
        fs::create_directories(sparseDir);
        fs::create_directories(imagesDir);
    }
    catch (const std::exception& e) {
        logCallback("ERROR creating output directories: " + std::string(e.what()));
        return false;
    }
    
    // Create Metashape Python script that exports to COLMAP format
    fs::path scriptPath = outputPath / "metashape_process.py";
    std::ofstream script(scriptPath);
    
    if (!script.is_open()) {
        logCallback("ERROR: Could not create Metashape script");
        return false;
    }
    
    script << "import Metashape\n";
    script << "import sys\n";
    script << "from pathlib import Path\n\n";
    script << "try:\n";
    script << "    doc = Metashape.Document()\n";
    script << "    chunk = doc.addChunk()\n\n";
    script << "    image_folder = Path(r\"" << framesDir << "\")\n";
    script << "    image_files = [str(p) for p in image_folder.glob(\"*.jpg\")]\n";
    script << "    print(f\"Adding {len(image_files)} images...\")\n";
    script << "    if len(image_files) == 0:\n";
    script << "        raise RuntimeError(f\"No images found in {image_folder}\")\n";
    script << "    chunk.addPhotos(image_files)\n\n";
    script << "    print(\"Aligning photos...\")\n";
    script << "    chunk.matchPhotos(downscale=1, generic_preselection=True)\n";
    script << "    chunk.alignCameras()\n\n";
    script << "    # Check if alignment succeeded\n";
    script << "    aligned_cameras = sum(1 for camera in chunk.cameras if camera.transform)\n";
    script << "    print(f\"Aligned {aligned_cameras} cameras\")\n";
    script << "    if aligned_cameras == 0:\n";
    script << "        raise RuntimeError(\"Camera alignment failed - no cameras aligned\")\n\n";
    script << "    # Export to COLMAP format (native Metashape export)\n";
    script << "    print(\"Exporting to COLMAP format...\")\n";
    script << "    sparse_path = Path(r\"" << sparseDir.string() << "\")\n";
    script << "    try:\n";
    script << "        colmap_file = sparse_path / 'cameras.txt'\n";
    script << "        chunk.exportCameras(path=str(colmap_file), format=Metashape.CamerasFormatColmap)\n";
    script << "        print(\"  SUCCESS: Native COLMAP cameras export\")\n";
    script << "    except Exception as e:\n";
    script << "        print(f\"  ERROR: COLMAP export failed: {e}\")\n";
    script << "        raise\n\n";
    script << "    # Copy images to output\n";
    script << "    import shutil\n";
    script << "    images_out = Path(r\"" << imagesDir.string() << "\")\n";
    script << "    for img in image_files:\n";
    script << "        shutil.copy2(img, images_out / Path(img).name)\n";
    script << "    print(f\"Copied {len(image_files)} images to output\")\n\n";
    script << "    project_path = Path(r\"" << outputDir << "\") / \"metashape_project.psx\"\n";
    script << "    doc.save(str(project_path))\n";
    script << "    print(\"Metashape processing complete!\")\n";
    script << "except Exception as e:\n";
    script << "    print(f\"ERROR: {type(e).__name__}: {e}\", file=sys.stderr)\n";
    script << "    import traceback\n";
    script << "    traceback.print_exc()\n";
    script << "    sys.exit(1)\n";
    
    script.close();
    
    logCallback("Running Metashape (this may take a while)...");
    
    // Create log file path
    fs::path logPath = outputPath / "metashape_log.txt";
    
    // Redirect output to log file
    std::string cmd = "\"\"" + metashapeExe + "\" -r \"" + scriptPath.string() + "\" > \"" + 
                     logPath.string() + "\" 2>&1\"";
    
    if (runCommand(cmd, logCallback) != 0) {
        logCallback("ERROR: Metashape processing failed");
        logCallback("Check log file for details: " + logPath.string());
        
        // Try to read and log the last few lines of the log file
        std::ifstream logFile(logPath);
        if (logFile.is_open()) {
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(logFile, line)) {
                lines.push_back(line);
            }
            logFile.close();
            
            // Log last 10 lines
            size_t startIdx = lines.size() > 10 ? lines.size() - 10 : 0;
            logCallback("Last lines from Metashape log:");
            for (size_t i = startIdx; i < lines.size(); i++) {
                logCallback("  " + lines[i]);
            }
        }
        return false;
    }
    
    logCallback("Metashape reconstruction complete!");
    logCallback("Output structure:");
    logCallback("  " + imagesDir.string() + " - Images");
    logCallback("  " + sparseDir.string() + " - Camera poses (COLMAP format)");
    
    return true;
}

bool runRealityScan(const std::string& framesDir, const std::string& outputDir, 
                   const std::string& realityscanExe, LogCallback logCallback) {
    if (realityscanExe.empty() || !fs::exists(realityscanExe)) {
        logCallback("ERROR: RealityScan executable not found: " + realityscanExe);
        return false;
    }
    
    logCallback("Using RealityScan 2.0: " + realityscanExe);
    logCallback("Input frames: " + framesDir);
    logCallback("Output: " + outputDir);
    
    fs::path outputPath(outputDir);
    fs::path projectFile = outputPath / "realityscan_project.rsproj";
    fs::path undistortedDir = outputPath / "undistorted";
    fs::path sparseDir = undistortedDir / "sparse" / "0";
    fs::path imagesDir = undistortedDir / "images";
    fs::path registrationFile = undistortedDir / "sparse" / "registration.txt";
    fs::path pointsFile = sparseDir / "points3D.txt";
    
    try {
        fs::create_directories(sparseDir);
        fs::create_directories(imagesDir);
    }
    catch (const std::exception& e) {
        logCallback("ERROR creating output directories: " + std::string(e.what()));
        return false;
    }
    
    // Create empty points3D.txt file (RealityScan may not generate sparse points)
    std::ofstream pointsOut(pointsFile);
    if (pointsOut.is_open()) {
        pointsOut << "# 3D point list with one line of data per point:\n";
        pointsOut << "# POINT3D_ID, X, Y, Z, R, G, B, ERROR, TRACK[] as (IMAGE_ID, POINT2D_IDX)\n";
        pointsOut << "# Number of points: 0\n";
        pointsOut.close();
        logCallback("Created empty points3D.txt (RealityScan sparse export skipped)");
    }
    
    logCallback("Running RealityScan (this may take a while)...");
    
    // Build RealityScan CLI command with export parameters
    std::string cmd = "\"\"" + realityscanExe + "\"" +
                     " -headless" +
                     " -newScene" +
                     " -addFolder \"" + framesDir + "\"" +
                     " -set appIncSubdirs=false" +
                     " -align" +
                     " -selectMaximalComponent" +
                     " -exportRegistration \"" + registrationFile.string() + "\"" +
                     " -exportUndistortedImages \"" + imagesDir.string() + "\"" +
                     " -save \"" + projectFile.string() + "\"" +
                     " -quit\"";
    
    logCallback("Command: " + cmd);
    
    if (runCommand(cmd, logCallback) != 0) {
        logCallback("ERROR: RealityScan processing failed");
        return false;
    }
    
    logCallback("");
    logCallback("RealityScan processing complete!");
    logCallback("");
    logCallback("Checking exports...");
    
    // Verify expected outputs were created
    bool success = true;
    if (fs::exists(registrationFile)) {
        logCallback("✅ Registration exported successfully");
    } else {
        logCallback("❌ Warning: registration.txt was not created");
        logCallback("   Camera registration may have failed");
        success = false;
    }
    
    if (fs::exists(imagesDir) && !fs::is_empty(imagesDir)) {
        size_t imageCount = std::distance(fs::directory_iterator(imagesDir), fs::directory_iterator{});
        logCallback("✅ Exported " + std::to_string(imageCount) + " undistorted images");
    } else {
        logCallback("❌ Warning: No undistorted images were exported");
        success = false;
    }
    
    // Copy COLMAP sparse files into images folder for Gaussian splatting compatibility
    if (success) {
        logCallback("");
        logCallback("Copying COLMAP files to images folder for Gaussian splatting...");
        
        try {
            // Copy registration.txt to images folder
            if (fs::exists(registrationFile)) {
                fs::path destReg = imagesDir / "registration.txt";
                fs::copy_file(registrationFile, destReg, fs::copy_options::overwrite_existing);
                logCallback("  Copied registration.txt");
            }
            
            // Copy points3D.txt to images folder
            if (fs::exists(pointsFile)) {
                fs::path destPoints = imagesDir / "points3D.txt";
                fs::copy_file(pointsFile, destPoints, fs::copy_options::overwrite_existing);
                logCallback("  Copied points3D.txt");
            }
            
            // Copy any other COLMAP files (cameras.txt, images.txt) if they exist
            for (const auto& entry : fs::directory_iterator(undistortedDir / "sparse")) {
                if (entry.is_regular_file()) {
                    fs::path filename = entry.path().filename();
                    std::string ext = filename.extension().string();
                    if (ext == ".txt" || ext == ".bin") {
                        fs::path dest = imagesDir / filename;
                        fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                        logCallback("  Copied " + filename.string());
                    }
                }
            }
            
            // Also check sparse/0 folder
            for (const auto& entry : fs::directory_iterator(sparseDir)) {
                if (entry.is_regular_file()) {
                    fs::path filename = entry.path().filename();
                    std::string ext = filename.extension().string();
                    if (ext == ".txt" || ext == ".bin") {
                        fs::path dest = imagesDir / filename;
                        // Only copy if not already copied
                        if (!fs::exists(dest)) {
                            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                            logCallback("  Copied " + filename.string());
                        }
                    }
                }
            }
            
            logCallback("✅ COLMAP files copied to images folder");
        }
        catch (const std::exception& e) {
            logCallback("⚠ Warning: Failed to copy some COLMAP files: " + std::string(e.what()));
        }
    }
    
    logCallback("");
    logCallback("Output directory: " + undistortedDir.string());
    logCallback("  Images + COLMAP data: " + imagesDir.string());
    logCallback("  Sparse (original): " + sparseDir.string());
    
    if (!success) {
        logCallback("");
        logCallback("⚠ IMPORTANT: RealityScan exports need manual verification");
        logCallback("   - Check that registration.txt contains camera data");
        logCallback("   - Verify undistorted images were exported correctly");
    }
    
    return success;
}

bool runPipeline(const PipelineConfig& config, LogCallback logCallback) {
    logCallback("=======================================================");
    logCallback("   Drone Reconstruction Pipeline - GUI Edition");
    logCallback("=======================================================");
    
    // Check vendor files
    if (!checkVendorFiles(logCallback)) {
        return false;
    }
    
    // Validate inputs
    if (config.videoPath.empty()) {
        logCallback("ERROR: Video path is required");
        return false;
    }
    
    if (config.outputBaseDir.empty()) {
        logCallback("ERROR: Output directory is required");
        return false;
    }
    
    if (!fs::exists(config.videoPath)) {
        logCallback("ERROR: Video path not found: " + config.videoPath);
        return false;
    }
    
    // Create output directories
    fs::path outputBase(config.outputBaseDir);
    fs::path framesDir = outputBase / "frames";
    
    try {
        fs::create_directories(framesDir);
        fs::create_directories(outputBase);
    }
    catch (const std::exception& e) {
        logCallback("ERROR creating directories: " + std::string(e.what()));
        return false;
    }
    
    // Check if input is a file or folder
    fs::path inputPath(config.videoPath);
    bool isFolder = fs::is_directory(inputPath);
    
    logCallback("Configuration:");
    if (isFolder) {
        logCallback("  Video Folder: " + config.videoPath);
    } else {
        logCallback("  Video:       " + config.videoPath);
    }
    logCallback("  Output:      " + config.outputBaseDir);
    logCallback("  Frame Rate:  " + std::to_string(config.frameRate) + " fps");
    
    std::string methodName;
    switch (config.method) {
        case ReconMethod::COLMAP: methodName = "COLMAP"; break;
        case ReconMethod::METASHAPE: methodName = "Metashape"; break;
        case ReconMethod::REALITYSCAN: methodName = "RealityScan"; break;
    }
    logCallback("  Method:      " + methodName);
    logCallback("");
    
    // Step 1: Frame Extraction
    logCallback("=======================================================");
    logCallback("STEP 1: Frame Extraction");
    logCallback("=======================================================");
    
    std::vector<std::string> videoFiles;
    std::string outputFolderName;
    
    if (isFolder) {
        // Process all videos in folder
        logCallback("Scanning folder for video files...");
        std::vector<std::string> extensions = {".mp4", ".MP4", ".mov", ".MOV", ".avi", ".AVI"};
        
        for (const auto& entry : fs::directory_iterator(inputPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                for (const auto& validExt : extensions) {
                    if (ext == validExt) {
                        videoFiles.push_back(entry.path().string());
                        break;
                    }
                }
            }
        }
        
        if (videoFiles.empty()) {
            logCallback("ERROR: No video files found in folder");
            return false;
        }
        
        logCallback("Found " + std::to_string(videoFiles.size()) + " video file(s)");
        for (const auto& vf : videoFiles) {
            logCallback("  - " + fs::path(vf).filename().string());
        }
        
        outputFolderName = "combined";
    } else {
        // Single video file
        videoFiles.push_back(config.videoPath);
        outputFolderName = inputPath.stem().string();
    }
    
    logCallback("");
    
    // Extract frames from all videos
    fs::path combinedFramesDir = framesDir / outputFolderName;
    try {
        fs::create_directories(combinedFramesDir);
    } catch (const std::exception& e) {
        logCallback("ERROR creating combined frames directory: " + std::string(e.what()));
        return false;
    }
    
    int totalFrames = 0;
    for (size_t i = 0; i < videoFiles.size(); i++) {
        if (videoFiles.size() > 1) {
            logCallback("Processing video " + std::to_string(i + 1) + "/" + std::to_string(videoFiles.size()) + ": " + 
                       fs::path(videoFiles[i]).filename().string());
        }
        
        if (!extractFrames(videoFiles[i], framesDir.string(), config.frameRate, logCallback)) {
            logCallback("WARNING: Frame extraction failed for " + videoFiles[i]);
            continue;
        }
        
        // Move frames to combined folder (if processing multiple videos)
        if (videoFiles.size() > 1) {
            fs::path videoStem = fs::path(videoFiles[i]).stem();
            fs::path videoFramesDir = framesDir / videoStem;
            
            if (fs::exists(videoFramesDir)) {
                // Copy all frames to combined folder
                for (const auto& frameEntry : fs::directory_iterator(videoFramesDir)) {
                    if (frameEntry.path().extension() == ".jpg") {
                        fs::path destPath = combinedFramesDir / frameEntry.path().filename();
                        try {
                            fs::copy_file(frameEntry.path(), destPath, fs::copy_options::overwrite_existing);
                            totalFrames++;
                        } catch (const std::exception& e) {
                            logCallback("WARNING: Failed to copy frame: " + std::string(e.what()));
                        }
                    }
                }
                
                // Remove individual video frames directory
                try {
                    fs::remove_all(videoFramesDir);
                } catch (...) {}
            }
        } else {
            // Count frames for single video
            fs::path videoStem = fs::path(videoFiles[0]).stem();
            fs::path videoFramesDir = framesDir / videoStem;
            for (const auto& entry : fs::directory_iterator(videoFramesDir)) {
                if (entry.path().extension() == ".jpg") {
                    totalFrames++;
                }
            }
        }
    }
    
    logCallback("Frame extraction completed successfully");
    logCallback("Total frames extracted: " + std::to_string(totalFrames));
    logCallback("");
    
    // Get the actual frames directory
    std::string actualFramesDir = combinedFramesDir.string();
    
    // Step 2: 3D Reconstruction
    logCallback("=======================================================");
    logCallback("STEP 2: 3D Reconstruction");
    logCallback("=======================================================");
    
    // Check for spaces in paths when using COLMAP
    if (config.method == ReconMethod::COLMAP) {
        if (config.videoPath.find(' ') != std::string::npos || config.outputBaseDir.find(' ') != std::string::npos) {
            logCallback("");
            logCallback("⚠⚠⚠ WARNING: SPACES IN PATHS DETECTED ⚠⚠⚠");
            logCallback("COLMAP does not work reliably with spaces in file paths.");
            logCallback("");
            logCallback("Your paths:");
            logCallback("  Video: " + config.videoPath);
            logCallback("  Output: " + config.outputBaseDir);
            logCallback("");
            logCallback("Please use paths WITHOUT spaces, for example:");
            logCallback("  Good: C:\\DroneOutput or C:\\Projects\\Output");
            logCallback("  Bad:  C:\\Drone videos or C:\\My Projects\\Output");
            logCallback("");
            logCallback("Processing will likely FAIL.");
            logCallback("");
            
            // Show popup warning
            std::string message = "WARNING: Your paths contain SPACES!\n\n"
                                "COLMAP does not work with spaces in file paths.\n\n"
                                "Video: " + config.videoPath + "\n"
                                "Output: " + config.outputBaseDir + "\n\n"
                                "Please use paths WITHOUT spaces.\n\n"
                                "Processing will likely FAIL!\n\n"
                                "Click OK to continue anyway (not recommended).";
            MessageBoxA(NULL, message.c_str(), "COLMAP Path Warning", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        }
    }
    
    bool success = false;
    switch (config.method) {
        case ReconMethod::COLMAP:
            success = runColmap(actualFramesDir, config.outputBaseDir, logCallback);
            break;
        case ReconMethod::METASHAPE:
            success = runMetashape(actualFramesDir, config.outputBaseDir, 
                                  config.metashapeExePath, logCallback);
            break;
        case ReconMethod::REALITYSCAN:
            success = runRealityScan(actualFramesDir, config.outputBaseDir, 
                                    config.realityscanExePath, logCallback);
            break;
    }
    
    if (!success) {
        logCallback("ERROR: 3D reconstruction failed");
        return false;
    }
    
    logCallback("3D reconstruction completed successfully");
    logCallback("");
    
    logCallback("=======================================================");
    logCallback("Pipeline completed successfully!");
    logCallback("=======================================================");
    logCallback("Output directory: " + config.outputBaseDir);
    logCallback("Ready for Gaussian Splatting!");
    
    return true;
}
