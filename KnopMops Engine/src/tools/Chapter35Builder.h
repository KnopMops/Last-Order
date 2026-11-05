#pragma once

#include <string>

namespace knopmops::tools {

// Собирает проект projects/chapter_3_5 (уровни razyezd и tunnel). Скрипты лежат в проекте отдельно.
bool BuildChapter35(const std::string& projectsDir, std::string& error);

}
