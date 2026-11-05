#pragma once

#include "map/MapData.h"
#include <string>
#include <vector>

namespace knopmops::map {

class BrushBuilder {
public:
    static MapBrush CreateBox(Vector3 minWorld, Vector3 maxWorld, const std::string& texture = "motel_wall");

    static MapBrush CreateCubeAt(Vector3 centerWorld, Vector3 sizeWorld, const std::string& texture = "motel_wall");

    static std::vector<MapBrush> CreateHollowRoom(
        Vector3 centerWorld,
        Vector3 sizeWorld,
        float wallThickness = 0.25f,
        const std::string& wallTex = "motel_wallpaper",
        const std::string& floorTex = "motel_carpet",
        const std::string& ceilingTex = "motel_ceiling"
    );

    static MapBrush CreateCylinder(
        Vector3 centerWorld,
        float radius,
        float height,
        int sides = 8,
        const std::string& texture = "motel_concrete"
    );

    static std::vector<MapBrush> HollowBrush(const MapBrush& brush, float thickness = 0.25f, const std::string& insideTex = "motel_wallpaper");
};

}

