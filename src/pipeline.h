#ifndef PIPELINE_H
#define PIPELINE_H

#include <string>
#include <functional>

// Callback for logging messages
using LogCallback = std::function<void(const std::string&)>;

// Reconstruction method options
enum class ReconMethod {
    COLMAP,
    METASHAPE,
    REALITYSCAN
};

// Pipeline configuration
struct PipelineConfig {
    std::string videoPath;
    std::string outputBaseDir;
    double frameRate;
    ReconMethod method;
    std::string metashapeExePath;
    std::string realityscanExePath;
};

// Main pipeline entry point
bool runPipeline(const PipelineConfig& config, LogCallback logCallback);

#endif // PIPELINE_H
