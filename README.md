# Drone Reconstruction Pipeline

A Windows desktop application for processing drone video footage into 3D reconstruction-ready datasets with automatic GPS embedding support.

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![Platform](https://img.shields.io/badge/platform-Windows-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)

If you find this useful consider donating to my coffee fund https://paypal.me/creature259

## 🎯 Overview

This application streamlines the process of preparing drone footage for 3D reconstruction and Gaussian Splatting workflows. It extracts frames from videos, automatically embeds GPS data from DJI SRT files, and processes them using industry-standard reconstruction tools.

<img width="545" height="442" alt="Screenshot 2025-12-06 170253" src="https://github.com/user-attachments/assets/1e2cdb42-15a6-4fde-aaab-b35d021395d7" />

## ✨ Features

- **🎬 Frame Extraction** - Extract frames at configurable frame rates from drone videos
- **📍 GPS Embedding** - Automatic GPS EXIF embedding from DJI SRT files (DMS format)
- **📁 Batch Processing** - Process multiple videos in a single run
- **🔧 Three Reconstruction Methods**:
  - **COLMAP** (bundled, GPU-accelerated, no installation required)
  - **Agisoft Metashape** (requires license)
  - **RealityScan 2.0** (free from Epic Games)
- **💾 Settings Persistence** - Remembers your paths and preferences
- **🚫 No Console Popups** - Clean execution with hidden console windows
- **📊 Real-time Progress** - Live logging of all operations
- **🎨 Gaussian Splatting Ready** - COLMAP files automatically placed for compatibility

## 📸 Screenshots

<details>
<summary>Click to view application interface</summary>

*Main application window with video selection, output options, and real-time progress logging*

</details>

## 🚀 Quick Start

### For End Users (No Compilation Required)

1. **Download the latest release** from the [Releases](../../releases) page
2. Extract the ZIP file to any location
3. Run `DroneRecon.exe`
4. Select your drone video, output folder, and click "Start Processing"

That's it! No installation, no Python, no dependencies to manage.

### For Developers

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/drone-recon-pipeline.git
cd drone-recon-pipeline

# Build with Visual Studio
# File > Open > CMake... > Select CMakeLists.txt
# Build > Build All

# Or build from command line
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

See [BUILD.md](BUILD.md) for detailed build instructions.

## 📦 What's Included

### Source Code Structure
```
src/
├── main.cpp        - Application entry point
├── gui.cpp         - Win32 GUI implementation
├── gui.h           - GUI header
├── pipeline.cpp    - Core processing logic
├── pipeline.h      - Pipeline configuration
└── gps_embed.h     - GPS parsing & EXIF embedding
```

### Bundled Dependencies (Binary Release Only)
- **FFmpeg** - Video frame extraction
- **COLMAP** - 3D reconstruction (GPU-accelerated)
- **ExifTool** - GPS EXIF metadata embedding

## 🎬 Supported Input Formats

- **Videos**: MP4, MOV, AVI
- **GPS Data**: DJI SRT files (placed next to video with same name)

## 📤 Output Structure

```
output/
├── frames/
│   └── [video_name]/      # Extracted frames with GPS EXIF
└── [method]/
    └── undistorted/
        ├── images/         # Final images + COLMAP data
        └── sparse/         # Camera registration
```

## 🔧 System Requirements

- **OS**: Windows 10/11 (64-bit)
- **RAM**: 8GB minimum, 16GB+ recommended
- **GPU**: NVIDIA GPU recommended for COLMAP
- **Storage**: ~600MB for application + working space for processing

## 📖 Usage

### Basic Workflow

1. **Select Input**
   - Click "File" for a single video
   - Click "Folder" to process multiple videos

2. **Choose Output Directory**
   - Select where processed frames and reconstruction will be saved

3. **Set Frame Rate**
   - `1.0 fps` (recommended) - 1 frame per second
   - `0.5 fps` - Fewer frames, faster processing
   - `2.0 fps` - More frames, better quality

4. **Select Method**
   - **COLMAP** - Built-in, works immediately
   - **Metashape** - Professional tool (requires license & installation)
   - **RealityScan** - Free tool (requires download from Epic Games)

5. **Start Processing**
   - Monitor progress in real-time log
   - Output ready for Gaussian Splatting when complete

### GPS Embedding

The application automatically embeds GPS data if an SRT file exists next to your video:

```
✅ my_drone_video.mp4
✅ my_drone_video.srt  ← GPS data file

Result: GPS coordinates embedded in all extracted frames
```

If no SRT file exists, frames are still extracted without GPS data (processing continues normally).

## 🏗️ Building from Source

### Prerequisites

- **Visual Studio 2019/2022** with C++ Desktop Development workload
- **CMake 3.15+** (included with Visual Studio)
- **Windows SDK 10.0+**

### Build Steps

```bash
# Configure
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64

# Compile
cmake --build . --config Release

# Output: build/Release/DroneRecon.exe
```

**Note**: The source repository does NOT include the vendor folder (FFmpeg, COLMAP, ExifTool). Download these separately or use the binary release which includes everything.

See [BUILD.md](BUILD.md) for:
- Detailed build instructions
- Troubleshooting common issues
- Creating distribution packages
- Development guidelines

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

1. **Report Bugs** - Open an issue with details about the problem
2. **Suggest Features** - Share your ideas for improvements
3. **Submit Pull Requests** - Fix bugs or add features
4. **Improve Documentation** - Help make the docs clearer

When contributing code:
- Maintain C++17 compatibility
- Follow existing code style
- Test all three reconstruction methods
- Update documentation as needed

## 📝 License

This project is released under the MIT License - see [LICENSE](LICENSE) for details.

### Bundled Dependencies (Binary Release)

The binary distribution includes these open-source tools:
- [FFmpeg](https://ffmpeg.org) - LGPL/GPL License
- [COLMAP](https://colmap.github.io) - BSD License
- [ExifTool](https://exiftool.org) - Perl Artistic License

Source code for these tools is available at their respective project pages.

## 🙏 Acknowledgments

- **FFmpeg** team for video processing capabilities
- **COLMAP** developers for 3D reconstruction
- **Phil Harvey** for ExifTool
- DJI for SRT GPS format documentation

## 📚 Documentation

- [BUILD.md](BUILD.md) - Build instructions and development guide
- [CHANGELOG.md](CHANGELOG.md) - Version history and changes
- User documentation included in binary releases

## 🐛 Known Issues

- Windows only (no Mac/Linux support in v1.0)
- Large videos (>10 minutes) may require significant processing time
- GPU required for optimal COLMAP performance

## 🗺️ Roadmap

Potential future enhancements:
- [ ] Linux/Mac support
- [ ] Drag & drop interface
- [ ] Progress bar for operations
- [ ] Frame preview before processing
- [ ] Command-line interface option
- [ ] Additional reconstruction method support

## 💬 Support

- **Issues**: [GitHub Issues](../../issues)
- **Discussions**: [GitHub Discussions](../../discussions)
- **Documentation**: See README.txt in binary release

## 🌟 Star History

If you find this project useful, please consider giving it a star! ⭐

---

**Made with ❤️ for the drone photogrammetry community**

*Convert your drone footage into stunning 3D reconstructions!*
