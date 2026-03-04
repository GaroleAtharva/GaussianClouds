#include "PLYLoader.h"
#include "GaussianData.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <cstring>
#include <unordered_map>
#include <filesystem>

// Supported PLY property types and their byte sizes
static size_t plyTypeSize(const std::string& type) {
    if (type == "float" || type == "float32") return 4;
    if (type == "double" || type == "float64") return 8;
    if (type == "uchar" || type == "uint8") return 1;
    if (type == "char" || type == "int8") return 1;
    if (type == "ushort" || type == "uint16") return 2;
    if (type == "short" || type == "int16") return 2;
    if (type == "uint" || type == "uint32") return 4;
    if (type == "int" || type == "int32") return 4;
    return 0;
}

struct PLYProperty {
    std::string name;
    std::string type;
    size_t byteOffset = 0;
    size_t byteSize = 0;
};

PLYLoadResult PLYLoader::load(const std::string& filepath, GaussianData& outData) {
    PLYLoadResult result;
    auto startTime = std::chrono::high_resolution_clock::now();

    outData.clear();

    // Check file exists and get size
    std::error_code ec;
    if (!std::filesystem::exists(filepath, ec)) {
        result.error = "File not found: " + filepath;
        return result;
    }
    result.fileSizeBytes = static_cast<size_t>(std::filesystem::file_size(filepath, ec));

    // Open file in binary mode
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        result.error = "Cannot open file: " + filepath;
        return result;
    }

    // --- Parse ASCII header ---
    std::string line;
    std::string format;
    size_t vertexCount = 0;
    bool inVertexElement = false;
    std::vector<PLYProperty> properties;
    size_t vertexStride = 0;

    // Read magic number
    std::getline(file, line);
    // Strip carriage return if present
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "ply") {
        result.error = "Not a PLY file (missing 'ply' magic)";
        return result;
    }

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == "end_header") break;

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "format") {
            iss >> format;
            if (format != "binary_little_endian") {
                result.error = "Unsupported PLY format: " + format + " (only binary_little_endian supported)";
                return result;
            }
        } else if (token == "element") {
            std::string elemName;
            size_t elemCount;
            iss >> elemName >> elemCount;
            if (elemName == "vertex") {
                vertexCount = elemCount;
                inVertexElement = true;
            } else {
                inVertexElement = false;
            }
        } else if (token == "property" && inVertexElement) {
            std::string type, name;
            iss >> type >> name;

            // Skip list properties
            if (type == "list") continue;

            size_t sz = plyTypeSize(type);
            if (sz == 0) {
                result.error = "Unknown property type: " + type;
                return result;
            }

            PLYProperty prop;
            prop.name = name;
            prop.type = type;
            prop.byteOffset = vertexStride;
            prop.byteSize = sz;
            properties.push_back(prop);
            vertexStride += sz;
        }
    }

    if (vertexCount == 0) {
        result.error = "No vertices found in PLY header";
        return result;
    }

    // Build property name -> index lookup
    std::unordered_map<std::string, size_t> propIndex;
    for (size_t i = 0; i < properties.size(); ++i) {
        propIndex[properties[i].name] = i;
    }

    // Validate mandatory properties (x, y, z)
    if (propIndex.find("x") == propIndex.end() ||
        propIndex.find("y") == propIndex.end() ||
        propIndex.find("z") == propIndex.end()) {
        result.error = "PLY file missing required x, y, z properties";
        return result;
    }

    // --- Read binary data block ---
    std::streampos headerEnd = file.tellg();
    size_t dataSize = vertexCount * vertexStride;

    std::vector<char> buffer(dataSize);
    file.read(buffer.data(), static_cast<std::streamsize>(dataSize));
    if (file.gcount() != static_cast<std::streamsize>(dataSize)) {
        result.error = "Unexpected end of file reading binary data (expected " +
                       std::to_string(dataSize) + " bytes, got " +
                       std::to_string(file.gcount()) + ")";
        return result;
    }

    // --- Extract properties ---
    outData.count = vertexCount;
    outData.sourcePath = filepath;
    outData.positions.resize(vertexCount);

    // Helper to read a float property (handles float and double source types)
    auto readFloat = [&](size_t vertIdx, const PLYProperty& prop) -> float {
        const char* ptr = buffer.data() + vertIdx * vertexStride + prop.byteOffset;
        if (prop.byteSize == 4) {
            float val;
            std::memcpy(&val, ptr, 4);
            return val;
        } else if (prop.byteSize == 8) {
            double val;
            std::memcpy(&val, ptr, 8);
            return static_cast<float>(val);
        }
        return 0.0f;
    };

    // Find property indices for optional fields
    auto findProp = [&](const std::string& name) -> const PLYProperty* {
        auto it = propIndex.find(name);
        return (it != propIndex.end()) ? &properties[it->second] : nullptr;
    };

    const PLYProperty* px = findProp("x");
    const PLYProperty* py = findProp("y");
    const PLYProperty* pz = findProp("z");
    const PLYProperty* pnx = findProp("nx");
    const PLYProperty* pny = findProp("ny");
    const PLYProperty* pnz = findProp("nz");
    const PLYProperty* popacity = findProp("opacity");

    // SH DC coefficients
    const PLYProperty* pf_dc_0 = findProp("f_dc_0");
    const PLYProperty* pf_dc_1 = findProp("f_dc_1");
    const PLYProperty* pf_dc_2 = findProp("f_dc_2");

    // Scale
    const PLYProperty* pscale_0 = findProp("scale_0");
    const PLYProperty* pscale_1 = findProp("scale_1");
    const PLYProperty* pscale_2 = findProp("scale_2");

    // Rotation
    const PLYProperty* prot_0 = findProp("rot_0");
    const PLYProperty* prot_1 = findProp("rot_1");
    const PLYProperty* prot_2 = findProp("rot_2");
    const PLYProperty* prot_3 = findProp("rot_3");

    // SH rest coefficients (f_rest_0 .. f_rest_44)
    std::array<const PLYProperty*, 45> pf_rest{};
    for (int i = 0; i < 45; ++i) {
        pf_rest[i] = findProp("f_rest_" + std::to_string(i));
    }

    // Check which optional groups are present
    bool hasNormals = pnx && pny && pnz;
    bool hasSHDC = pf_dc_0 && pf_dc_1 && pf_dc_2;
    bool hasOpacity = popacity != nullptr;
    bool hasScale = pscale_0 && pscale_1 && pscale_2;
    bool hasRotation = prot_0 && prot_1 && prot_2 && prot_3;
    bool hasSHRest = pf_rest[0] != nullptr;

    // Allocate optional vectors
    if (hasNormals)   outData.normals.resize(vertexCount);
    if (hasSHDC)      outData.shDC.resize(vertexCount);
    if (hasOpacity)   outData.opacities.resize(vertexCount);
    if (hasScale)     outData.scales.resize(vertexCount);
    if (hasRotation)  outData.rotations.resize(vertexCount);
    if (hasSHRest)    outData.shRest.resize(vertexCount);

    // Extract per-vertex data
    for (size_t i = 0; i < vertexCount; ++i) {
        outData.positions[i] = glm::vec3(
            readFloat(i, *px),
            readFloat(i, *py),
            readFloat(i, *pz)
        );

        if (hasNormals) {
            outData.normals[i] = glm::vec3(
                readFloat(i, *pnx),
                readFloat(i, *pny),
                readFloat(i, *pnz)
            );
        }

        if (hasSHDC) {
            outData.shDC[i] = glm::vec3(
                readFloat(i, *pf_dc_0),
                readFloat(i, *pf_dc_1),
                readFloat(i, *pf_dc_2)
            );
        }

        if (hasOpacity) {
            outData.opacities[i] = readFloat(i, *popacity);
        }

        if (hasScale) {
            outData.scales[i] = glm::vec3(
                readFloat(i, *pscale_0),
                readFloat(i, *pscale_1),
                readFloat(i, *pscale_2)
            );
        }

        if (hasRotation) {
            outData.rotations[i] = glm::quat(
                readFloat(i, *prot_0),  // w
                readFloat(i, *prot_1),  // x
                readFloat(i, *prot_2),  // y
                readFloat(i, *prot_3)   // z
            );
        }

        if (hasSHRest) {
            for (int j = 0; j < 45; ++j) {
                if (pf_rest[j]) {
                    outData.shRest[i][j] = readFloat(i, *pf_rest[j]);
                } else {
                    outData.shRest[i][j] = 0.0f;
                }
            }
        }
    }

    // Also check for simple RGB properties (red, green, blue) as fallback colors
    const PLYProperty* pred = findProp("red");
    const PLYProperty* pgreen = findProp("green");
    const PLYProperty* pblue = findProp("blue");
    bool hasRGB = pred && pgreen && pblue;

    // Compute derived data (SH DC -> colors, sigmoid, exp, bbox)
    outData.computeDerivedData();

    // If no SH DC but has simple RGB, override colors
    if (!hasSHDC && hasRGB) {
        outData.colors.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            const char* ptr = buffer.data() + i * vertexStride;
            float r, g, b;
            // Handle uchar (0-255) vs float (0-1) color properties
            if (pred->byteSize == 1) {
                uint8_t rv, gv, bv;
                std::memcpy(&rv, ptr + pred->byteOffset, 1);
                std::memcpy(&gv, ptr + pgreen->byteOffset, 1);
                std::memcpy(&bv, ptr + pblue->byteOffset, 1);
                r = rv / 255.0f;
                g = gv / 255.0f;
                b = bv / 255.0f;
            } else {
                r = readFloat(i, *pred);
                g = readFloat(i, *pgreen);
                b = readFloat(i, *pblue);
            }
            outData.colors[i] = glm::vec3(r, g, b);
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.loadTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    result.vertexCount = vertexCount;
    result.success = true;

    std::cout << "PLY loaded: " << vertexCount << " vertices from " << filepath
              << " (" << result.loadTimeMs << " ms)" << std::endl;

    return result;
}
