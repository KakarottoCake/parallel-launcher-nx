#include "src/core/catalog.hpp"

#include <algorithm>
#include <cctype>

#include <borealis.hpp>

namespace {

std::string toLower(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool contains(const std::string& haystack, const std::string& needleLower) {
    if (needleLower.empty())
        return true;
    return toLower(haystack).find(needleLower) != std::string::npos;
}

// Keeps the sample table below readable; positional aggregate init on a struct
// this wide is unmaintainable.
struct SampleHack {
    const char* id;
    const char* name;
    const char* author;
    const char* category;
    int         starCount;
    int         downloads;
    double      rating;
    double      difficulty;
    int         year;
    GfxPlugin   plugin;
    bool        installed;
    const char* description;
};

const SampleHack kSamples[] = {
    { "b3313", "B3313: Internal Affairs", "B3313 Team", "Surreal", 120, 15400, 4.90, 4.0, 2023,
      GfxPlugin::ParaLLEl, true,
      "A liminal, ever-shifting reconstruction of an SM64 that never shipped. Doors lead "
      "somewhere different every time you walk through them, and the castle map quietly "
      "rewrites itself as you collect stars." },

    { "sm74", "Super Mario 74", "Lugmillord", "Classic Overhaul", 157, 28900, 4.80, 3.8, 2010,
      GfxPlugin::GLideN64, true,
      "A 157-star classic built in the SM64 Editor era. Dense, tightly designed courses with "
      "a difficulty curve that keeps climbing right through the endgame." },

    { "green_stars", "Super Mario 64: The Green Stars", "Kampel125", "Classic Overhaul", 130, 32000, 4.70, 3.0, 2012,
      GfxPlugin::GLideN64, true,
      "130 hand-placed green stars across a full set of original worlds. One of the most "
      "widely played complete overhauls, and a reasonable first hack for newcomers." },

    { "star_revenge_1", "Star Revenge 1: Redone", "BroDute", "Legacy Overhaul", 125, 19500, 4.60, 3.5, 2017,
      GfxPlugin::OGRE, true,
      "The opening chapter of the long-running Star Revenge series, rebuilt with modern tools "
      "while keeping the original level flow intact." },

    { "beyond_mirror", "Super Mario 64: Beyond the Cursed Mirror", "Rover", "Standard", 120, 42000, 4.95, 3.2, 2022,
      GfxPlugin::ParaLLEl, false,
      "An award-winning, atmosphere-first hack with custom lighting, an original score, and "
      "puzzle design that leans on observation rather than execution." },

    { "sm64_land", "Super Mario 64 Land", "Kaze Emanuar", "Standard", 132, 78000, 4.88, 3.0, 2016,
      GfxPlugin::GLideN64, false,
      "A 3D Land style reimagining: linear obstacle courses, a Tanooki Suit and Fire Flower, "
      "and 132 stars spread across compact, purpose-built levels." },

    { "sm64_sapphire", "Super Mario 64 Sapphire", "Stargazer", "Mini Hack", 30, 16000, 4.65, 2.5, 2019,
      GfxPlugin::GLideN64, false,
      "A 30-star mini hack set across tropical, crystal and deep-ocean landscapes. Short, "
      "gentle, and unusually pretty for the era." },

    { "kaizo_mario_64", "Kaizo Mario 64", "SM64 Kaizo Club", "Kaizo", 120, 15000, 4.50, 5.0, 2015,
      GfxPlugin::GLideN64, false,
      "Frame-perfect jumps, shell chaining, and no sympathy whatsoever. The reference point "
      "every other Kaizo N64 hack is measured against." },

    { "star_road", "Super Mario Star Road", "Skelux", "Legacy Overhaul", 130, 95000, 4.90, 3.5, 2011,
      GfxPlugin::OGRE, false,
      "130 original stars, a custom soundtrack, and worlds that still hold up fifteen years "
      "later. The single most downloaded SM64 hack ever made." },

    { "last_impact", "Super Mario: The Last Impact", "Kaze Emanuar", "Standard", 130, 61000, 4.82, 3.4, 2015,
      GfxPlugin::GLideN64, false,
      "130 stars, new power-ups, new enemies, and a set of boss fights built from scratch on "
      "top of a heavily modified engine." },

    { "od_castle", "Super Mario 64: Ocean Depths", "Arthurtilly", "Mini Hack", 40, 12400, 4.55, 2.8, 2018,
      GfxPlugin::GLideN64, false,
      "A submerged 40-star hack with a genuinely unnerving sense of scale. Heavy on water "
      "physics, light on hand-holding." },

    { "sm64_rgb", "Star Revenge 6.5: Fadeout Isle", "BroDute", "Legacy Overhaul", 100, 8700, 4.40, 3.9, 2019,
      GfxPlugin::OGRE, false,
      "A mid-series Star Revenge entry with heavy custom geometry. Known for tripping up newer "
      "HLE renderers, which is why the author pins it to OGRE." },

    { "render96", "Render96 Star Showcase", "Render96 Team", "Tech Demo", 15, 22000, 4.30, 1.5, 2021,
      GfxPlugin::ParaLLEl, false,
      "A short showcase built to stress-test high-fidelity assets and framebuffer effects. "
      "Useful as a renderer benchmark before you commit to a long hack." },

    { "lugmillord_master", "Lugmillord's Masterpiece", "Lugmillord", "Kaizo", 150, 21000, 4.80, 4.5, 2020,
      GfxPlugin::ParaLLEl, false,
      "150 stars of veteran-tier level design. Assumes you already know every movement trick "
      "SM64 has and asks you to chain them." },

    { "halfway_island", "Halfway Island", "Nicce", "Mini Hack", 50, 9800, 4.35, 2.2, 2020,
      GfxPlugin::GLideN64, false,
      "A relaxed 50-star hack with an open hub and no time pressure. Frequently recommended as "
      "a palate cleanser between harder projects." },

    { "sm64_ztar", "Ztar Attack 2: Chaos Edition", "Pieordie1", "Standard", 121, 14200, 4.25, 3.6, 2014,
      GfxPlugin::OGRE, false,
      "A sequel-scale hack with an original story, custom bosses, and level themes that swing "
      "hard between playful and hostile." },
};

HackVersion makeVersion(const char* name, GfxPlugin plugin, int64_t releasedAt, bool archived) {
    HackVersion v;
    v.name              = name;
    v.recommendedPlugin = plugin;
    v.releasedAt        = releasedAt;
    v.archived          = archived;
    v.sizeBytes         = 400 * 1024;
    return v;
}

} // namespace

Catalog& Catalog::instance() {
    static Catalog s_instance;
    return s_instance;
}

void Catalog::loadSampleData() {
    if (m_seeded)
        return;

    m_hacks.clear();
    m_hacks.reserve(std::size(kSamples));

    for (const SampleHack& s : kSamples) {
        HackInfo h;
        h.id            = s.id;
        h.name          = s.name;
        h.slug          = s.id;
        h.author        = s.author;
        h.category      = s.category;
        h.description   = s.description;
        h.starCount     = s.starCount;
        h.downloads     = s.downloads;
        h.rating        = s.rating;
        h.difficulty    = s.difficulty;
        h.releaseYear   = s.year;
        h.defaultPlugin = s.plugin;
        h.isInstalled   = s.installed;

        if (s.installed) {
            h.localRomPath     = std::string("sdmc:/switch/parallel-launcher/hacks/") + s.id + ".z64";
            h.installedVersion = "v1.0";
            h.playTime         = 3600;
        }

        // Sample versions: a current release plus one archived predecessor,
        // which is enough to exercise the version picker.
        h.versions.push_back(makeVersion("v1.0 (Latest)", s.plugin, 0, false));
        h.versions.push_back(makeVersion("v0.9 (Archived)", s.plugin, 0, true));

        m_hacks.push_back(std::move(h));
    }

    m_seeded = true;
    brls::Logger::info("Catalog seeded with {} hacks ({} installed)", m_hacks.size(), installedCount());
}

std::vector<const HackInfo*> Catalog::query(const CatalogQuery& q) const {
    const std::string needle = toLower(q.search);

    std::vector<const HackInfo*> results;
    results.reserve(m_hacks.size());

    for (const HackInfo& h : m_hacks) {
        if (q.installedOnly && !h.isInstalled)
            continue;
        if (q.category != "All" && h.category != q.category)
            continue;
        if (!needle.empty() && !contains(h.name, needle) && !contains(h.author, needle)
            && !contains(h.category, needle))
            continue;

        results.push_back(&h);
    }

    std::stable_sort(results.begin(), results.end(),
        [&q](const HackInfo* a, const HackInfo* b) {
            switch (q.sort) {
                case SortOrder::TopRated:   return a->rating > b->rating;
                case SortOrder::Newest:     return a->releaseYear > b->releaseYear;
                case SortOrder::NameAsc:    return toLower(a->name) < toLower(b->name);
                case SortOrder::StarCount:  return a->starCount > b->starCount;
                case SortOrder::Difficulty: return a->difficulty > b->difficulty;
                case SortOrder::Popular:
                default:                    return a->downloads > b->downloads;
            }
        });

    return results;
}

std::vector<std::string> Catalog::categories() const {
    std::vector<std::string> out { "All" };
    for (const HackInfo& h : m_hacks) {
        if (std::find(out.begin(), out.end(), h.category) == out.end())
            out.push_back(h.category);
    }
    std::sort(out.begin() + 1, out.end());
    return out;
}

const HackInfo* Catalog::find(const std::string& id) const {
    for (const HackInfo& h : m_hacks) {
        if (h.id == id)
            return &h;
    }
    return nullptr;
}

HackInfo* Catalog::findMutable(const std::string& id) {
    for (HackInfo& h : m_hacks) {
        if (h.id == id)
            return &h;
    }
    return nullptr;
}

int Catalog::installedCount() const {
    return static_cast<int>(std::count_if(m_hacks.begin(), m_hacks.end(),
        [](const HackInfo& h) { return h.isInstalled; }));
}
