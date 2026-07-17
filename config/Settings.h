#pragma once

#include "../game/WaystoneData.h"
#include "../third_party/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace WaystoneHelperConfig {

inline constexpr int   kScanIntervalMinMs  = 150;
inline constexpr int   kScanIntervalMaxMs  = 2000;
inline constexpr int   kTargetAffixMin     = 0;
inline constexpr int   kTargetAffixMax     = 8;
inline constexpr int   kBorderThicknessMin = 1;
inline constexpr int   kBorderThicknessMax = 12;
inline constexpr float kBadgeScaleMin      = 0.5f;
inline constexpr float kBadgeScaleMax      = 1.8f;
inline constexpr int   kMinMatchedAffixMax = 8;
inline constexpr int   kMinMatchesMax      = 16;
inline constexpr int   kMinTierMax         = 16;
inline constexpr int   kStatThresholdMax   = 1000;
inline constexpr std::size_t kMaxNameLen   = 95;

inline void ClampColor(float c[4]) {
    for (int i = 0; i < 4; ++i)
        c[i] = c[i] < 0.f ? 0.f : (c[i] > 1.f ? 1.f : c[i]);
}

inline void SetColor(float dst[4], float r, float g, float b, float a) {
    dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = a;
}

inline nlohmann::json ColorToJson(const float c[4]) {
    return nlohmann::json{c[0], c[1], c[2], c[3]};
}

inline void ColorFromJson(const nlohmann::json& j, float dst[4]) {
    if (!j.is_array()) return;
    for (int i = 0; i < 4 && i < static_cast<int>(j.size()); ++i)
        if (j[i].is_number()) dst[i] = j[i].get<float>();
    ClampColor(dst);
}

inline std::vector<std::string> StringVecFromJson(const nlohmann::json& j) {
    std::vector<std::string> out;
    if (!j.is_array()) return out;
    for (const auto& e : j) {
        if (!e.is_string()) continue;
        std::string s = e.get<std::string>();
        if (s.empty()) continue;
        if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(std::move(s));
    }
    return out;
}

struct AffixGroup {
    std::string id;
    std::string name = "New Group";
    bool  enabled = true;
    int   minMatched = 1;
    float color[4] = {0.0f, 0.749f, 1.0f, 1.0f};
    std::vector<std::string> selectedAffixIds;
};

inline constexpr int kCorruptedAny = 0;
inline constexpr int kCorruptedOnly = 1;
inline constexpr int kCorruptedNever = 2;

struct BorderRule {
    std::string id;
    std::string name = "New Group";
    bool  enabled = true;
    float color[4] = {0.20f, 0.90f, 0.35f, 1.0f};
    std::unordered_map<std::string, int> statMins;
    int   minTier = 0;
    int   minAffixes = 0;
    int   corrupted = kCorruptedAny;
    int   rarity = -1;
};

struct StatAggregateKeys {
    int monsterEffectiveness = 8209;
    int itemRarity = 8206;
    int packSize = 8207;
    int monsterRarity = 8208;
    int waystoneDropChance = 8210;

    int For(const std::string& statId) const {
        if (statId == WaystoneHelper::StatIds::MonsterEffectiveness) return monsterEffectiveness;
        if (statId == WaystoneHelper::StatIds::ItemRarity)           return itemRarity;
        if (statId == WaystoneHelper::StatIds::PackSize)             return packSize;
        if (statId == WaystoneHelper::StatIds::MonsterRarity)        return monsterRarity;
        if (statId == WaystoneHelper::StatIds::WaystoneDropChance)   return waystoneDropChance;
        return 0;
    }
};

struct Settings {
    bool enabled = true;
    bool overlayEnabled = true;

    bool highlightImportantAffixes = true;
    bool highlightMonsterEffectiveness = true;
    bool highlightItemRarity = true;
    bool highlightMonsterPackSize = false;
    bool highlightMonsterRarity = false;
    bool highlightWaystoneDropChance = false;

