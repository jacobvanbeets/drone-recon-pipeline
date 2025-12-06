#include "gui.h"
#include "pipeline.h"
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <thread>
#include <shlobj.h>
#include <fstream>
#include <map>

// Control IDs
#define ID_VIDEO_PATH 1001
#define ID_VIDEO_BROWSE 1002
#define ID_OUTPUT_PATH 1003
#define ID_OUTPUT_BROWSE 1004
#define ID_FPS_EDIT 1005
#define ID_RADIO_COLMAP 1006
#define ID_RADIO_METASHAPE 1007
#define ID_RADIO_REALITYSCAN 1008
#define ID_METASHAPE_PATH 1009
#define ID_METASHAPE_BROWSE 1010
#define ID_REALITYSCAN_PATH 1011
#define ID_REALITYSCAN_BROWSE 1012
#define ID_START_BUTTON 1013
#define ID_LOG_TEXT 1014
#define ID_VIDEO_BROWSE_FOLDER 1015

// Global window handles
HWND g_hwndVideoPath = NULL;
HWND g_hwndOutputPath = NULL;
HWND g_hwndFps = NULL;
HWND g_hwndMetashapePath = NULL;
HWND g_hwndRealityScanPath = NULL;
HWND g_hwndLogText = NULL;
HWND g_hwndStartButton = NULL;
HWND g_hwndRadioColmap = NULL;
HWND g_hwndRadioMetashape = NULL;
HWND g_hwndRadioRealityScan = NULL;
HWND g_hwndMetashapeBrowse = NULL;
HWND g_hwndRealityScanBrowse = NULL;

bool g_processing = false;

// Forward declaration
void UpdateMethodControls();

std::string GetSettingsPath() {
    char appDataPath[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath) == S_OK) {
        std::string settingsDir = std::string(appDataPath) + "\\DroneRecon";
        CreateDirectoryA(settingsDir.c_str(), NULL);
        return settingsDir + "\\settings.ini";
    }
    return "settings.ini";
}

void SaveSettings() {
    std::string settingsPath = GetSettingsPath();
    std::ofstream file(settingsPath);
    if (!file.is_open()) return;
    
    char buffer[MAX_PATH];
    
    // Save video path
    GetWindowTextA(g_hwndVideoPath, buffer, MAX_PATH);
    if (buffer[0] != '\0') {
        file << "video_path=" << buffer << "\n";
    }
    
    // Save output path
    GetWindowTextA(g_hwndOutputPath, buffer, MAX_PATH);
    if (buffer[0] != '\0') {
        file << "output_path=" << buffer << "\n";
    }
    
    // Save FPS
    GetWindowTextA(g_hwndFps, buffer, 32);
    file << "fps=" << buffer << "\n";
    
    // Save method
    if (SendMessage(g_hwndRadioColmap, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        file << "method=colmap\n";
    } else if (SendMessage(g_hwndRadioMetashape, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        file << "method=metashape\n";
    } else {
        file << "method=realityscan\n";
    }
    
    // Save Metashape path
    GetWindowTextA(g_hwndMetashapePath, buffer, MAX_PATH);
    if (buffer[0] != '\0') {
        file << "metashape_path=" << buffer << "\n";
    }
    
    // Save RealityScan path
    GetWindowTextA(g_hwndRealityScanPath, buffer, MAX_PATH);
    if (buffer[0] != '\0') {
        file << "realityscan_path=" << buffer << "\n";
    }
    
    file.close();
}

void LoadSettings() {
    std::string settingsPath = GetSettingsPath();
    std::ifstream file(settingsPath);
    if (!file.is_open()) return;
    
    std::map<std::string, std::string> settings;
    std::string line;
    
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            settings[key] = value;
        }
    }
    file.close();
    
    // Load video path
    if (settings.count("video_path")) {
        SetWindowTextA(g_hwndVideoPath, settings["video_path"].c_str());
    }
    
    // Load output path
    if (settings.count("output_path")) {
        SetWindowTextA(g_hwndOutputPath, settings["output_path"].c_str());
    }
    
    // Load FPS
    if (settings.count("fps")) {
        SetWindowTextA(g_hwndFps, settings["fps"].c_str());
    }
    
    // Load method
    if (settings.count("method")) {
        // Uncheck all radio buttons first
        SendMessage(g_hwndRadioColmap, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hwndRadioMetashape, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hwndRadioRealityScan, BM_SETCHECK, BST_UNCHECKED, 0);
        
        // Check the saved method
        std::string method = settings["method"];
        if (method == "colmap") {
            SendMessage(g_hwndRadioColmap, BM_SETCHECK, BST_CHECKED, 0);
        } else if (method == "metashape") {
            SendMessage(g_hwndRadioMetashape, BM_SETCHECK, BST_CHECKED, 0);
        } else if (method == "realityscan") {
            SendMessage(g_hwndRadioRealityScan, BM_SETCHECK, BST_CHECKED, 0);
        }
    }
    
    // Load Metashape path
    if (settings.count("metashape_path")) {
        SetWindowTextA(g_hwndMetashapePath, settings["metashape_path"].c_str());
    }
    
    // Load RealityScan path
    if (settings.count("realityscan_path")) {
        SetWindowTextA(g_hwndRealityScanPath, settings["realityscan_path"].c_str());
    }
    
    UpdateMethodControls();
}

