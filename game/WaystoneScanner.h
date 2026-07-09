#pragma once

#include "PanelDetector.h"
#include "WaystoneData.h"
#include "WaystoneTypes.h"
#include "../config/Settings.h"
#include "sdk/PluginSDK.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace WaystoneHelper {

struct ParsedWaystone {
    std::string sourcePath;
    int affixCount = 0;
    int tier = 0;
    int rarity = 0;
    bool identified = true;
    std::unordered_set<std::string> matchKeys;
    std::unordered_set<std::string> presentStatIds;
    std::unordered_map<std::string, int> statValues;
    bool modsRead = false;
};

struct VisibleWaystone {
    std::uintptr_t address = 0;
    ScreenRect rect;
    std::string name;
    std::string path;
    const ParsedWaystone* parsed = nullptr;
};

class WaystoneScanner {
public:
    std::vector<VisibleWaystone> Scan(const PluginSDK::Context* ctx,
                                      const WaystoneData& data,
                                      const WaystoneHelperConfig::Settings& settings,
                                      float displayW, float displayH,
                                      int maxNewParsesPerScan = 40) {
        std::vector<VisibleWaystone> out;
        if (!ctx) return out;

        if (m_cache.size() > 4000) m_cache.clear();

        ctx->Inventory.Scan(-1);
        const auto all = ctx->Inventory.GetAll();

        std::unordered_set<std::uintptr_t> seenThisScan;
        int newParses = 0;

        for (const auto& inv : all) {
            for (const auto& item : inv.Items) {
                if (!LooksLikeWaystone(item.Path, item.BaseTypeName)) continue;
                auto rect = ResolveItemRect(inv, item, displayW, displayH);
                if (!rect) continue;
                if (item.Address == 0) continue;
                if (!seenThisScan.insert(item.Address).second) continue;

                auto it = m_cache.find(item.Address);
                const bool freshAddress =
                    it == m_cache.end() || it->second.sourcePath != item.Path;

                if (freshAddress) {
                    const bool doMods = settings.readMods && newParses < maxNewParsesPerScan;
                    ParsedWaystone p = Parse(ctx, data, settings, item, doMods);
                    if (doMods && p.modsRead) ++newParses;
                    if (it == m_cache.end())
                        it = m_cache.emplace(item.Address, std::move(p)).first;
                    else
                        it->second = std::move(p);
                } else if (settings.readMods && !it->second.modsRead
                           && newParses < maxNewParsesPerScan) {
                    ParsedWaystone p = Parse(ctx, data, settings, item, true);
                    if (p.modsRead) { ++newParses; it->second = std::move(p); }
                }

                VisibleWaystone vw;
                vw.address = item.Address;
                vw.rect = *rect;
                vw.name = item.UniqueName.empty() ? item.BaseTypeName : item.UniqueName;
                vw.path = item.Path;
                vw.parsed = &it->second;
                out.push_back(std::move(vw));
            }
        }
        return out;
    }

    void Reset() { m_cache.clear(); }

