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

struct RuleConditionMatch {
    std::string label;
    float color[4] = {1.f, 1.f, 1.f, 1.f};
};

struct RuleMatch {
    std::string name;
    float color[4] = {1.f, 1.f, 1.f, 1.f};
    int matched = 0;
    int selected = 0;
    std::vector<RuleConditionMatch> conditions;
};

struct Score {
    bool matched = false;
    int  affixCount = 0;
    bool hasTargetAffixCount = false;
    bool hasAffixCountBadge = false;
    std::vector<StatBadge> importantStats;
    std::vector<GroupMatch> affixGroupBadges;
    std::vector<GroupMatch> trackedGroupMatches;
    std::vector<RuleMatch> borderRules;

    bool HasBorder() const { return !borderRules.empty(); }
    const float* BorderColor() const { return borderRules.front().color; }
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

inline std::vector<RuleMatch> EvaluateBorderRules(
    const WaystoneHelperConfig::Settings& s, const WaystoneData& data,
    const ParsedWaystone& t, const std::vector<GroupMatch>& groupMatches) {
    std::vector<RuleMatch> out;
    for (const auto& r : s.borderRules) {
        if (!r.enabled) continue;
        RuleMatch rm;
        rm.name = r.name;
        CopyColor(rm.color, r.color);

        if (r.minAffixCount > 0) {
            ++rm.selected;
            if (t.affixCount >= r.minAffixCount) {
                ++rm.matched;
                RuleConditionMatch c;
                c.label = std::to_string(t.affixCount) + "/"
                          + std::to_string(r.minAffixCount) + " affixes";
                CopyColor(c.color, s.affixCountBadgeColor);
                rm.conditions.push_back(std::move(c));
            }
        }

        if (r.minTier > 0) {
            ++rm.selected;
            if (t.tier >= r.minTier) {
                ++rm.matched;
                RuleConditionMatch c;
                c.label = "T" + std::to_string(t.tier) + " (>=T"
                          + std::to_string(r.minTier) + ")";
                CopyColor(c.color, s.affixCountBadgeColor);
                rm.conditions.push_back(std::move(c));
            }
        }

        for (const auto& sc : r.statConditions) {
            if (!data.IsTrackedStat(sc.statId)) continue;
            ++rm.selected;
            int val = 0;
            auto vIt = t.statValues.find(sc.statId);
            if (vIt != t.statValues.end()) val = vIt->second;
            const bool ok = sc.minValue <= 0
                ? t.presentStatIds.count(sc.statId) > 0
                : val >= sc.minValue;
            if (ok) {
                ++rm.matched;
                RuleConditionMatch c;
                c.label = data.BadgeLabelFor(sc.statId);
                if (val > 0) c.label += std::to_string(val);
                if (sc.minValue > 0) c.label += ">=" + std::to_string(sc.minValue);
                CopyColor(c.color, s.StatColor(sc.statId));
                rm.conditions.push_back(std::move(c));
            }
        }

        for (const auto& groupId : r.selectedAffixGroupIds) {
            ++rm.selected;
            for (const auto& gm : groupMatches) {
                if (gm.groupId != groupId) continue;
                ++rm.matched;
                RuleConditionMatch c;
                c.label = gm.name + " " + gm.BadgeLabel();
                CopyColor(c.color, gm.color);
                rm.conditions.push_back(std::move(c));
                break;
            }
        }

        if (rm.selected == 0) continue;
        int required = r.minMatches;
        if (required > rm.selected) required = rm.selected;
        if (required < 1) required = 1;
        if (rm.matched >= required) out.push_back(std::move(rm));
    }
    return out;
}

inline Score Evaluate(const WaystoneHelperConfig::Settings& s, const WaystoneData& data,
                      const ParsedWaystone& t) {
    Score e;
    e.affixCount = t.affixCount;
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

    if (s.enableBorderRules)
        e.borderRules = EvaluateBorderRules(s, data, t, e.trackedGroupMatches);

    e.matched = e.hasAffixCountBadge || !e.importantStats.empty()
                || !e.affixGroupBadges.empty() || e.HasBorder();
    return e;
}

inline bool HasAnythingToShow(const Score& e) {
    return e.matched || !e.trackedGroupMatches.empty();
}

}
