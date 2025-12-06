# Drone Reconstruction Pipeline - Source Code

Version: 1.0.0  
Build Date: December 6, 2025  
Language: C++ (C++17)  
Platform: Windows x64

## Overview

This is a Windows desktop application that processes drone video footage into 3D reconstruction-ready datasets. It extracts frames, embeds GPS data from SRT files, and prepares data for Gaussian Splatting workflows.

## Project Structure

```
DroneRecon-Source-v1.0/
├── src/
│   ├── main.cpp           - Application entry point
│   ├── gui.cpp            - Win32 GUI implementation
│   ├── pipeline.cpp       - Core processing logic
│   ├── gps_embed.h        - GPS parsing and EXIF embedding
│   └── pipeline.h         - Pipeline header/config
├── vendor/
│   ├── ffmpeg/            - Video frame extraction
│   ├── colmap/            - 3D reconstruction
│   └── exiftool/          - GPS EXIF embedding
├── CMakeLists.txt         - CMake build configuration
└── BUILD.md               - This file
```

## Requirements

### Development Tools
- **Visual Studio 2019 or 2022** (with C++ Desktop Development workload)
- **CMake 3.15+** (included with Visual Studio or standalone)
- **Windows SDK 10.0+**

### Runtime Dependencies (Bundled in vendor/)
- FFmpeg (video processing)
- COLMAP (3D reconstruction)
- ExifTool 13.43 (GPS embedding)

## Building from Source

### Method 1: Using Visual Studio (Recommended)

1. **Open in Visual Studio**
   ```
   File > Open > CMake...
   Select: CMakeLists.txt
   ```

2. **Configure Build**
   - Visual Studio will auto-configure CMake
   - Select configuration: `x64-Release` or `x64-Debug`
   - Wait for CMake generation to complete

3. **Build**
   - Build > Build All
   - Or press: `Ctrl+Shift+B`

4. **Output Location**
   ```
   out/build/x64-Release/DroneRecon.exe
   ```

### Method 2: Using CMake Command Line

1. **Open Developer Command Prompt**
   ```
   Start Menu > Visual Studio 2022 > Developer Command Prompt
   ```

2. **Create Build Directory**
   ```cmd
   cd C:\Users\jacob\Projects\DroneRecon-Source-v1.0
   mkdir build
   cd build
   ```

3. **Configure with CMake**
   ```cmd
   cmake .. -G "Visual Studio 17 2022" -A x64
   ```

4. **Build**
   ```cmd
   cmake --build . --config Release
   ```

5. **Output Location**
   ```
   build/Release/DroneRecon.exe
   ```

## Creating Distribution Package

After building, create the distribution folder:

1. **Copy Executable**
   ```powershell
   Copy-Item build\Release\DroneRecon.exe Distribution\
   ```

2. **Copy Vendor Dependencies**
   ```powershell
   Copy-Item vendor Distribution\vendor -Recurse
   ```

3. **Test the Package**
   - Navigate to `Distribution/`
   - Run `DroneRecon.exe`
   - Verify vendor files are detected

## Code Structure

### main.cpp
- Application entry point
- Windows subsystem initialization
- WinMain entry

### gui.cpp
- Win32 API GUI implementation
- Window creation and message handling
- Settings persistence (load/save)
- Browse dialogs (file/folder)
- Control event handlers
- Threading for background processing

### pipeline.cpp
- Core processing logic
- Frame extraction with FFmpeg
- GPS embedding from SRT files
- COLMAP reconstruction
- Metashape integration
- RealityScan integration
- Command execution with hidden consoles
- Progress logging

### gps_embed.h
- SRT file parsing
- GPS data extraction
- DMS (degrees/minutes/seconds) conversion
- ExifTool command generation
- EXIF/XMP GPS tag embedding

### pipeline.h
- Configuration structures
- Enum definitions (ReconMethod)
- Function declarations
- LogCallback type definition

## Key Features Implementation

### 1. GPS Embedding (gps_embed.h)
- Parses DJI SRT files for GPS coordinates
- Converts decimal degrees to DMS format
- Writes both EXIF and XMP GPS tags for compatibility
- Automatically skips if SRT file not found

### 2. Hidden Console Execution (pipeline.cpp)
- Uses Windows CreateProcess API
- Pipes stdout/stderr to GUI log
- No console window popups
- Wraps commands in `cmd.exe /c` for proper execution

### 3. Settings Persistence (gui.cpp)
- Saves to `%APPDATA%\DroneRecon\settings.ini`
- Stores paths, frame rate, method selection
- Auto-loads on startup
- Independent per-user settings

### 4. Multi-Video Processing (pipeline.cpp)
- Scans folders for video files
- Processes each video independently
- Combines frames into single output
- Individual GPS embedding per video

### 5. RealityScan COLMAP Export (pipeline.cpp)
- Exports registration data and undistorted images
- Copies COLMAP files to images folder
- Gaussian Splatting compatibility

## Dependencies

### Compiler Requirements
- C++17 standard library
- Windows SDK (for Win32 API)

### External Libraries (Header-only)
- Standard C++ library (filesystem, fstream, sstream, etc.)
- Windows API (windows.h, commctrl.h, commdlg.h, shlobj.h)

### Bundled Tools (Runtime)
- FFmpeg (dynamically invoked)
- COLMAP (dynamically invoked)
- ExifTool (dynamically invoked)

## Compilation Flags

From CMakeLists.txt:
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(/W3 /EHsc)
add_executable(DroneRecon WIN32 src/main.cpp src/gui.cpp src/pipeline.cpp)
target_link_libraries(DroneRecon comctl32 comdlg32 shell32 ole32)
```

## Troubleshooting Build Issues

### Issue: "Cannot find CMakeLists.txt"
**Solution:** Ensure you're in the correct directory with CMakeLists.txt

### Issue: "MSVC not found"
**Solution:** Install Visual Studio with C++ Desktop Development workload

### Issue: Build fails with linker errors
**Solution:** 
- Clean build directory: `cmake --build . --target clean`
- Rebuild: `cmake --build . --config Release`

### Issue: Executable runs but can't find vendor files
**Solution:** 
- Copy vendor/ folder to same directory as .exe
- Or run from build directory: `build/Release/DroneRecon.exe`

## Testing Changes

After making code changes:

1. **Rebuild**
   ```cmd
   cmake --build . --config Release
   ```

2. **Test Locally**
   - Copy vendor/ to build/Release/ if not present
   - Run build/Release/DroneRecon.exe

3. **Test Distribution**
   - Copy new .exe to distribution folder
   - Test with fresh settings (delete %APPDATA%\DroneRecon)

## Version History

### v1.0.0 (December 6, 2025)
- Initial C++ implementation
- Win32 GUI with settings persistence
- Frame extraction with GPS embedding
- Three reconstruction methods (COLMAP, Metashape, RealityScan)
- Hidden console execution
- Multi-video batch processing
- Gaussian Splatting compatibility

## Contributing

When making changes:
1. Keep vendor files separate from source code
2. Update BUILD.md if build process changes
3. Test all three reconstruction methods
4. Verify settings persistence works
5. Check GPS embedding with/without SRT files
6. Test single video and multi-video processing

## License

This application uses the following open-source tools:
- FFmpeg - LGPL/GPL License
- COLMAP - BSD License
- ExifTool - Perl Artistic License

Source code for bundled tools available at their respective project pages.

## Support

For build issues or questions:
- Review this BUILD.md
- Check CMakeLists.txt for configuration
- Verify Visual Studio installation
- Ensure Windows SDK is installed
- Check that vendor/ folder is complete

---

**Ready to Build!** Follow the instructions above to compile from source.