    bool showAffixCountBadge = true;

    bool enableAffixGroups = true;
    bool showAffixGroupBadges = true;

    bool  borderEnabled = true;

    bool hideWhenHovered = true;
    bool showHoverBreakdown = true;

    bool readMods = true;
    bool debugMode = false;

    int   toggleOverlayVk = 0;

    int   scanIntervalMs = 650;
    int   targetAffixCount = 8;
    int   borderThickness = 3;
    float badgeScale = 0.9f;

    float affixCountBadgeColor[4]        = {0.0f, 0.749f, 1.0f, 1.0f};
    float monsterEffectivenessColor[4]   = {1.0f, 0.0f,   0.0f, 1.0f};
    float itemRarityColor[4]             = {1.0f, 0.647f, 0.0f, 1.0f};
    float monsterPackSizeColor[4]        = {0.196f, 0.804f, 0.196f, 1.0f};
    float monsterRarityColor[4]          = {0.0f, 0.749f, 1.0f, 1.0f};
    float waystoneDropChanceColor[4]     = {1.0f, 0.843f, 0.0f, 1.0f};
    float badgeBackgroundColor[4]        = {0.0f, 0.0f, 0.0f, 0.863f};
    float badgeTextColor[4]              = {1.0f, 1.0f, 1.0f, 1.0f};

    StatAggregateKeys statAggregateKeys;

    std::vector<AffixGroup> affixGroups;
    std::vector<BorderRule> borderRules;

    long nextId = 1;

    std::string MakeId(const char* prefix) {
        return std::string(prefix) + std::to_string(nextId++);
    }

    const float* StatColor(const std::string& statId) const {
        if (statId == WaystoneHelper::StatIds::MonsterEffectiveness) return monsterEffectivenessColor;
        if (statId == WaystoneHelper::StatIds::ItemRarity)           return itemRarityColor;
        if (statId == WaystoneHelper::StatIds::PackSize)             return monsterPackSizeColor;
        if (statId == WaystoneHelper::StatIds::MonsterRarity)        return monsterRarityColor;
        if (statId == WaystoneHelper::StatIds::WaystoneDropChance)   return waystoneDropChanceColor;
        return badgeTextColor;
    }

    bool StatDisplayEnabled(const std::string& statId) const {
        if (statId == WaystoneHelper::StatIds::MonsterEffectiveness) return highlightMonsterEffectiveness;
        if (statId == WaystoneHelper::StatIds::ItemRarity)           return highlightItemRarity;
        if (statId == WaystoneHelper::StatIds::PackSize)             return highlightMonsterPackSize;
        if (statId == WaystoneHelper::StatIds::MonsterRarity)        return highlightMonsterRarity;
        if (statId == WaystoneHelper::StatIds::WaystoneDropChance)   return highlightWaystoneDropChance;
        return false;
    }

    const AffixGroup* FindGroup(const std::string& id) const {
        for (const auto& g : affixGroups) if (g.id == id) return &g;
        return nullptr;
    }

    std::filesystem::path SettingsPath(const std::filesystem::path& pluginDir) const {
        return pluginDir / "config" / "settings.json";
    }

