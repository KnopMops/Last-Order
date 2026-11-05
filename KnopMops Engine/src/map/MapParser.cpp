#include "map/MapParser.h"
#include "core/Log.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>

namespace knopmops::map {

std::string MapParser::s_lastError;

const std::string& MapParser::GetLastError() {
    return s_lastError;
}

namespace {

struct Token {
    std::string text;
    bool quoted = false;
};

class Tokenizer {
public:
    explicit Tokenizer(const std::string& src) : m_src(src) {}

    bool Next(Token& out) {
        SkipWhitespaceAndComments();
        if (m_pos >= m_src.size()) return false;

        char c = m_src[m_pos];
        out.quoted = false;
        out.text.clear();

        if (c == '"') {
            ++m_pos;
            while (m_pos < m_src.size() && m_src[m_pos] != '"') {
                out.text.push_back(m_src[m_pos++]);
            }
            if (m_pos < m_src.size() && m_src[m_pos] == '"') ++m_pos;
            out.quoted = true;
            return true;
        }

        if (c == '{' || c == '}' || c == '(' || c == ')' || c == '[' || c == ']') {
            out.text.push_back(c);
            ++m_pos;
            return true;
        }

        while (m_pos < m_src.size() && !std::isspace(static_cast<unsigned char>(m_src[m_pos])) &&
               m_src[m_pos] != '{' && m_src[m_pos] != '}' &&
               m_src[m_pos] != '(' && m_src[m_pos] != ')' &&
               m_src[m_pos] != '[' && m_src[m_pos] != ']') {
            out.text.push_back(m_src[m_pos++]);
        }
        return true;
    }

private:
    void SkipWhitespaceAndComments() {
        while (m_pos < m_src.size()) {
            if (std::isspace(static_cast<unsigned char>(m_src[m_pos]))) {
                ++m_pos;
                continue;
            }
            if (m_pos + 1 < m_src.size() && m_src[m_pos] == '/' && m_src[m_pos + 1] == '/') {
                m_pos += 2;
                while (m_pos < m_src.size() && m_src[m_pos] != '\n') ++m_pos;
                continue;
            }
            break;
        }
    }

    const std::string& m_src;
    size_t m_pos = 0;
};

bool ParseVector3(Tokenizer& tok, Vector3& out) {
    Token t;
    if (!tok.Next(t) || t.text != "(") return false;
    if (!tok.Next(t)) return false; out.x = std::stof(t.text);
    if (!tok.Next(t)) return false; out.y = std::stof(t.text);
    if (!tok.Next(t)) return false; out.z = std::stof(t.text);
    if (!tok.Next(t) || t.text != ")") return false;
    return true;
}

}

bool MapParser::LoadFromFile(const std::string& path, MapDocument& outDoc) {
    std::ifstream file(path);
    if (!file.is_open()) {
        s_lastError = "Could not open map file: " + path;
        core::Log::Error(s_lastError);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    bool res = LoadFromString(buffer.str(), outDoc);
    if (res) {
        outDoc.SetFilePath(path);
        outDoc.SetModified(false);
        core::Log::Success("Successfully loaded map: " + path);
    }
    return res;
}

bool MapParser::LoadFromString(const std::string& source, MapDocument& outDoc) {
    outDoc.Clear();
    Tokenizer tok(source);
    Token t;

    int entityIndex = 0;
    while (tok.Next(t)) {
        if (t.text != "{") continue;

        MapEntity ent;
        bool inEntity = true;

        while (inEntity && tok.Next(t)) {
            if (t.text == "}") {
                inEntity = false;
                break;
            }

            if (t.text == "{") {
                MapBrush brush;
                brush.name = "Brush_" + std::to_string(ent.brushes.size());
                bool inBrush = true;

                while (inBrush && tok.Next(t)) {
                    if (t.text == "}") {
                        inBrush = false;
                        break;
                    }

                    if (t.text == "(") {
                        MapFace face;
                        Token coord;
                        if (!tok.Next(coord)) break; face.planePoints[0].x = std::stof(coord.text);
                        if (!tok.Next(coord)) break; face.planePoints[0].y = std::stof(coord.text);
                        if (!tok.Next(coord)) break; face.planePoints[0].z = std::stof(coord.text);
                        if (!tok.Next(coord) || coord.text != ")") break;

                        if (!ParseVector3(tok, face.planePoints[1])) break;
                        if (!ParseVector3(tok, face.planePoints[2])) break;

                        Token texToken;
                        if (!tok.Next(texToken)) break;
                        face.texture = texToken.text;

                        Token next;
                        if (tok.Next(next)) {
                            if (next.text == "[") {
                                Token val;
                                if (tok.Next(val)) face.uAxis[0] = std::stod(val.text);
                                if (tok.Next(val)) face.uAxis[1] = std::stod(val.text);
                                if (tok.Next(val)) face.uAxis[2] = std::stod(val.text);
                                if (tok.Next(val)) face.uOffset = std::stod(val.text);
                                tok.Next(val);

                                tok.Next(val);
                                if (tok.Next(val)) face.vAxis[0] = std::stod(val.text);
                                if (tok.Next(val)) face.vAxis[1] = std::stod(val.text);
                                if (tok.Next(val)) face.vAxis[2] = std::stod(val.text);
                                if (tok.Next(val)) face.vOffset = std::stod(val.text);
                                tok.Next(val);

                                if (tok.Next(val)) face.rotation = std::stod(val.text);
                                if (tok.Next(val)) face.uScale = std::stod(val.text);
                                if (tok.Next(val)) face.vScale = std::stod(val.text);
                            } else {
                                try {
                                    face.uOffset = std::stod(next.text);
                                    Token val;
                                    if (tok.Next(val)) face.vOffset = std::stod(val.text);
                                    if (tok.Next(val)) face.rotation = std::stod(val.text);
                                    if (tok.Next(val)) face.uScale = std::stod(val.text);
                                    if (tok.Next(val)) face.vScale = std::stod(val.text);
                                } catch (...) {}
                            }
                        }

                        brush.faces.push_back(face);
                    }
                }

                brush.BuildPolygons();
                ent.brushes.push_back(brush);
                continue;
            }

            if (t.quoted || !t.text.empty()) {
                std::string key = t.text;
                Token val;
                if (tok.Next(val)) {
                    ent.Set(key, val.text);
                }
            }
        }

        ent.classname = ent.Get("classname", "unknown");
        ent.SyncOriginFromProperties();

        if (ent.classname == "worldspawn" || entityIndex == 0) {
            outDoc.GetWorldspawn() = ent;
        } else {
            outDoc.AddEntity(ent);
        }
        ++entityIndex;
    }

    outDoc.RebuildAllGeometry();
    return true;
}

}

