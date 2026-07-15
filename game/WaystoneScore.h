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
    int  rarity = 0;
    bool corrupted = false;
    bool hasTargetAffixCount = false;
    bool hasAffixCountBadge = false;
    std::vector<StatBadge> importantStats;
    std::vector<GroupMatch> affixGroupBadges;
    std::vector<GroupMatch> trackedGroupMatches;
    bool border = false;
    float borderColor[4] = {1.f, 1.f, 1.f, 1.f};
    std::string borderName;
    std::vector<std::string> borderMet;
};

inline const char* RarityName(int rarity) {
    switch (rarity) {
        case 0: return "normal";
        case 1: return "magic";
        case 2: return "rare";
        case 3: return "unique";
        default: return "?";
    }
}

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

inline bool RuleHasConditions(const WaystoneHelperConfig::BorderRule& g) {
    if (g.minTier > 0 || g.minAffixes > 0) return true;
    if (g.corrupted != WaystoneHelperConfig::kCorruptedAny || g.rarity >= 0) return true;
    for (const auto& kv : g.statMins)
        if (kv.second > 0) return true;
    return false;
}

inline bool RuleMatches(const WaystoneHelperConfig::BorderRule& g, const WaystoneData& data,
                        const ParsedWaystone& t, std::vector<std::string>& met) {
    met.clear();

    for (const auto& gs : data.GeneratedStats()) {
        auto mIt = g.statMins.find(gs.statId);
        if (mIt == g.statMins.end() || mIt->second <= 0) continue;
        int val = 0;
        auto vIt = t.statValues.find(gs.statId);
        if (vIt != t.statValues.end()) val = vIt->second;
        if (val < mIt->second) return false;
        met.push_back(gs.badgeLabel + std::to_string(val) + ">=" + std::to_string(mIt->second));
    }

    if (g.minTier > 0) {
        if (t.tier < g.minTier) return false;
        met.push_back("T" + std::to_string(t.tier));
    }
    if (g.minAffixes > 0) {
        if (t.affixCount < g.minAffixes) return false;
        met.push_back(std::to_string(t.affixCount) + " affixes");
    }
    if (g.corrupted == WaystoneHelperConfig::kCorruptedOnly) {
        if (!t.corrupted) return false;
        met.push_back("corrupted");
    } else if (g.corrupted == WaystoneHelperConfig::kCorruptedNever) {
        if (t.corrupted) return false;
        met.push_back("not corrupted");
    }
    if (g.rarity >= 0) {
        if (t.rarity != g.rarity) return false;
        met.push_back(RarityName(t.rarity));
    }
    return true;
}

inline void EvaluateBorder(const WaystoneHelperConfig::Settings& s, const WaystoneData& data,
                           const ParsedWaystone& t, Score& e) {
    std::vector<std::string> met;
    for (const auto& g : s.borderRules) {
        if (!g.enabled || !RuleHasConditions(g)) continue;
        if (!RuleMatches(g, data, t, met)) continue;
        e.border = true;
        e.borderName = g.name;
        CopyColor(e.borderColor, g.color);
        e.borderMet = std::move(met);
        return;
    }
}

inline Score Evaluate(const WaystoneHelperConfig::Settings& s, const WaystoneData& data,
                      const ParsedWaystone& t) {
    Score e;
    e.affixCount = t.affixCount;
    e.tier = t.tier;
    e.rarity = t.rarity;
    e.corrupted = t.corrupted;
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
