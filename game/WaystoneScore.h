#pragma once

#include "WaystoneData.h"
#include "WaystoneScanner.h"
#include "../config/Settings.h"

#include <string>
#include <vector>

namespace WaystoneHelper {

struct StatBadge {
    std::string statId;
    std::string label;
    int value = 0;
    const float* color = nullptr;
};

struct GroupMatch {
    std::string groupId;
    std::string name;
    float color[4] = {1.f, 1.f, 1.f, 1.f};
    int matched = 0;
    int selected = 0;
    std::vector<std::string> matchedLabels;

    std::string BadgeLabel() const {
        if (selected <= 1) return std::to_string(matched);
        return std::to_string(matched) + "/" + std::to_string(selected);
    }
};

struct Score {
    bool matched = false;
    int  affixCount = 0;
    int  tier = 0;
    bool hasTargetAffixCount = false;
    bool hasAffixCountBadge = false;
    std::vector<StatBadge> importantStats;
    std::vector<GroupMatch> affixGroupBadges;
    std::vector<GroupMatch> trackedGroupMatches;
    bool border = false;
    std::vector<std::string> borderMet;
};

inline void CopyColor(float dst[4], const float src[4]) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
}

inline bool AffixPresent(const Affix& a,
                         const std::unordered_set<std::string>& matchKeys) {
    for (const auto& id : a.NormModIds)
        if (matchKeys.count(id)) return true;
    return false;
}

inline std::vector<GroupMatch> EvaluateGroups(const WaystoneHelperConfig::Settings& s,
                                              const WaystoneData& data,
                                              const ParsedWaystone& t) {
    std::vector<GroupMatch> out;
    for (const auto& g : s.affixGroups) {
        if (!g.enabled || g.selectedAffixIds.empty()) continue;
        GroupMatch gm;
        gm.groupId = g.id;
        gm.name = g.name;
        CopyColor(gm.color, g.color);
        for (const auto& affixId : g.selectedAffixIds) {
            const Affix* a = data.FindAffix(affixId);
            if (!a) continue;
            ++gm.selected;
            if (AffixPresent(*a, t.matchKeys)) {
                ++gm.matched;
                gm.matchedLabels.push_back(a->ShortLabel());
            }
        }
        if (gm.selected > 0 && gm.matched >= g.minMatched)
            out.push_back(std::move(gm));
    }
    return out;
}

inline void EvaluateBorder(const WaystoneHelperConfig::Settings& s, const WaystoneData& data,
                           const ParsedWaystone& t, Score& e) {
    int conditions = 0, met = 0;

    if (s.borderRequireAffixCount) {
        ++conditions;
        if (t.affixCount >= s.targetAffixCount) {
            ++met;
            e.borderMet.push_back(std::to_string(t.affixCount) + "/"
                                  + std::to_string(s.targetAffixCount) + " affixes");
        }
    }

    if (s.borderMinTier > 0) {
        ++conditions;
        if (t.tier >= s.borderMinTier) {
            ++met;
            e.borderMet.push_back("T" + std::to_string(t.tier));
        }
    }

    for (const auto& g : data.GeneratedStats()) {
        const int mn = s.StatMin(g.statId);
        if (mn <= 0) continue;
        ++conditions;
        int val = 0;
        auto vIt = t.statValues.find(g.statId);
        if (vIt != t.statValues.end()) val = vIt->second;
        if (val >= mn) {
            ++met;
            e.borderMet.push_back(g.badgeLabel + std::to_string(val) + ">=" + std::to_string(mn));
        }
    }

    e.border = conditions > 0 && met == conditions;
}

inline Score Evaluate(const WaystoneHelperConfig::Settings& s, const WaystoneData& data,
                      const ParsedWaystone& t) {
    Score e;
    e.affixCount = t.affixCount;
    e.tier = t.tier;
    e.hasTargetAffixCount = t.affixCount >= s.targetAffixCount;
    e.hasAffixCountBadge = s.showAffixCountBadge && e.hasTargetAffixCount;

    if (s.highlightImportantAffixes) {
        for (const auto& g : data.GeneratedStats()) {
            if (!s.StatDisplayEnabled(g.statId)) continue;
            if (!t.presentStatIds.count(g.statId)) continue;
            StatBadge b;
            b.statId = g.statId;
            b.label = g.badgeLabel;
            auto vIt = t.statValues.find(g.statId);
            b.value = (vIt != t.statValues.end()) ? vIt->second : 0;
            b.color = s.StatColor(g.statId);
            e.importantStats.push_back(std::move(b));
        }
    }

    if (s.enableAffixGroups && !s.affixGroups.empty())
        e.trackedGroupMatches = EvaluateGroups(s, data, t);
    if (s.showAffixGroupBadges)
        e.affixGroupBadges = e.trackedGroupMatches;

    if (s.borderEnabled)
        EvaluateBorder(s, data, t, e);

    e.matched = e.hasAffixCountBadge || !e.importantStats.empty()
                || !e.affixGroupBadges.empty() || e.border;
    return e;
}

inline bool HasAnythingToShow(const Score& e) {
    return e.matched || !e.trackedGroupMatches.empty();
}

}
