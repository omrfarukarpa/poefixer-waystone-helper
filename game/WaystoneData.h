#pragma once

#include "WaystoneTypes.h"
#include "../third_party/json.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace WaystoneHelper {

namespace StatIds {
inline constexpr const char* MonsterEffectiveness = "map_monster_potency_+%_final_from_map";
inline constexpr const char* ItemRarity           = "map_item_drop_rarity_+%_final_from_map";
inline constexpr const char* PackSize             = "map_pack_size_+%_final_from_map";
inline constexpr const char* MonsterRarity        =
    "map_number_of_magic_and_rare_packs_+%_final_and_rare_monster_modifiers_chance_+%_final_from_map";
inline constexpr const char* WaystoneDropChance   = "map_map_item_drop_chance_+%_final_from_map";
}

struct GeneratedStat {
    std::string statId;
    std::string displayName;
    std::string badgeLabel;
};

struct Affix {
    std::string Id;
    std::string Label;
    std::string Category;
    std::vector<std::string> ModIds;
    std::unordered_set<std::string> NormModIds;

    std::string ShortLabel() const {
        const auto pos = Label.find(" / ");
        return pos == std::string::npos ? Label : Label.substr(0, pos);
    }
};

class WaystoneData {
public:
    std::string Load(const std::filesystem::path& pluginDir) {
        m_affixes.clear();
        m_affixIndex.clear();
        m_modStatIds.clear();
        m_generatedStats = DefaultGeneratedStats();

        try {
            const auto path = pluginDir / "config" / "waystone_data.json";
            if (!std::filesystem::exists(path))
                return "waystone_data.json not found — affix-count only (stats/groups disabled).";

            std::ifstream in(path);
            if (!in.is_open())
                return "waystone_data.json could not be opened — affix-count only.";

            nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.is_object())
                return "waystone_data.json is corrupt — affix-count only.";

            LoadGeneratedStats(j);
            LoadAffixes(j);
            LoadModStatIds(j);

            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "Loaded %zu affix families and %zu tracked mods from waystone_data.json.",
                          m_affixes.size(), m_modStatIds.size());
            return buf;
        } catch (...) {
            return "Failed to load waystone_data.json — affix-count only.";
        }
    }

    const std::vector<GeneratedStat>& GeneratedStats() const { return m_generatedStats; }
    const std::vector<Affix>& Affixes() const { return m_affixes; }
    std::size_t TrackedModCount() const { return m_modStatIds.size(); }

    const Affix* FindAffix(const std::string& id) const {
        auto it = m_affixIndex.find(NormalizeIdentifier(id));
        return it == m_affixIndex.end() ? nullptr : &m_affixes[it->second];
    }

    static bool AffixMatchesMod(const Affix& a, const std::string& normId,
                                const std::string& normName) {
        if (!normId.empty() && a.NormModIds.count(normId)) return true;
        if (!normName.empty() && a.NormModIds.count(normName)) return true;
        return false;
    }

    const std::vector<std::string>* StatIdsForMod(const std::string& normId) const {
        auto it = m_modStatIds.find(normId);
        return it == m_modStatIds.end() ? nullptr : &it->second;
    }

    std::string BadgeLabelFor(const std::string& statId) const {
        for (const auto& g : m_generatedStats)
            if (g.statId == statId) return g.badgeLabel;
        return {};
    }

    bool IsTrackedStat(const std::string& statId) const {
        for (const auto& g : m_generatedStats)
            if (g.statId == statId) return true;
        return false;
    }

private:
    std::vector<GeneratedStat> m_generatedStats;
    std::vector<Affix> m_affixes;
    std::unordered_map<std::string, std::size_t> m_affixIndex;
    std::unordered_map<std::string, std::vector<std::string>> m_modStatIds;

    static std::vector<GeneratedStat> DefaultGeneratedStats() {
        return {
            {StatIds::MonsterEffectiveness, "Monster Effectiveness", "E"},
            {StatIds::ItemRarity,           "Item Rarity",           "R"},
            {StatIds::PackSize,             "Monster Pack Size",     "P"},
            {StatIds::MonsterRarity,        "Monster Rarity",        "MR"},
            {StatIds::WaystoneDropChance,   "Waystone Drop Chance",  "W"},
        };
    }

    void LoadGeneratedStats(const nlohmann::json& j) {
        if (!j.contains("generatedStats") || !j["generatedStats"].is_array()) return;
        std::vector<GeneratedStat> loaded;
        for (const auto& e : j["generatedStats"]) {
            if (!e.is_object()) continue;
            GeneratedStat g;
            g.statId      = e.value("statId", std::string());
            g.displayName = e.value("displayName", std::string());
            g.badgeLabel  = e.value("badgeLabel", std::string());
            if (!g.statId.empty() && !g.badgeLabel.empty())
                loaded.push_back(std::move(g));
        }
        if (!loaded.empty()) m_generatedStats = std::move(loaded);
    }

    void LoadAffixes(const nlohmann::json& j) {
        if (!j.contains("affixes") || !j["affixes"].is_array()) return;
        for (const auto& e : j["affixes"]) {
            if (!e.is_object()) continue;
            Affix a;
            a.Id       = e.value("id", std::string());
            a.Label    = e.value("label", std::string());
            a.Category = e.value("category", std::string());
            if (a.Id.empty()) continue;
            if (e.contains("modIds") && e["modIds"].is_array()) {
                for (const auto& m : e["modIds"]) {
                    if (!m.is_string()) continue;
                    std::string raw = m.get<std::string>();
                    if (raw.empty()) continue;
                    a.ModIds.push_back(raw);
                    a.NormModIds.insert(NormalizeIdentifier(raw));
                }
            }
            const std::string normId = NormalizeIdentifier(a.Id);
            if (m_affixIndex.count(normId)) continue;
            m_affixIndex.emplace(normId, m_affixes.size());
            m_affixes.push_back(std::move(a));
        }
    }

    void LoadModStatIds(const nlohmann::json& j) {
        if (!j.contains("modStatIdsById") || !j["modStatIdsById"].is_object()) return;
        for (auto it = j["modStatIdsById"].begin(); it != j["modStatIdsById"].end(); ++it) {
            if (!it.value().is_array()) continue;
            std::vector<std::string> stats;
            for (const auto& s : it.value()) {
                if (s.is_string()) stats.push_back(s.get<std::string>());
                else stats.push_back(std::string());
            }
            while (!stats.empty() && stats.back().empty()) stats.pop_back();
            if (stats.empty()) continue;
            m_modStatIds.emplace(NormalizeIdentifier(it.key()), std::move(stats));
        }
    }
};

}
