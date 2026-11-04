#pragma once
#include <string>

#ifndef LO_GAME_VERSION
#define LO_GAME_VERSION "2.0.1"
#endif

#ifndef LO_COMMIT_HASH
#define LO_COMMIT_HASH "unknown"
#endif

#ifndef LO_BUILD_ID
#define LO_BUILD_ID "0"
#endif

namespace game {

inline std::string GetGameVersion() {
    return LO_GAME_VERSION;
}

inline std::string GetCommitHash() {
    return LO_COMMIT_HASH;
}

inline std::string GetBuildId() {
    return LO_BUILD_ID;
}

inline std::string GetVersionBuildString() {
    return std::string("2.0.1-") + LO_COMMIT_HASH;
}

}
