#include "engine/3d/QuakeMap.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace engine::world3d {

namespace {

struct Token {
    std::string text;
    bool quoted = false;
};

struct DVec {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

DVec Sub(DVec a, DVec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
DVec Add(DVec a, DVec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
DVec Scale(DVec a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double Dot(DVec a, DVec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
DVec Cross(DVec a, DVec b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
DVec Normalize(DVec a) {
    double len = std::sqrt(Dot(a, a));
    return len > 1e-12 ? Scale(a, 1.0 / len) : a;
}

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
            while (m_pos < m_src.size() && m_src[m_pos] != '"') out.text.push_back(m_src[m_pos++]);
            ++m_pos;
            out.quoted = true;
            return true;
        }
        if (IsDelimiter(c)) {
            out.text.push_back(c);
            ++m_pos;
            return true;
        }
        while (m_pos < m_src.size() && !std::isspace(static_cast<unsigned char>(m_src[m_pos])) && !IsDelimiter(m_src[m_pos])) {
            out.text.push_back(m_src[m_pos++]);
        }
        return true;
    }

private:
    static bool IsDelimiter(char c) {
        return c == '{' || c == '}' || c == '(' || c == ')' || c == '[' || c == ']';
    }

    void SkipWhitespaceAndComments() {
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++m_pos;
            } else if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '/') {
                while (m_pos < m_src.size() && m_src[m_pos] != '\n') ++m_pos;
            } else {
                break;
            }
        }
    }

    const std::string& m_src;
    size_t m_pos = 0;
};

double ToDouble(const Token& t) { return std::strtod(t.text.c_str(), nullptr); }

bool Expect(Tokenizer& tk, const char* text) {
    Token t;
    return tk.Next(t) && t.text == text;
}

bool ReadPoint(Tokenizer& tk, DVec& out) {
    Token a, b, c;
    if (!Expect(tk, "(")) return false;
    if (!tk.Next(a) || !tk.Next(b) || !tk.Next(c)) return false;
    out = {ToDouble(a), ToDouble(b), ToDouble(c)};
    return Expect(tk, ")");
}

void AssignStandardAxes(MapFace& face) {
    static const std::array<std::array<DVec, 3>, 6> baseAxes = {{
        {{{0, 0, 1}, {1, 0, 0}, {0, -1, 0}}},
        {{{0, 0, -1}, {1, 0, 0}, {0, -1, 0}}},
        {{{1, 0, 0}, {0, 1, 0}, {0, 0, -1}}},
        {{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}}},
        {{{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}},
        {{{0, -1, 0}, {1, 0, 0}, {0, 0, -1}}},
    }};
    DVec n{face.planeNormal[0], face.planeNormal[1], face.planeNormal[2]};
    size_t best = 0;
    double bestDot = -2.0;
    for (size_t i = 0; i < baseAxes.size(); ++i) {
        double d = Dot(n, baseAxes[i][0]);
        if (d > bestDot) {
            bestDot = d;
            best = i;
        }
    }
    face.uAxis[0] = baseAxes[best][1].x;
    face.uAxis[1] = baseAxes[best][1].y;
    face.uAxis[2] = baseAxes[best][1].z;
    face.vAxis[0] = baseAxes[best][2].x;
    face.vAxis[1] = baseAxes[best][2].y;
    face.vAxis[2] = baseAxes[best][2].z;
}

bool ReadFace(Tokenizer& tk, MapFace& face) {
    DVec p0, p1, p2;
    if (!ReadPoint(tk, p0) || !ReadPoint(tk, p1) || !ReadPoint(tk, p2)) return false;
    DVec n = Normalize(Cross(Sub(p2, p0), Sub(p1, p0)));
    face.planeNormal[0] = n.x;
    face.planeNormal[1] = n.y;
    face.planeNormal[2] = n.z;
    face.planeDist = Dot(n, p0);

    Token tex;
    if (!tk.Next(tex)) return false;
    face.texture = tex.text;

    Token t;
    if (!tk.Next(t)) return false;
    if (t.text == "[") {
        Token ux, uy, uz, uo, vx, vy, vz, vo, rot, sx, sy;
        if (!tk.Next(ux) || !tk.Next(uy) || !tk.Next(uz) || !tk.Next(uo)) return false;
        if (!Expect(tk, "]") || !Expect(tk, "[")) return false;
        if (!tk.Next(vx) || !tk.Next(vy) || !tk.Next(vz) || !tk.Next(vo)) return false;
        if (!Expect(tk, "]")) return false;
        if (!tk.Next(rot) || !tk.Next(sx) || !tk.Next(sy)) return false;
        face.uAxis[0] = ToDouble(ux);
        face.uAxis[1] = ToDouble(uy);
        face.uAxis[2] = ToDouble(uz);
        face.uOffset = ToDouble(uo);
        face.vAxis[0] = ToDouble(vx);
        face.vAxis[1] = ToDouble(vy);
        face.vAxis[2] = ToDouble(vz);
        face.vOffset = ToDouble(vo);
        face.uScale = ToDouble(sx);
        face.vScale = ToDouble(sy);
    } else {
        Token yoff, rot, sx, sy;
        if (!tk.Next(yoff) || !tk.Next(rot) || !tk.Next(sx) || !tk.Next(sy)) return false;
        AssignStandardAxes(face);
        face.uOffset = ToDouble(t);
        face.vOffset = ToDouble(yoff);
        face.uScale = ToDouble(sx);
        face.vScale = ToDouble(sy);
    }
    if (std::abs(face.uScale) < 1e-6) face.uScale = 1.0;
    if (std::abs(face.vScale) < 1e-6) face.vScale = 1.0;
    return true;
}

std::vector<DVec> ClipPolygon(const std::vector<DVec>& poly, DVec n, double d) {
    std::vector<DVec> out;
    constexpr double eps = 1e-5;
    size_t count = poly.size();
    for (size_t i = 0; i < count; ++i) {
        DVec a = poly[i];
        DVec b = poly[(i + 1) % count];
        double da = Dot(n, a) - d;
        double db = Dot(n, b) - d;
        bool aIn = da <= eps;
        bool bIn = db <= eps;
        if (aIn) out.push_back(a);
        if (aIn != bIn) {
            double t = da / (da - db);
            out.push_back(Add(a, Scale(Sub(b, a), t)));
        }
    }
    return out;
}

std::vector<std::vector<DVec>> BuildRawPolygons(const MapBrush& brush) {
    std::vector<std::vector<DVec>> result(brush.faces.size());
    for (size_t i = 0; i < brush.faces.size(); ++i) {
        const MapFace& f = brush.faces[i];
        DVec n{f.planeNormal[0], f.planeNormal[1], f.planeNormal[2]};
        DVec ref = std::abs(n.z) < 0.9 ? DVec{0, 0, 1} : DVec{1, 0, 0};
        DVec a = Normalize(Cross(ref, n));
        DVec b = Cross(n, a);
        DVec c = Scale(n, f.planeDist);
        constexpr double S = 131072.0;
        std::vector<DVec> poly = {
            Add(c, Add(Scale(a, -S), Scale(b, -S))),
            Add(c, Add(Scale(a, S), Scale(b, -S))),
            Add(c, Add(Scale(a, S), Scale(b, S))),
            Add(c, Add(Scale(a, -S), Scale(b, S))),
        };
        for (size_t j = 0; j < brush.faces.size() && poly.size() >= 3; ++j) {
            if (j == i) continue;
            const MapFace& o = brush.faces[j];
            poly = ClipPolygon(poly, DVec{o.planeNormal[0], o.planeNormal[1], o.planeNormal[2]}, o.planeDist);
        }
        std::vector<DVec> cleaned;
        for (const DVec& p : poly) {
            bool duplicate = false;
            for (const DVec& q : cleaned) {
                DVec dlt = Sub(p, q);
                if (Dot(dlt, dlt) < 1e-6) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) cleaned.push_back(p);
        }
        if (cleaned.size() >= 3) result[i] = std::move(cleaned);
    }
    return result;
}

}