    void Load(const std::filesystem::path& pluginDir) {
        try {
            const auto path = SettingsPath(pluginDir);
            if (!std::filesystem::exists(path)) return;
            std::ifstream in(path);
            if (!in.is_open()) return;

            nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.is_object()) return;

            enabled = j.value("enabled", enabled);
            overlayEnabled = j.value("overlay_enabled", overlayEnabled);
            highlightImportantAffixes = j.value("highlight_important_affixes", highlightImportantAffixes);
            highlightMonsterEffectiveness = j.value("highlight_monster_effectiveness", highlightMonsterEffectiveness);
            highlightItemRarity = j.value("highlight_item_rarity", highlightItemRarity);
            highlightMonsterPackSize = j.value("highlight_monster_pack_size", highlightMonsterPackSize);
            highlightMonsterRarity = j.value("highlight_monster_rarity", highlightMonsterRarity);
            highlightWaystoneDropChance = j.value("highlight_waystone_drop_chance", highlightWaystoneDropChance);
            showAffixCountBadge = j.value("show_affix_count_badge", showAffixCountBadge);
            enableAffixGroups = j.value("enable_affix_groups", enableAffixGroups);
            showAffixGroupBadges = j.value("show_affix_group_badges", showAffixGroupBadges);
            borderEnabled = j.value("border_enabled", borderEnabled);
            hideWhenHovered = j.value("hide_when_hovered", hideWhenHovered);
            showHoverBreakdown = j.value("show_hover_breakdown", showHoverBreakdown);
            readMods = j.value("read_mods", readMods);
            debugMode = j.value("debug_mode", debugMode);
            toggleOverlayVk = std::clamp(j.value("toggle_overlay_vk", toggleOverlayVk), 0, 254);

            scanIntervalMs = std::clamp(j.value("scan_interval_ms", scanIntervalMs),
                                        kScanIntervalMinMs, kScanIntervalMaxMs);
            targetAffixCount = std::clamp(j.value("target_affix_count", targetAffixCount),
                                          kTargetAffixMin, kTargetAffixMax);
            borderThickness = std::clamp(j.value("border_thickness", borderThickness),
                                         kBorderThicknessMin, kBorderThicknessMax);
            badgeScale = std::clamp(j.value("badge_scale", badgeScale),
                                    kBadgeScaleMin, kBadgeScaleMax);

            if (j.contains("colors") && j["colors"].is_object()) {
                const auto& c = j["colors"];
                ColorFromJson(c.value("affix_count", nlohmann::json()), affixCountBadgeColor);
                ColorFromJson(c.value("monster_effectiveness", nlohmann::json()), monsterEffectivenessColor);
                ColorFromJson(c.value("item_rarity", nlohmann::json()), itemRarityColor);
                ColorFromJson(c.value("monster_pack_size", nlohmann::json()), monsterPackSizeColor);
                ColorFromJson(c.value("monster_rarity", nlohmann::json()), monsterRarityColor);
                ColorFromJson(c.value("waystone_drop_chance", nlohmann::json()), waystoneDropChanceColor);
                ColorFromJson(c.value("badge_background", nlohmann::json()), badgeBackgroundColor);
                ColorFromJson(c.value("badge_text", nlohmann::json()), badgeTextColor);
            }

            if (j.contains("stat_aggregate_keys") && j["stat_aggregate_keys"].is_object()) {
                const auto& k = j["stat_aggregate_keys"];
                statAggregateKeys.monsterEffectiveness = k.value("monster_effectiveness", statAggregateKeys.monsterEffectiveness);
                statAggregateKeys.itemRarity = k.value("item_rarity", statAggregateKeys.itemRarity);
                statAggregateKeys.packSize = k.value("pack_size", statAggregateKeys.packSize);
                statAggregateKeys.monsterRarity = k.value("monster_rarity", statAggregateKeys.monsterRarity);
                statAggregateKeys.waystoneDropChance = k.value("waystone_drop_chance", statAggregateKeys.waystoneDropChance);
            }

            LoadAffixGroups(j);

            if (j.contains("border_rules") && j["border_rules"].is_array())
                LoadBorderRules(j);
            else
                MigrateLegacyBorder(j);

            nextId = j.value("next_id", nextId);
            if (nextId < 1) nextId = 1;
            EnsureIds();
        } catch (...) {
        }
    }

    void Save(const std::filesystem::path& pluginDir) const {
        try {
            std::error_code ec;
            std::filesystem::create_directories(pluginDir / "config", ec);

            nlohmann::json j;
            j["enabled"] = enabled;
            j["overlay_enabled"] = overlayEnabled;
            j["highlight_important_affixes"] = highlightImportantAffixes;
            j["highlight_monster_effectiveness"] = highlightMonsterEffectiveness;
            j["highlight_item_rarity"] = highlightItemRarity;
            j["highlight_monster_pack_size"] = highlightMonsterPackSize;
            j["highlight_monster_rarity"] = highlightMonsterRarity;
            j["highlight_waystone_drop_chance"] = highlightWaystoneDropChance;
            j["show_affix_count_badge"] = showAffixCountBadge;
            j["enable_affix_groups"] = enableAffixGroups;
            j["show_affix_group_badges"] = showAffixGroupBadges;
            j["border_enabled"] = borderEnabled;
            j["hide_when_hovered"] = hideWhenHovered;
            j["show_hover_breakdown"] = showHoverBreakdown;
            j["read_mods"] = readMods;
            j["debug_mode"] = debugMode;
            j["toggle_overlay_vk"] = toggleOverlayVk;
            j["scan_interval_ms"] = scanIntervalMs;
            j["target_affix_count"] = targetAffixCount;
            j["border_thickness"] = borderThickness;
            j["badge_scale"] = badgeScale;

            nlohmann::json colors;
            colors["affix_count"] = ColorToJson(affixCountBadgeColor);
            colors["monster_effectiveness"] = ColorToJson(monsterEffectivenessColor);
            colors["item_rarity"] = ColorToJson(itemRarityColor);
            colors["monster_pack_size"] = ColorToJson(monsterPackSizeColor);
            colors["monster_rarity"] = ColorToJson(monsterRarityColor);
            colors["waystone_drop_chance"] = ColorToJson(waystoneDropChanceColor);
            colors["badge_background"] = ColorToJson(badgeBackgroundColor);
            colors["badge_text"] = ColorToJson(badgeTextColor);
            j["colors"] = std::move(colors);

            nlohmann::json keys;
            keys["monster_effectiveness"] = statAggregateKeys.monsterEffectiveness;
            keys["item_rarity"] = statAggregateKeys.itemRarity;
            keys["pack_size"] = statAggregateKeys.packSize;
            keys["monster_rarity"] = statAggregateKeys.monsterRarity;
            keys["waystone_drop_chance"] = statAggregateKeys.waystoneDropChance;
            j["stat_aggregate_keys"] = std::move(keys);

            nlohmann::json garr = nlohmann::json::array();
            for (const auto& g : affixGroups) {
                nlohmann::json e;
                e["id"] = g.id;
                e["name"] = g.name;
                e["enabled"] = g.enabled;
                e["min_matched"] = g.minMatched;
                e["color"] = ColorToJson(g.color);
                e["selected_affix_ids"] = g.selectedAffixIds;
                garr.push_back(std::move(e));
            }
            j["affix_groups"] = std::move(garr);

            nlohmann::json barr = nlohmann::json::array();
            for (const auto& g : borderRules) {
                nlohmann::json e;
                e["id"] = g.id;
                e["name"] = g.name;
                e["enabled"] = g.enabled;
                e["color"] = ColorToJson(g.color);
                nlohmann::json sm = nlohmann::json::object();
                for (const auto& kv : g.statMins)
                    if (kv.second > 0) sm[kv.first] = kv.second;
                e["stat_mins"] = std::move(sm);
                e["min_tier"] = g.minTier;
                e["min_affixes"] = g.minAffixes;
                e["corrupted"] = g.corrupted;
                e["rarity"] = g.rarity;
                barr.push_back(std::move(e));
            }
            j["border_rules"] = std::move(barr);

            j["next_id"] = nextId;

            const std::string text = j.dump(2);
            std::ofstream out(SettingsPath(pluginDir));
            if (out.is_open()) out << text;
        } catch (...) {
        }
    }

