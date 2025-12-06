#pragma once
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

struct GPSData {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double timestamp = 0.0;
    bool valid = false;
};

// Parse DJI SRT file and extract GPS data
std::vector<GPSData> parseSRT(const std::string& srtPath) {
    std::vector<GPSData> frames;
    
    std::ifstream file(srtPath);
    if (!file.is_open()) {
        return frames;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Split by double newlines to get blocks
    std::regex blockRegex("\\n\\n+");
    std::sregex_token_iterator iter(content.begin(), content.end(), blockRegex, -1);
    std::sregex_token_iterator end;
    
    std::regex timeRegex("(\\d{2}):(\\d{2}):(\\d{2}),(\\d{3})");
    std::regex gpsRegex("GPS:\\s*\\(([\\-\\d\\.]+)\\s*,\\s*([\\-\\d\\.]+)\\)");
    std::regex latRegex("\\[latitude:\\s*([\\-\\d\\.]+)\\]", std::regex::icase);
    std::regex lonRegex("\\[longtitude:\\s*([\\-\\d\\.]+)\\]", std::regex::icase);
    std::regex altRegex1("H:\\s*([\\-\\d\\.]+)m");
    std::regex altRegex2("\\[altitude:\\s*([\\-\\d\\.]+)\\]", std::regex::icase);
    
    for (; iter != end; ++iter) {
        std::string block = iter->str();
        std::istringstream iss(block);
        std::string line;
        std::vector<std::string> lines;
        
        while (std::getline(iss, line)) {
            if (!line.empty() && line != "\\r") {
                lines.push_back(line);
            }
        }
        
        if (lines.size() < 3) continue;
        
        // Parse timestamp
        std::smatch timeMatch;
        if (!std::regex_search(lines[1], timeMatch, timeRegex)) continue;
        
        int hours = std::stoi(timeMatch[1]);
        int minutes = std::stoi(timeMatch[2]);
        int seconds = std::stoi(timeMatch[3]);
        int ms = std::stoi(timeMatch[4]);
        double timestamp = hours * 3600 + minutes * 60 + seconds + ms / 1000.0;
        
        // Join metadata lines
        std::string metadata;
        for (size_t i = 2; i < lines.size(); i++) {
            metadata += lines[i] + " ";
        }
        
        GPSData data;
        data.timestamp = timestamp;
        
        // Try to parse GPS coordinates
        std::smatch gpsMatch;
        if (std::regex_search(metadata, gpsMatch, gpsRegex)) {
            data.longitude = std::stod(gpsMatch[1]);
            data.latitude = std::stod(gpsMatch[2]);
            data.valid = true;
        } else {
            // Try alternative format
            std::smatch latMatch, lonMatch;
            if (std::regex_search(metadata, latMatch, latRegex)) {
                data.latitude = std::stod(latMatch[1]);
            }
            if (std::regex_search(metadata, lonMatch, lonRegex)) {
                data.longitude = std::stod(lonMatch[1]);
            }
            if (latMatch.size() > 0 && lonMatch.size() > 0) {
                data.valid = true;
            }
        }
        
        // Parse altitude
        std::smatch altMatch;
        if (std::regex_search(metadata, altMatch, altRegex1)) {
            data.altitude = std::stod(altMatch[1]);
        } else if (std::regex_search(metadata, altMatch, altRegex2)) {
            data.altitude = std::stod(altMatch[1]);
        }
        
        if (data.valid) {
            frames.push_back(data);
        }
    }
    
    return frames;
}

// Find closest GPS data for a given timestamp
GPSData getGPSForTimestamp(const std::vector<GPSData>& frames, double timestamp) {
    if (frames.empty()) {
        return GPSData();
    }
    
    double minDiff = std::abs(frames[0].timestamp - timestamp);
    size_t closestIdx = 0;
    
    for (size_t i = 1; i < frames.size(); i++) {
        double diff = std::abs(frames[i].timestamp - timestamp);
        if (diff < minDiff) {
            minDiff = diff;
            closestIdx = i;
        }
    }
    
    return frames[closestIdx];
}

// Convert decimal degrees to DMS string for exiftool
std::string decimalToDMS(double decimal) {
    double absDec = std::abs(decimal);
    int degrees = static_cast<int>(absDec);
    double minutesDecimal = (absDec - degrees) * 60.0;
    int minutes = static_cast<int>(minutesDecimal);
    double seconds = (minutesDecimal - minutes) * 60.0;
    
    std::ostringstream oss;
    oss << degrees << " " << minutes << " " << std::fixed << std::setprecision(4) << seconds;
    return oss.str();
}

// Generate exiftool command to embed GPS data into both EXIF and XMP for maximum compatibility
std::string generateExiftoolCommand(const std::string& exiftoolPath, const std::string& imagePath, 
                                   double latitude, double longitude, double altitude) {
    std::ostringstream cmd;
    cmd << "\"\"" << exiftoolPath << "\" ";
    
    const char latRef = latitude >= 0 ? 'N' : 'S';
    const char lonRef = longitude >= 0 ? 'E' : 'W';
    
    // EXIF GPS (DMS format for lat/lon)
    cmd << "-EXIF:GPSLatitude=\"" << decimalToDMS(latitude) << "\" ";
    cmd << "-EXIF:GPSLatitudeRef=" << latRef << " ";
    cmd << "-EXIF:GPSLongitude=\"" << decimalToDMS(longitude) << "\" ";
    cmd << "-EXIF:GPSLongitudeRef=" << lonRef << " ";
    
    // GPS Version ID must be "2.3.0.0" or "2 3 0 0" array format
    cmd << "-EXIF:GPSVersionID=\"2.3.0.0\" ";
    cmd << "-EXIF:GPSMapDatum=\"WGS-84\" ";
    
    if (altitude != 0.0) {
        cmd << "-EXIF:GPSAltitude=" << std::abs(altitude) << " ";
        cmd << "-EXIF:GPSAltitudeRef=" << (altitude >= 0 ? "0" : "1") << " ";
    }
    
    // XMP GPS (decimal degrees) for applications that prefer XMP
    cmd << "-XMP:GPSLatitude=" << std::fixed << std::setprecision(8) << latitude << " ";
    cmd << "-XMP:GPSLongitude=" << std::fixed << std::setprecision(8) << longitude << " ";
    if (altitude != 0.0) {
        cmd << "-XMP:GPSAltitude=" << std::abs(altitude) << " ";
    }
    
    cmd << "-overwrite_original ";
    cmd << "\"" << imagePath << "\"\"";
    
    return cmd.str();
}