Vector2 BrushPolygon::TexelUV(Vector3 p) const {
    return Vector2{
        p.x * uWorld.x + p.y * uWorld.y + p.z * uWorld.z + uOffset,
        p.x * vWorld.x + p.y * vWorld.y + p.z * vWorld.z + vOffset,
    };
}

const std::string& MapEntity::ClassName() const {
    static const std::string empty;
    auto it = props.find("classname");
    return it != props.end() ? it->second : empty;
}

std::string MapEntity::Get(const std::string& key, const std::string& fallback) const {
    auto it = props.find(key);
    return it != props.end() ? it->second : fallback;
}

float MapEntity::GetFloat(const std::string& key, float fallback) const {
    auto it = props.find(key);
    if (it == props.end() || it->second.empty()) return fallback;
    return std::strtof(it->second.c_str(), nullptr);
}

int MapEntity::GetInt(const std::string& key, int fallback) const {
    auto it = props.find(key);
    if (it == props.end() || it->second.empty()) return fallback;
    return std::atoi(it->second.c_str());
}

Vector3 MapEntity::GetVector(const std::string& key, Vector3 fallback) const {
    auto it = props.find(key);
    if (it == props.end()) return fallback;
    std::istringstream ss(it->second);
    Vector3 v = fallback;
    ss >> v.x >> v.y >> v.z;
    return v;
}

Vector3 MapEntity::GetOrigin() const {
    Vector3 q = GetVector("origin");
    return QuakeMap::ToWorld(q.x, q.y, q.z);
}

float MapEntity::GetYawRadians() const {
    return GetFloat("angle", 0.0f) * DEG2RAD;
}

