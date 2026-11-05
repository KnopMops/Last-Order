#pragma once

#include "map/MapData.h"
#include <string>

namespace knopmops::map {

class MapParser {
public:
    static bool LoadFromFile(const std::string& path, MapDocument& outDoc);
    static bool LoadFromString(const std::string& source, MapDocument& outDoc);
    static const std::string& GetLastError();

private:
    static std::string s_lastError;
};

}

