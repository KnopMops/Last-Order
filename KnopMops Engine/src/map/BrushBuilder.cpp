#include "map/BrushBuilder.h"
#include <cmath>
#include <numbers>

namespace knopmops::map {

MapBrush BrushBuilder::CreateBox(Vector3 minWorld, Vector3 maxWorld, const std::string& texture) {
    MapBrush b;
    b.name = "Box";

    Vector3 qMin = core::WorldToQuake(Vector3{minWorld.x, maxWorld.y, maxWorld.z});
    Vector3 qMax = core::WorldToQuake(Vector3{maxWorld.x, minWorld.y, minWorld.z});

    float x1 = std::min(qMin.x, qMax.x);
    float x2 = std::max(qMin.x, qMax.x);
    float y1 = std::min(qMin.y, qMax.y);
    float y2 = std::max(qMin.y, qMax.y);
    float z1 = std::min(qMin.z, qMax.z);
    float z2 = std::max(qMin.z, qMax.z);

    auto makeFace = [&](Vector3 p1, Vector3 p2, Vector3 p3, double ux, double uy, double uz, double vx, double vy, double vz) {
        MapFace f;
        f.texture = texture;
        f.planePoints[0] = p1;
        f.planePoints[1] = p2;
        f.planePoints[2] = p3;
        f.uAxis[0] = ux; f.uAxis[1] = uy; f.uAxis[2] = uz;
        f.vAxis[0] = vx; f.vAxis[1] = vy; f.vAxis[2] = vz;
        f.uScale = 1.0;
        f.vScale = 1.0;
        return f;
    };

    b.faces.push_back(makeFace({x1, y1, z2}, {x2, y1, z2}, {x2, y2, z2}, 1, 0, 0, 0, -1, 0));
    b.faces.push_back(makeFace({x1, y2, z1}, {x2, y2, z1}, {x2, y1, z1}, 1, 0, 0, 0, -1, 0));
    b.faces.push_back(makeFace({x1, y2, z2}, {x1, y1, z2}, {x1, y1, z1}, 0, 1, 0, 0, 0, -1));
    b.faces.push_back(makeFace({x2, y1, z2}, {x2, y2, z2}, {x2, y2, z1}, 0, 1, 0, 0, 0, -1));
    b.faces.push_back(makeFace({x2, y2, z2}, {x1, y2, z2}, {x1, y2, z1}, 1, 0, 0, 0, 0, -1));
    b.faces.push_back(makeFace({x1, y1, z2}, {x2, y1, z2}, {x2, y1, z1}, 1, 0, 0, 0, 0, -1));

    b.BuildPolygons();
    return b;
}

MapBrush BrushBuilder::CreateCubeAt(Vector3 centerWorld, Vector3 sizeWorld, const std::string& texture) {
    Vector3 half = core::Vec3Scale(sizeWorld, 0.5f);
    Vector3 minW = core::Vec3Sub(centerWorld, half);
    Vector3 maxW = core::Vec3Add(centerWorld, half);
    return CreateBox(minW, maxW, texture);
}

std::vector<MapBrush> BrushBuilder::CreateHollowRoom(
    Vector3 centerWorld,
    Vector3 sizeWorld,
    float wallThickness,
    const std::string& wallTex,
    const std::string& floorTex,
    const std::string& ceilingTex
) {
    std::vector<MapBrush> brushes;
    Vector3 half = core::Vec3Scale(sizeWorld, 0.5f);
    float t = wallThickness;

    Vector3 minW = core::Vec3Sub(centerWorld, half);
    Vector3 maxW = core::Vec3Add(centerWorld, half);

    MapBrush floor = CreateBox(
        Vector3{minW.x, minW.y - t, minW.z},
        Vector3{maxW.x, minW.y, maxW.z},
        floorTex
    );
    floor.name = "Floor";
    brushes.push_back(floor);

    MapBrush ceiling = CreateBox(
        Vector3{minW.x, maxW.y, minW.z},
        Vector3{maxW.x, maxW.y + t, maxW.z},
        ceilingTex
    );
    ceiling.name = "Ceiling";
    brushes.push_back(ceiling);

    MapBrush wallNorth = CreateBox(
        Vector3{minW.x, minW.y, minW.z - t},
        Vector3{maxW.x, maxW.y, minW.z},
        wallTex
    );
    wallNorth.name = "Wall_North";
    brushes.push_back(wallNorth);

    MapBrush wallSouth = CreateBox(
        Vector3{minW.x, minW.y, maxW.z},
        Vector3{maxW.x, maxW.y, maxW.z + t},
        wallTex
    );
    wallSouth.name = "Wall_South";
    brushes.push_back(wallSouth);

    MapBrush wallWest = CreateBox(
        Vector3{minW.x - t, minW.y, minW.z - t},
        Vector3{minW.x, maxW.y, maxW.z + t},
        wallTex
    );
    wallWest.name = "Wall_West";
    brushes.push_back(wallWest);

    MapBrush wallEast = CreateBox(
        Vector3{maxW.x, minW.y, minW.z - t},
        Vector3{maxW.x + t, maxW.y, maxW.z + t},
        wallTex
    );
    wallEast.name = "Wall_East";
    brushes.push_back(wallEast);

    return brushes;
}

MapBrush BrushBuilder::CreateCylinder(
    Vector3 centerWorld,
    float radius,
    float height,
    int sides,
    const std::string& texture
) {
    if (sides < 3) sides = 3;
    MapBrush b;
    b.name = "Cylinder";

    Vector3 qCenter = core::WorldToQuake(centerWorld);
    float qRadius = radius * core::QUAKE_UNITS_PER_METER;
    float qHalfH = (height * 0.5f) * core::QUAKE_UNITS_PER_METER;
    float z1 = qCenter.z - qHalfH;
    float z2 = qCenter.z + qHalfH;

    auto makeFace = [&](Vector3 p1, Vector3 p2, Vector3 p3) {
        MapFace f;
        f.texture = texture;
        f.planePoints[0] = p1;
        f.planePoints[1] = p2;
        f.planePoints[2] = p3;
        f.uScale = 1.0;
        f.vScale = 1.0;
        return f;
    };

    b.faces.push_back(makeFace({-qRadius, -qRadius, z2}, {qRadius, -qRadius, z2}, {qRadius, qRadius, z2}));
    b.faces.push_back(makeFace({-qRadius, qRadius, z1}, {qRadius, qRadius, z1}, {qRadius, -qRadius, z1}));

    for (int i = 0; i < sides; ++i) {
        float a1 = (static_cast<float>(i) / sides) * 2.0f * 3.14159265f;
        float a2 = (static_cast<float>(i + 1) / sides) * 2.0f * 3.14159265f;

        Vector3 p1{qCenter.x + qRadius * std::cos(a1), qCenter.y + qRadius * std::sin(a1), z1};
        Vector3 p2{qCenter.x + qRadius * std::cos(a2), qCenter.y + qRadius * std::sin(a2), z1};
        Vector3 p3{qCenter.x + qRadius * std::cos(a2), qCenter.y + qRadius * std::sin(a2), z2};

        b.faces.push_back(makeFace(p1, p2, p3));
    }

    b.BuildPolygons();
    return b;
}

std::vector<MapBrush> BrushBuilder::HollowBrush(const MapBrush& brush, float thickness, const std::string& insideTex) {
    std::vector<MapBrush> res;
    Vector3 size{
        brush.bounds.max.x - brush.bounds.min.x,
        brush.bounds.max.y - brush.bounds.min.y,
        brush.bounds.max.z - brush.bounds.min.z
    };
    if (size.x <= thickness * 2.0f || size.y <= thickness * 2.0f || size.z <= thickness * 2.0f) {
        res.push_back(brush);
        return res;
    }
    return CreateHollowRoom(brush.center, size, thickness, insideTex, insideTex, insideTex);
}

}

