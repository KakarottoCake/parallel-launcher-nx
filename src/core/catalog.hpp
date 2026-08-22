#pragma once

#include <string>
#include <vector>

#include "src/core/models.hpp"

enum class SortOrder {
    Popular = 0,
    TopRated,
    Newest,
    NameAsc,
    StarCount,
    Difficulty,
};

inline const char* getSortOrderName(SortOrder order) {
    switch (order) {
        case SortOrder::Popular:    return "Most Downloaded";
        case SortOrder::TopRated:   return "Highest Rated";
        case SortOrder::Newest:     return "Newest First";
        case SortOrder::NameAsc:    return "Name (A-Z)";
        case SortOrder::StarCount:  return "Star Count";
        case SortOrder::Difficulty: return "Difficulty";
        default:                    return "Default";
    }
}

// What the tabs ask the catalog for. An empty search / "All" category matches
// everything.
struct CatalogQuery {
    std::string search;
    std::string category      = "All";
    SortOrder   sort          = SortOrder::Popular;
    bool        installedOnly = false;
};

// In-memory catalog of hacks. Today it is seeded from a bundled sample set so
// the UI is exercised end to end; the RHDC client swaps in over the same
// interface without the tabs changing.
class Catalog {
  public:
    static Catalog& instance();

    // Seeds the sample data set. Idempotent.
    void loadSampleData();

    const std::vector<HackInfo>& all() const { return m_hacks; }

    // Pointers are stable until the next mutation of the catalog; the tabs
    // rebuild their result list on every query so this is safe.
    std::vector<const HackInfo*> query(const CatalogQuery& q) const;

    // Distinct categories present in the catalog, "All" first.
    std::vector<std::string> categories() const;

    const HackInfo* find(const std::string& id) const;
    HackInfo*       findMutable(const std::string& id);

    int installedCount() const;
    int totalCount() const { return static_cast<int>(m_hacks.size()); }

  private:
    Catalog() = default;

    std::vector<HackInfo> m_hacks;
    bool                  m_seeded = false;
};
