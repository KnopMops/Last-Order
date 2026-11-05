#include "map/MapSerializer.h"
#include "core/Log.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace knopmops::map {

namespace {

void WriteFace(std::stringstream& ss, const MapFace& f) {
    ss << "( "
       << std::fixed << std::setprecision(1)
       << f.planePoints[0].x << " " << f.planePoints[0].y << " " << f.planePoints[0].z << " ) ( "
       << f.planePoints[1].x << " " << f.planePoints[1].y << " " << f.planePoints[1].z << " ) ( "
       << f.planePoints[2].x << " " << f.planePoints[2].y << " " << f.planePoints[2].z << " ) "
       << (f.texture.empty() ? "motel_wall" : f.texture) << " "
       << "[ " << f.uAxis[0] << " " << f.uAxis[1] << " " << f.uAxis[2] << " " << f.uOffset << " ] "
       << "[ " << f.vAxis[0] << " " << f.vAxis[1] << " " << f.vAxis[2] << " " << f.vOffset << " ] "
       << f.rotation << " " << f.uScale << " " << f.vScale << "\n";
}

void WriteBrush(std::stringstream& ss, const MapBrush& b) {
    ss << "{\n";
    for (const auto& f : b.faces) {
        WriteFace(ss, f);
    }
    ss << "}\n";
}

void WriteEntity(std::stringstream& ss, const MapEntity& ent) {
    ss << "{\n";
    ss << "\"classname\" \"" << (ent.classname.empty() ? "worldspawn" : ent.classname) << "\"\n";
    for (const auto& [k, v] : ent.properties) {
        if (k == "classname") continue;
        ss << "\"" << k << "\" \"" << v << "\"\n";
    }
    for (const auto& b : ent.brushes) {
        WriteBrush(ss, b);
    }
    ss << "}\n";
}

}

std::string MapSerializer::SaveToString(const MapDocument& doc) {
    std::stringstream ss;

    WriteEntity(ss, doc.GetWorldspawn());

    for (const auto& ent : doc.GetEntities()) {
        WriteEntity(ss, ent);
    }

    return ss.str();
}

bool MapSerializer::SaveToFile(const std::string& path, const MapDocument& doc) {
    std::ofstream file(path);
    if (!file.is_open()) {
        core::Log::Error("Failed to save map to file: " + path);
        return false;
    }

    file << SaveToString(doc);
    core::Log::Success("Successfully saved map: " + path);
    return true;
}

}

