# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-12-06

### Added
- Initial release of Drone Reconstruction Pipeline
- Frame extraction from drone videos (MP4, MOV, AVI) at configurable frame rates
- Automatic GPS EXIF embedding from DJI SRT files
- GPS coordinate conversion to DMS (degrees/minutes/seconds) format for maximum compatibility
- Multi-video batch processing support
- Three reconstruction methods:
  - COLMAP (bundled, GPU-accelerated, no installation required)
  - Agisoft Metashape integration (requires separate license)
  - RealityScan 2.0 integration (free tool from Epic Games)
- Win32 native GUI with intuitive controls
- Settings persistence across sessions (saved to %APPDATA%\DroneRecon\settings.ini)
- Real-time progress logging with hidden console execution
- Automatic COLMAP file placement for Gaussian Splatting compatibility
- Smart SRT file detection (skips GPS embedding if not present)
- Video file/folder selection options
- Portable distribution with all dependencies bundled

### Features
- No Python runtime required - fully compiled C++ application
- No installation needed - portable executable
- Minimal dependencies - uses only Windows SDK and bundled tools
- Settings memory - remembers paths, frame rates, and method preferences
- Error handling and recovery for robust processing
- Multi-threaded background processing to keep UI responsive

### Technical
- Written in C++17 with Win32 API for GUI
- CMake build system for easy compilation
- Hidden console execution using Windows CreateProcess API
- Proper GPS EXIF format (both EXIF and XMP tags)
- DMS coordinate format for broad tool compatibility
- Comprehensive error logging and user feedback

### Known Limitations
- Windows only (no Mac/Linux support in v1.0)
- Metashape and RealityScan require separate installations
- Large videos (>10 minutes) may require significant processing time
- GPU strongly recommended for COLMAP performance

### Bundled Dependencies
- FFmpeg (latest) - Video processing
- COLMAP 3.x - 3D reconstruction
- ExifTool 13.43 - GPS EXIF embedding

## [Unreleased]

### Planned Features
- Linux and macOS support
- Drag & drop video file interface
- Visual progress bar for operations
- Frame preview before processing
- Batch settings profiles
- Command-line interface option
- Additional reconstruction method integrations

---

**Note**: This is the first public release. Future versions will focus on cross-platform support and enhanced features based on user feedback.