void AppendLog(const std::string& message) {
    if (g_hwndLogText == NULL) return;
    
    int len = GetWindowTextLengthA(g_hwndLogText);
    SendMessageA(g_hwndLogText, EM_SETSEL, len, len);
    SendMessageA(g_hwndLogText, EM_REPLACESEL, FALSE, (LPARAM)(message + "\r\n").c_str());
    SendMessageA(g_hwndLogText, EM_SCROLLCARET, 0, 0);
}

std::string BrowseForFile(HWND hwnd, const char* filter, const char* title) {
    char filename[MAX_PATH] = "";
    
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    return "";
}

std::string BrowseForFolder(HWND hwnd, const char* title) {
    char path[MAX_PATH] = "";
    
    BROWSEINFOA bi = {};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        SHGetPathFromIDListA(pidl, path);
        CoTaskMemFree(pidl);
        return std::string(path);
    }
    return "";
}

void UpdateMethodControls() {
    bool colmapSelected = (SendMessage(g_hwndRadioColmap, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool metashapeSelected = (SendMessage(g_hwndRadioMetashape, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool realityscanSelected = (SendMessage(g_hwndRadioRealityScan, BM_GETCHECK, 0, 0) == BST_CHECKED);
    
    EnableWindow(g_hwndMetashapePath, metashapeSelected);
    EnableWindow(g_hwndMetashapeBrowse, metashapeSelected);
    EnableWindow(g_hwndRealityScanPath, realityscanSelected);
    EnableWindow(g_hwndRealityScanBrowse, realityscanSelected);
}

void RunPipelineAsync(HWND hwnd) {
    // Get configuration from UI
    char videoPath[MAX_PATH];
    char outputPath[MAX_PATH];
    char fpsText[32];
    char metashapePath[MAX_PATH];
    char realityscanPath[MAX_PATH];
    
    GetWindowTextA(g_hwndVideoPath, videoPath, MAX_PATH);
    GetWindowTextA(g_hwndOutputPath, outputPath, MAX_PATH);
    GetWindowTextA(g_hwndFps, fpsText, 32);
    GetWindowTextA(g_hwndMetashapePath, metashapePath, MAX_PATH);
    GetWindowTextA(g_hwndRealityScanPath, realityscanPath, MAX_PATH);
    
    PipelineConfig config;
    config.videoPath = videoPath;
    config.outputBaseDir = outputPath;
    config.frameRate = atof(fpsText);
    config.metashapeExePath = metashapePath;
    config.realityscanExePath = realityscanPath;
    
    // Determine method
    if (SendMessage(g_hwndRadioColmap, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        config.method = ReconMethod::COLMAP;
    } else if (SendMessage(g_hwndRadioMetashape, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        config.method = ReconMethod::METASHAPE;
    } else {
        config.method = ReconMethod::REALITYSCAN;
    }
    
    // Disable controls
    EnableWindow(g_hwndStartButton, FALSE);
    SetWindowTextA(g_hwndStartButton, "Processing...");
    
    // Clear log
    SetWindowTextA(g_hwndLogText, "");
    
    // Run pipeline in background thread
    std::thread([hwnd, config]() {
        bool success = runPipeline(config, AppendLog);
        
        // Re-enable controls on UI thread
        PostMessage(hwnd, WM_USER + 1, success ? 1 : 0, 0);
    }).detach();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Title
            CreateWindowA("STATIC", "Drone Reconstruction Pipeline - GUI Edition",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 10, 760, 30, hwnd, NULL, NULL, NULL);
            
            // Video input section
            CreateWindowA("STATIC", "Video File/Folder:",
                WS_CHILD | WS_VISIBLE,
                10, 50, 100, 20, hwnd, NULL, NULL, NULL);
            g_hwndVideoPath = CreateWindowA("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                120, 50, 460, 25, hwnd, (HMENU)ID_VIDEO_PATH, NULL, NULL);
            CreateWindowA("BUTTON", "File",
                WS_CHILD | WS_VISIBLE,
                590, 50, 60, 25, hwnd, (HMENU)ID_VIDEO_BROWSE, NULL, NULL);
            CreateWindowA("BUTTON", "Folder",
                WS_CHILD | WS_VISIBLE,
                660, 50, 110, 25, hwnd, (HMENU)ID_VIDEO_BROWSE_FOLDER, NULL, NULL);
            
            // Output directory section
            CreateWindowA("STATIC", "Output Directory:",
                WS_CHILD | WS_VISIBLE,
                10, 85, 100, 20, hwnd, NULL, NULL, NULL);
            g_hwndOutputPath = CreateWindowA("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                120, 85, 520, 25, hwnd, (HMENU)ID_OUTPUT_PATH, NULL, NULL);
            CreateWindowA("BUTTON", "Browse...",
                WS_CHILD | WS_VISIBLE,
                650, 85, 120, 25, hwnd, (HMENU)ID_OUTPUT_BROWSE, NULL, NULL);
            
            // FPS section
            CreateWindowA("STATIC", "Frame Rate (fps):",
                WS_CHILD | WS_VISIBLE,
                10, 120, 100, 20, hwnd, NULL, NULL, NULL);
            g_hwndFps = CreateWindowA("EDIT", "1.0",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                120, 120, 100, 25, hwnd, (HMENU)ID_FPS_EDIT, NULL, NULL);
            
            // Reconstruction method section
            CreateWindowA("STATIC", "Reconstruction Method:",
                WS_CHILD | WS_VISIBLE,
                10, 160, 150, 20, hwnd, NULL, NULL, NULL);
            
            g_hwndRadioColmap = CreateWindowA("BUTTON", "COLMAP (bundled)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                10, 185, 200, 25, hwnd, (HMENU)ID_RADIO_COLMAP, NULL, NULL);
            g_hwndRadioMetashape = CreateWindowA("BUTTON", "Agisoft Metashape",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                10, 215, 200, 25, hwnd, (HMENU)ID_RADIO_METASHAPE, NULL, NULL);
            g_hwndRadioRealityScan = CreateWindowA("BUTTON", "RealityScan 2.0",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                10, 245, 200, 25, hwnd, (HMENU)ID_RADIO_REALITYSCAN, NULL, NULL);
            
            // Default to COLMAP
            SendMessage(g_hwndRadioColmap, BM_SETCHECK, BST_CHECKED, 0);
            
            // Metashape path
            CreateWindowA("STATIC", "Metashape Path:",
                WS_CHILD | WS_VISIBLE,
                220, 215, 100, 20, hwnd, NULL, NULL, NULL);
            g_hwndMetashapePath = CreateWindowA("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_DISABLED,
                330, 215, 310, 25, hwnd, (HMENU)ID_METASHAPE_PATH, NULL, NULL);
            g_hwndMetashapeBrowse = CreateWindowA("BUTTON", "Browse...",
                WS_CHILD | WS_VISIBLE | WS_DISABLED,
                650, 215, 120, 25, hwnd, (HMENU)ID_METASHAPE_BROWSE, NULL, NULL);
            
            // RealityScan path
            CreateWindowA("STATIC", "RealityScan Path:",
                WS_CHILD | WS_VISIBLE,
                220, 245, 100, 20, hwnd, NULL, NULL, NULL);
            g_hwndRealityScanPath = CreateWindowA("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_DISABLED,
                330, 245, 310, 25, hwnd, (HMENU)ID_REALITYSCAN_PATH, NULL, NULL);
            g_hwndRealityScanBrowse = CreateWindowA("BUTTON", "Browse...",
                WS_CHILD | WS_VISIBLE | WS_DISABLED,
                650, 245, 120, 25, hwnd, (HMENU)ID_REALITYSCAN_BROWSE, NULL, NULL);
            
            // Start button
            g_hwndStartButton = CreateWindowA("BUTTON", "Start Processing",
                WS_CHILD | WS_VISIBLE,
                300, 285, 180, 35, hwnd, (HMENU)ID_START_BUTTON, NULL, NULL);
            
            // Log section
            CreateWindowA("STATIC", "Progress Log:",
                WS_CHILD | WS_VISIBLE,
                10, 330, 100, 20, hwnd, NULL, NULL, NULL);
            g_hwndLogText = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 355, 760, 235, hwnd, (HMENU)ID_LOG_TEXT, NULL, NULL);
            
            // Set font for all controls
            HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            
            EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
                SendMessage(child, WM_SETFONT, (WPARAM)lParam, TRUE);
                return TRUE;
            }, (LPARAM)hFont);
            
            // Load saved settings
            LoadSettings();
            
            AppendLog("Ready. Configure settings and click 'Start Processing'.");
            AppendLog("FFmpeg and COLMAP are bundled in the vendor/ directory.");
            
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_VIDEO_BROWSE: {
                    std::string path = BrowseForFile(hwnd,
                        "Video Files (*.mp4;*.mov;*.avi)\0*.mp4;*.mov;*.avi\0All Files (*.*)\0*.*\0",
                        "Select Video File");
                    if (!path.empty()) {
                        SetWindowTextA(g_hwndVideoPath, path.c_str());
                        SaveSettings();
                    }
                    return 0;
                }
                
                case ID_VIDEO_BROWSE_FOLDER: {
                    std::string path = BrowseForFolder(hwnd, "Select Folder Containing Videos");
                    if (!path.empty()) {
                        SetWindowTextA(g_hwndVideoPath, path.c_str());
                        SaveSettings();
                    }
                    return 0;
                }
                
                case ID_OUTPUT_BROWSE: {
                    std::string path = BrowseForFolder(hwnd, "Select Output Directory");
                    if (!path.empty()) {
                        SetWindowTextA(g_hwndOutputPath, path.c_str());
                        SaveSettings();
                    }
                    return 0;
                }
                
                case ID_METASHAPE_BROWSE: {
                    std::string path = BrowseForFile(hwnd,
                        "Metashape Executable (metashape.exe)\0metashape.exe\0All Files (*.*)\0*.*\0",
                        "Select Metashape Executable");
                    if (!path.empty()) {
                        SetWindowTextA(g_hwndMetashapePath, path.c_str());
                        SaveSettings();
                    }
                    return 0;
                }
                
                case ID_REALITYSCAN_BROWSE: {
                    std::string path = BrowseForFile(hwnd,
                        "RealityScan Executable (RealityScan.exe)\0RealityScan.exe\0All Files (*.*)\0*.*\0",
                        "Select RealityScan Executable");
                    if (!path.empty()) {
                        SetWindowTextA(g_hwndRealityScanPath, path.c_str());
                        SaveSettings();
                    }
                    return 0;
                }
                
                case ID_RADIO_COLMAP:
                case ID_RADIO_METASHAPE:
                case ID_RADIO_REALITYSCAN:
                    UpdateMethodControls();
                    SaveSettings();
                    return 0;
                
                case ID_START_BUTTON:
                    if (!g_processing) {
                        g_processing = true;
                        RunPipelineAsync(hwnd);
                    }
                    return 0;
            }
            break;
        }
        
        case WM_USER + 1: {
            // Pipeline completed
            g_processing = false;
            EnableWindow(g_hwndStartButton, TRUE);
            SetWindowTextA(g_hwndStartButton, "Start Processing");
            
            if (wParam == 1) {
                AppendLog("==============================================");
                AppendLog("SUCCESS! Pipeline completed.");
                MessageBoxA(hwnd, "Pipeline completed successfully!\nOutput is ready for Gaussian Splatting.",
                    "Success", MB_OK | MB_ICONINFORMATION);
            } else {
                AppendLog("==============================================");
                AppendLog("ERROR: Pipeline failed. Check the log above.");
                MessageBoxA(hwnd, "Pipeline failed. Please check the log for details.",
                    "Error", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        
        case WM_CLOSE:
            if (g_processing) {
                int result = MessageBoxA(hwnd,
                    "Processing is in progress. Are you sure you want to exit?",
                    "Confirm Exit", MB_YESNO | MB_ICONWARNING);
                if (result == IDNO) {
                    return 0;
                }
            }
            SaveSettings();
            DestroyWindow(hwnd);
            return 0;
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int runGUI(HINSTANCE hInstance) {
    const char* CLASS_NAME = "DroneReconWindowClass";
    
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClassA(&wc);
    
    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "Drone Reconstruction Pipeline",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 650,
        NULL, NULL, hInstance, NULL
    );
    
    if (hwnd == NULL) {
        return 1;
    }
    
    ShowWindow(hwnd, SW_SHOW);
    
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