    static std::string BuildDebugDump(const PluginSDK::Context* ctx, std::uintptr_t address,
                                      const std::string& baseName, const std::string& path) {
        std::string s;
        s += "[Waystone Helper] debug dump\n";
        s += "base='" + baseName + "' path='" + path + "'\n";
        if (!ctx || address == 0) { s += "no context / address\n"; return s; }

        std::string probe = ctx->Inventory.ReadItemBaseTypeName(address);
        if (probe.empty()) { s += "probe failed (address not live) - skipped\n"; return s; }

        const auto mods = ctx->Inventory.ReadItemMods(address);
        char line[512];
        std::snprintf(line, sizeof(line), "explicit mods: %zu (valid=%d)\n",
                      mods.ExplicitMods.size(), mods.Valid ? 1 : 0);
        s += line;
        for (const auto& m : mods.ExplicitMods) {
            std::string fmt = ctx->Inventory.FormatStat(m.StatKey, m.Value0, m.Value1);
            std::snprintf(line, sizeof(line),
                          "  id='%s' name='%s' stat='%s' v0=%.1f v1=%.1f :: %s\n",
                          m.Id.c_str(), m.Name.c_str(), m.StatKey.c_str(),
                          m.Value0, m.Value1, fmt.c_str());
            s += line;
        }

        const auto agg = ctx->Inventory.ReadItemAggregatedStats(address);
        std::snprintf(line, sizeof(line), "aggregated stats: %zu pairs {key,value}\n", agg.size());
        s += line;
        for (const auto& pr : agg) {
            std::snprintf(line, sizeof(line), "  key=%d value=%d\n", pr.first, pr.second);
            s += line;
        }
        return s;
    }

private:
    ParsedWaystone Parse(const PluginSDK::Context* ctx, const WaystoneData& data,
                         const WaystoneHelperConfig::Settings& settings,
                         const PluginSDK::InventoryItem& item, bool readMods) {
        ParsedWaystone p;
        p.sourcePath = item.Path;
        p.tier = ParseWaystoneTier(item.Path);
        p.rarity = item.Rarity;
        p.identified = item.IsIdentified;

        if (!readMods || item.Address == 0) return p;

        std::string probe = ctx->Inventory.ReadItemBaseTypeName(item.Address);
        if (probe.empty()) return p;

        const auto mods = ctx->Inventory.ReadItemMods(item.Address);
        p.modsRead = mods.Valid;
        p.affixCount = static_cast<int>(mods.ExplicitMods.size());

        for (const auto& m : mods.ExplicitMods) {
            const std::string normId = NormalizeIdentifier(m.Id);
            const std::string normName = NormalizeIdentifier(m.Name);
            if (!normId.empty()) p.matchKeys.insert(normId);
            if (!normName.empty()) p.matchKeys.insert(normName);

            AddStatKeyDirect(data, p, m);
            AddFromIdMap(data, p, normId, m);
        }

        AddFromAggregated(ctx, data, settings, p, item.Address);

        return p;
    }

    static void AddStatKeyDirect(const WaystoneData& data, ParsedWaystone& p,
                                 const PluginSDK::Mod& m) {
        if (m.StatKey.empty()) return;
        const std::string normStat = NormalizeIdentifier(m.StatKey);
        for (const auto& g : data.GeneratedStats()) {
            if (NormalizeIdentifier(g.statId) != normStat) continue;
            p.presentStatIds.insert(g.statId);
            int v = static_cast<int>(std::lround(std::fabs(m.Value0)));
            if (v > 0) Credit(p, g.statId, v);
            break;
        }
    }

    static void AddFromIdMap(const WaystoneData& data, ParsedWaystone& p,
                             const std::string& normId, const PluginSDK::Mod& m) {
        const auto* statIds = data.StatIdsForMod(normId);
        if (!statIds) return;
        for (std::size_t i = 0; i < statIds->size(); ++i) {
            const std::string& statId = (*statIds)[i];
            if (statId.empty() || !data.IsTrackedStat(statId)) continue;
            p.presentStatIds.insert(statId);
            int v = 0;
            if (i == 0) v = static_cast<int>(std::lround(std::fabs(m.Value0)));
            else if (i == 1) v = static_cast<int>(std::lround(std::fabs(m.Value1)));
            if (v > 0) Credit(p, statId, v);
        }
    }

    static void AddFromAggregated(const PluginSDK::Context* ctx, const WaystoneData& data,
                                  const WaystoneHelperConfig::Settings& settings,
                                  ParsedWaystone& p, std::uintptr_t addr) {
        const auto agg = ctx->Inventory.ReadItemAggregatedStats(addr);
        if (agg.empty()) return;
        for (const auto& g : data.GeneratedStats()) {
            const int key = settings.statAggregateKeys.For(g.statId);
            if (key == 0) continue;
            for (const auto& pr : agg) {
                if (pr.first != key) continue;
                int v = pr.second < 0 ? -pr.second : pr.second;
                if (v > 0) {
                    p.presentStatIds.insert(g.statId);
                    p.statValues[g.statId] = v;
                }
                break;
            }
        }
    }

    static void Credit(ParsedWaystone& p, const std::string& statId, int v) {
        int& cur = p.statValues[statId];
        if (v > cur) cur = v;
    }

    std::unordered_map<std::uintptr_t, ParsedWaystone> m_cache;
};

}