bool QuakeMap::Load(const std::string& path) {
    m_entities.clear();
    m_error.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        m_error = "cannot open " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();
    Tokenizer tk(src);

    Token t;
    while (tk.Next(t)) {
        if (t.text != "{" || t.quoted) {
            m_error = "expected entity";
            return false;
        }
        MapEntity entity;
        while (true) {
            if (!tk.Next(t)) {
                m_error = "unexpected end of file";
                return false;
            }
            if (t.quoted) {
                Token value;
                if (!tk.Next(value)) return false;
                entity.props[t.text] = value.text;
            } else if (t.text == "{") {
                MapBrush brush;
                while (true) {
                    Token peek;
                    Tokenizer probe = tk;
                    if (!probe.Next(peek)) return false;
                    if (peek.text == "}") {
                        tk.Next(peek);
                        break;
                    }
                    MapFace face;
                    if (!ReadFace(tk, face)) {
                        m_error = "bad brush face";
                        return false;
                    }
                    brush.faces.push_back(face);
                }
                if (brush.faces.size() >= 4) entity.brushes.push_back(std::move(brush));
            } else if (t.text == "}") {
                break;
            }
        }
        m_entities.push_back(std::move(entity));
    }
    return !m_entities.empty();
}

const MapEntity* QuakeMap::FindFirst(const std::string& className) const {
    for (const auto& e : m_entities) {
        if (e.ClassName() == className) return &e;
    }
    return nullptr;
}

std::vector<const MapEntity*> QuakeMap::FindAll(const std::string& className) const {
    std::vector<const MapEntity*> out;
    for (const auto& e : m_entities) {
        if (e.ClassName() == className) out.push_back(&e);
    }
    return out;
}

Vector3 QuakeMap::ToWorld(double qx, double qy, double qz) {
    return Vector3{
        static_cast<float>(qx / UNITS_PER_METER),
        static_cast<float>(qz / UNITS_PER_METER),
        static_cast<float>(-qy / UNITS_PER_METER),
    };
}

Vector3 QuakeMap::ToWorldDirection(double qx, double qy, double qz) {
    return Vector3{static_cast<float>(qx), static_cast<float>(qz), static_cast<float>(-qy)};
}

std::vector<BrushPolygon> QuakeMap::BuildPolygons(const MapBrush& brush) {
    std::vector<BrushPolygon> out;
    auto raw = BuildRawPolygons(brush);
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].size() < 3) continue;
        const MapFace& f = brush.faces[i];
        BrushPolygon poly;
        poly.texture = f.texture;
        poly.normal = ToWorldDirection(f.planeNormal[0], f.planeNormal[1], f.planeNormal[2]);
        double su = UNITS_PER_METER / f.uScale;
        double sv = UNITS_PER_METER / f.vScale;
        poly.uWorld = Vector3{static_cast<float>(f.uAxis[0] * su), static_cast<float>(f.uAxis[2] * su), static_cast<float>(-f.uAxis[1] * su)};
        poly.vWorld = Vector3{static_cast<float>(f.vAxis[0] * sv), static_cast<float>(f.vAxis[2] * sv), static_cast<float>(-f.vAxis[1] * sv)};
        poly.uOffset = static_cast<float>(f.uOffset);
        poly.vOffset = static_cast<float>(f.vOffset);

        DVec center{};
        for (const DVec& p : raw[i]) center = Add(center, p);
        center = Scale(center, 1.0 / static_cast<double>(raw[i].size()));

        DVec n{poly.normal.x, poly.normal.y, poly.normal.z};
        DVec ref = std::abs(n.y) < 0.9 ? DVec{0, 1, 0} : DVec{1, 0, 0};
        DVec a = Normalize(Cross(ref, n));
        DVec b = Cross(n, a);
        Vector3 cw = ToWorld(center.x, center.y, center.z);

        std::vector<std::pair<double, Vector3>> sorted;
        for (const DVec& p : raw[i]) {
            Vector3 w = ToWorld(p.x, p.y, p.z);
            DVec d{w.x - cw.x, w.y - cw.y, w.z - cw.z};
            sorted.emplace_back(std::atan2(Dot(d, b), Dot(d, a)), w);
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) { return l.first < r.first; });
        for (const auto& s : sorted) poly.points.push_back(s.second);
        out.push_back(std::move(poly));
    }
    return out;
}

std::vector<Vector3> QuakeMap::BuildVertices(const MapBrush& brush) {
    std::vector<Vector3> out;
    for (const auto& poly : BuildRawPolygons(brush)) {
        for (const DVec& p : poly) {
            Vector3 w = ToWorld(p.x, p.y, p.z);
            bool duplicate = false;
            for (const Vector3& q : out) {
                float dx = q.x - w.x, dy = q.y - w.y, dz = q.z - w.z;
                if (dx * dx + dy * dy + dz * dz < 1e-8f) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) out.push_back(w);
        }
    }
    return out;
}

BoundingBox QuakeMap::BuildBounds(const MapBrush& brush) {
    std::vector<Vector3> verts = BuildVertices(brush);
    BoundingBox box{{1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
    for (const Vector3& v : verts) {
        box.min.x = std::min(box.min.x, v.x);
        box.min.y = std::min(box.min.y, v.y);
        box.min.z = std::min(box.min.z, v.z);
        box.max.x = std::max(box.max.x, v.x);
        box.max.y = std::max(box.max.y, v.y);
        box.max.z = std::max(box.max.z, v.z);
    }
    return box;
}

}
