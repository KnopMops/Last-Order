#pragma once

#include "scene/Scene.h"
#include "map/MapData.h"
#include <string>
#include <vector>

namespace knopmops::chapter4 {

struct Chapter4Config {
    std::string title = "Глава 4: Подземный комплекс «Заслон»";
    std::string setting = "Заброшенный советский бункер связи под особняком";
    int survivalDays = 5;
    float monsterSpeed = 2.6f;
    float hearingSensitivity = 9.0f;
    bool enableLimpInjuryDay3 = true;
    bool enableCarEscape = true;
    bool enableTunnelEscape = true;
    bool enableRadioEscape = true;
};

class Chapter4Director {
public:
    Chapter4Director();
    ~Chapter4Director();

    void Init();
    void RenderUI(scene::Scene* scene, map::MapDocument* mapDoc);

    bool ExportChapter4(const scene::Scene& scene, const map::MapDocument& mapDoc);

    const Chapter4Config& GetConfig() const { return m_config; }
    Chapter4Config& GetConfigMut() { return m_config; }

private:
    Chapter4Config m_config;
    char m_titleBuf[128]{};
    char m_settingBuf[256]{};

    void GenerateChapterAssets(const std::string& baseDir);
};

}

