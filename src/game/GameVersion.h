#pragma once
#include <string>

#ifndef LO_GAME_VERSION
#define LO_GAME_VERSION "1.0.1"
#endif

#ifndef LO_BUILD_ID
#define LO_BUILD_ID "dc77df1"
#endif

namespace game {

inline std::string GetGameVersion() {
    return LO_GAME_VERSION;
}

inline std::string GetBuildId() {
    return LO_BUILD_ID;
}

inline std::string GetVersionBuildString() {
    return std::string("v") + LO_GAME_VERSION + "_" + LO_BUILD_ID;
}

}