private:
    void LoadAffixGroups(const nlohmann::json& j) {
        if (!j.contains("affix_groups") || !j["affix_groups"].is_array()) return;
        affixGroups.clear();
        for (const auto& e : j["affix_groups"]) {
            if (!e.is_object()) continue;
            AffixGroup g;
            g.id = e.value("id", std::string());
            g.name = e.value("name", std::string("New Group"));
            if (g.name.size() > kMaxNameLen) g.name.resize(kMaxNameLen);
            g.enabled = e.value("enabled", true);
            g.minMatched = std::clamp(e.value("min_matched", 1), 1, kMinMatchedAffixMax);
            ColorFromJson(e.value("color", nlohmann::json()), g.color);
            g.selectedAffixIds = StringVecFromJson(e.value("selected_affix_ids", nlohmann::json()));
            affixGroups.push_back(std::move(g));
        }
    }

    void LoadBorderRules(const nlohmann::json& j) {
        borderRules.clear();
        for (const auto& e : j["border_rules"]) {
            if (!e.is_object()) continue;
            BorderRule g;
            g.id = e.value("id", std::string());
            g.name = e.value("name", std::string("New Group"));
            if (g.name.size() > kMaxNameLen) g.name.resize(kMaxNameLen);
            g.enabled = e.value("enabled", true);
            ColorFromJson(e.value("color", nlohmann::json()), g.color);
            if (e.contains("stat_mins") && e["stat_mins"].is_object()) {
                for (auto it = e["stat_mins"].begin(); it != e["stat_mins"].end(); ++it) {
                    if (!it.value().is_number()) continue;
                    const int v = std::clamp(it.value().get<int>(), 0, kStatThresholdMax);
                    if (v > 0) g.statMins[it.key()] = v;
                }
            }
            g.minTier = std::clamp(e.value("min_tier", 0), 0, kMinTierMax);
            g.minAffixes = std::clamp(e.value("min_affixes", 0), 0, kTargetAffixMax);
            g.corrupted = std::clamp(e.value("corrupted", kCorruptedAny),
                                     kCorruptedAny, kCorruptedNever);
            g.rarity = std::clamp(e.value("rarity", -1), -1, 3);
            borderRules.push_back(std::move(g));
        }
    }

    void MigrateLegacyBorder(const nlohmann::json& j) {
        struct LegacyMin { const char* key; const char* statId; };
        static const LegacyMin kLegacy[] = {
            {"min_monster_effectiveness", WaystoneHelper::StatIds::MonsterEffectiveness},
            {"min_item_rarity",           WaystoneHelper::StatIds::ItemRarity},
            {"min_monster_pack_size",     WaystoneHelper::StatIds::PackSize},
            {"min_monster_rarity",        WaystoneHelper::StatIds::MonsterRarity},
            {"min_waystone_drop_chance",  WaystoneHelper::StatIds::WaystoneDropChance},
        };

        BorderRule g;
        bool any = false;
        for (const auto& l : kLegacy) {
            const int v = std::clamp(j.value(l.key, 0), 0, kStatThresholdMax);
            if (v > 0) { g.statMins[l.statId] = v; any = true; }
        }
        g.minTier = std::clamp(j.value("border_min_tier", 0), 0, kMinTierMax);
        if (g.minTier > 0) any = true;
        if (j.value("border_require_affix_count", false)) {
            g.minAffixes = std::clamp(j.value("target_affix_count", targetAffixCount),
                                      kTargetAffixMin, kTargetAffixMax);
            if (g.minAffixes > 0) any = true;
        }
        if (!any) return;

        if (j.contains("colors") && j["colors"].is_object())
            ColorFromJson(j["colors"].value("border", nlohmann::json()), g.color);
        g.name = "Border";
        borderRules.push_back(std::move(g));
    }

    void EnsureIds() {
        for (auto& g : affixGroups)
            if (g.id.empty()) g.id = MakeId("g");
        for (auto& g : borderRules)
            if (g.id.empty()) g.id = MakeId("b");
    }
};

}
