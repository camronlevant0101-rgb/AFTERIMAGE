// AFTERIMAGE - plain-text preset format (.aipreset). Message-thread only (uses std::string).
//   AFTERIMAGE-PRESET 1
//   name=Ghost Vocal
//   category=Vocal
//   size=160 ...
#pragma once
#include "ParamTable.h"
#include "Presets.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

namespace afterimage
{
struct PresetData { std::string name = "Init"; std::string category = "User"; Params p; };

inline std::string serializePreset (const PresetData& d)
{
    std::ostringstream o;
    o << "AFTERIMAGE-PRESET 1\nname=" << d.name << "\ncategory=" << d.category << "\n";
    for (int i = 0; i < kNumParams; ++i)
    {
        char buf[64]; std::snprintf (buf, sizeof buf, "%.9g", (double) (d.p.*(kParams[i].member)));
        o << kParams[i].id << "=" << buf << "\n";
    }
    return o.str();
}

inline float clampToParam (const ParamInfo& pi, float v)
{
    if (! (v == v)) return pi.def;
    v = v < pi.min ? pi.min : (v > pi.max ? pi.max : v);
    return pi.kind == Kind::Float ? v : (float) (int) (v + 0.5f);
}

// Returns false if the header is missing. Unknown keys are ignored; missing keys keep defaults.
inline bool parsePreset (const std::string& text, PresetData& out)
{
    std::istringstream in (text);
    std::string line;
    if (! std::getline (in, line) || line.rfind ("AFTERIMAGE-PRESET", 0) != 0) return false;
    PresetData d;
    while (std::getline (in, line))
    {
        if (! line.empty() && line.back() == '\r') line.pop_back();
        const auto eq = line.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr (0, eq), v = line.substr (eq + 1);
        if (k == "name") d.name = v;
        else if (k == "category") d.category = v;
        else { const int idx = findParam (k.c_str()); if (idx >= 0) d.p.*(kParams[idx].member) = clampToParam (kParams[idx], (float) std::atof (v.c_str())); }
    }
    out = d; return true;
}
} // namespace afterimage
