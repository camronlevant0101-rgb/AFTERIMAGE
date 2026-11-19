// Writes every factory preset to <dir>/<Category>/<Name>.aipreset (human-readable text).
#include "PresetIO.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
using namespace afterimage;
int main (int argc, char** argv)
{
    const std::filesystem::path root = argc > 1 ? argv[1] : "Presets";
    for (const auto& f : kFactoryPresets)
    {
        const auto dir = root / kCategoryNames[(int) f.category];
        std::filesystem::create_directories (dir);
        PresetData d; d.name = f.name; d.category = kCategoryNames[(int) f.category]; d.p = f.p;
        std::ofstream (dir / (std::string (f.name) + ".aipreset")) << serializePreset (d);
    }
    std::printf ("Exported %d presets to %s\n", kNumFactoryPresets, root.string().c_str());
}
