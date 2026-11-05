#pragma once

#include "map/MapData.h"
#include <string>

namespace knopmops::map {

class MapSerializer {
public:
    static bool SaveToFile(const std::string& path, const MapDocument& doc);
    static std::string SaveToString(const MapDocument& doc);
};

}

