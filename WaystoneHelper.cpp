#include "sdk/PluginSDK.h"

#include "config/Settings.h"
#include "game/BadgeRenderer.h"
#include "game/PanelDetector.h"
#include "game/WaystoneData.h"
#include "game/WaystoneScanner.h"
#include "game/WaystoneScore.h"
#include "game/WaystoneTypes.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

inline constexpr const char* kWaystoneHelperVersion    = "1.2.0";
inline constexpr const char* kWaystoneHelperMaintainer = "Omer Faruk ARPA";

using WaystoneHelperConfig::Settings;

class WaystoneHelperPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "Waystone Helper"; }

    bool WantsOverlay() const override {
        return m_settings.enabled && m_settings.overlayEnabled;
    }

    void OnEnable(bool) override {
        if (!HostCompatible()) {
            ctx()->Log.Error(
                "Waystone Helper: incompatible PoeFixer host (SDK version/size mismatch) — disabled");
            return;
        }
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        m_settings.Load(DirectoryPath());
        m_dataStatus = m_data.Load(DirectoryPath());
        ctx()->Log.Info(("Waystone Helper: " + m_dataStatus).c_str());
        m_lastScan = std::chrono::steady_clock::now()
                     - std::chrono::milliseconds(m_settings.scanIntervalMs);
        ctx()->Log.Info("Waystone Helper plugin enabled");
    }

    void OnDisable() override {
        m_scanner.Reset();
        m_visible.clear();
        m_draw.clear();
        SaveSettings();
        ctx()->Log.Info("Waystone Helper plugin disabled");
    }

    void DrawUI() override {
        if (!m_settings.enabled || !m_settings.overlayEnabled) return;
        if (!ctx()->Game.IsInGame()) return;
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        if (!ctx()->Game.IsForeground()) return;

        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        RefreshIfNeeded(disp.x, disp.y);

        const ImVec2 mouse = ImGui::GetMousePos();
        const WaystoneHelper::VisibleWaystone* hovered = FindHovered(mouse);
        if (hovered) {
            m_debugAddr = hovered->address;
            m_debugName = hovered->name;
            m_debugPath = hovered->path;
        }
        const std::uintptr_t hoveredAddr = hovered ? hovered->address : 0;

        for (const auto& d : m_draw) {
            if (m_settings.hideWhenHovered && d.address == hoveredAddr) continue;
            WaystoneHelper::RenderWaystone(d.rect, d.score, m_settings);
        }

        if (m_settings.showHoverBreakdown && hovered && hovered->parsed) {
            WaystoneHelper::Score e =
                WaystoneHelper::Evaluate(m_settings, m_data, *hovered->parsed);
            if (WaystoneHelper::HasAnythingToShow(e))
                WaystoneHelper::DrawHoverBreakdown(hovered->rect, hovered->name, e, m_settings);
        }
    }

    void DrawSettings() override {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        ImGui::TextDisabled("Waystone Helper v%s  -  by %s",
                            kWaystoneHelperVersion, kWaystoneHelperMaintainer);
        ImGui::Checkbox("Enable Waystone Helper", &m_settings.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Show overlay", &m_settings.overlayEnabled);

        if (ImGui::CollapsingHeader("How to use")) {
            ImGui::TextWrapped(
                "Waystone Helper highlights maps in any open item window (inventory, "
                "stash, guild stash). The top-left badge shows the explicit affix count "
                "when it reaches the target; top-right badges show selected map stats "
                "(E/R/P/MR/W); left-side badges show custom affix-group matches; the "
                "border color comes from the first matching border rule.");
        }

        DrawStatSettings();
        DrawBorderSettings();
        DrawGeneralDisplaySettings();
        DrawDetectionSettings();
        DrawAffixGroupSettings();
        DrawDebugSettings();
    }

    void SaveSettings() override { m_settings.Save(DirectoryPath()); }

private:
    struct DrawItem {
        std::uintptr_t address = 0;
        WaystoneHelper::ScreenRect rect;
        WaystoneHelper::Score score;
    };

    Settings m_settings;
    WaystoneHelper::WaystoneData m_data;
    std::string m_dataStatus;
    WaystoneHelper::WaystoneScanner m_scanner;
    std::vector<WaystoneHelper::VisibleWaystone> m_visible;
    std::vector<DrawItem> m_draw;
    std::chrono::steady_clock::time_point m_lastScan{};

    std::unordered_map<std::string, std::string> m_affixSearch;

    std::uintptr_t m_debugAddr = 0;
    std::string m_debugName;
    std::string m_debugPath;

    int m_lastVisibleCount = 0;
    int m_lastMatchedCount = 0;

    void RefreshIfNeeded(float w, float h) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastScan).count();
        if (elapsed < m_settings.scanIntervalMs) return;
        m_lastScan = now;

        m_visible = m_scanner.Scan(ctx(), m_data, m_settings, w, h);

        m_draw.clear();
        m_draw.reserve(m_visible.size());
        for (const auto& vw : m_visible) {
            if (!vw.parsed) continue;
            WaystoneHelper::Score e = WaystoneHelper::Evaluate(m_settings, m_data, *vw.parsed);
            if (!e.matched) continue;
            m_draw.push_back(DrawItem{vw.address, vw.rect, std::move(e)});
        }
        m_lastVisibleCount = static_cast<int>(m_visible.size());
        m_lastMatchedCount = static_cast<int>(m_draw.size());

        if (m_settings.debugMode && !m_visible.empty())
            WriteAutoDump();
    }

    void WriteAutoDump() {
        std::string all = "[Waystone Helper] auto dump (debug mode)\n";
        char hdr[96];
        std::snprintf(hdr, sizeof(hdr), "visible waystones: %zu\n\n", m_visible.size());
        all += hdr;
        int n = 0;
        for (const auto& vw : m_visible) {
            if (n++ >= 8) break;
            all += "==================================================\n";
            all += WaystoneHelper::WaystoneScanner::BuildDebugDump(
                ctx(), vw.address, vw.name, vw.path);
            all += "\n";
        }
        try {
            const auto dir = DirectoryPath() / "debug";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            std::ofstream out(dir / "waystone-dump.txt");
            if (out.is_open()) out << all;
        } catch (...) {}
    }

    const WaystoneHelper::VisibleWaystone* FindHovered(const ImVec2& mouse) const {
        for (const auto& vw : m_visible) {
            const auto& r = vw.rect;
            if (mouse.x >= r.x && mouse.x <= r.x + r.w
                && mouse.y >= r.y && mouse.y <= r.y + r.h)
                return &vw;
        }
        return nullptr;
    }

    static void HelpMarker(const char* desc) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", desc);
    }

    static void ColorEdit(const char* label, float c[4]) {
        ImGui::ColorEdit4(label, c,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    }

    static bool StringSelected(const std::vector<std::string>& v, const std::string& id) {
        for (const auto& s : v) if (s == id) return true;
        return false;
    }

    static void ToggleString(std::vector<std::string>& v, const std::string& id, bool on) {
        if (on) {
            if (!StringSelected(v, id)) v.push_back(id);
        } else {
            v.erase(std::remove(v.begin(), v.end(), id), v.end());
        }
    }

    void DrawStatSettings() {
        ImGui::SeparatorText("Map stats");
        ImGui::Checkbox("Show selected map-stat badges", &m_settings.highlightImportantAffixes);
        HelpMarker("Checkbox = show the badge. The 'min' box = require this stat for the "
                   "border: set e.g. 30 to only border maps with that stat >= 30% "
                   "(0 = badge only, no border condition). Turn on 'Draw border' below.");
        ImGui::Indent();
        DrawStatRow("Monster Effectiveness (E)", WaystoneHelper::StatIds::MonsterEffectiveness,
                    &m_settings.highlightMonsterEffectiveness, m_settings.monsterEffectivenessColor);
        DrawStatRow("Item Rarity (R)", WaystoneHelper::StatIds::ItemRarity,
                    &m_settings.highlightItemRarity, m_settings.itemRarityColor);
        DrawStatRow("Monster Pack Size (P)", WaystoneHelper::StatIds::PackSize,
                    &m_settings.highlightMonsterPackSize, m_settings.monsterPackSizeColor);
        DrawStatRow("Monster Rarity (MR)", WaystoneHelper::StatIds::MonsterRarity,
                    &m_settings.highlightMonsterRarity, m_settings.monsterRarityColor);
        DrawStatRow("Waystone Drop Chance (W)", WaystoneHelper::StatIds::WaystoneDropChance,
                    &m_settings.highlightWaystoneDropChance, m_settings.waystoneDropChanceColor);
        ImGui::Unindent();
    }

    void DrawStatRow(const char* label, const char* statId, bool* enabled, float color[4]) {
        ImGui::PushID(label);
        ImGui::Checkbox(label, enabled);
        ImGui::SameLine();
        ColorEdit("##col", color);
        ImGui::SameLine();
        int* mn = m_settings.StatMinPtr(statId);
        if (mn) {
            ImGui::SetNextItemWidth(64.f);
            if (ImGui::InputInt("min >=%", mn, 0, 0))
                *mn = std::clamp(*mn, 0, WaystoneHelperConfig::kStatThresholdMax);
        }
        ImGui::PopID();
    }

    void DrawGeneralDisplaySettings() {
        ImGui::SeparatorText("Affix count & display");
        ImGui::Checkbox("Show affix-count badge", &m_settings.showAffixCountBadge);
        HelpMarker("Top-left badge when the waystone has at least the target affix count. "
                   "Set the target to 0/1/2 to also flag white/magic maps.");
        ImGui::SameLine();
        ColorEdit("Count color", m_settings.affixCountBadgeColor);

        ImGui::SliderInt("Target affix count", &m_settings.targetAffixCount,
                         WaystoneHelperConfig::kTargetAffixMin,
                         WaystoneHelperConfig::kTargetAffixMax);
        ImGui::SliderFloat("Badge scale", &m_settings.badgeScale,
                           WaystoneHelperConfig::kBadgeScaleMin,
                           WaystoneHelperConfig::kBadgeScaleMax, "%.2f");
        ImGui::Checkbox("Hide overlay on the hovered item", &m_settings.hideWhenHovered);
        HelpMarker("Skip badges/border on the item under the cursor so the game tooltip "
                   "stays readable.");
        ImGui::Checkbox("Show hover breakdown panel", &m_settings.showHoverBreakdown);
    }

    void DrawDetectionSettings() {
        ImGui::SeparatorText("Detection");
        ImGui::Checkbox("Read item mods (affix count + stats + groups)", &m_settings.readMods);
        if (m_settings.readMods) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 190, 90, 255));
            ImGui::TextWrapped(
                "Reads each waystone's mods for the affix count, map stats and affix "
                "groups. If waystones ever crash the game, turn this off (or set "
                "\"read_mods\": false in config/settings.json).");
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Mods off: nothing is highlighted (affix count and stats "
                                "are unavailable).");
        }
        ImGui::SliderInt("Scan interval (ms)", &m_settings.scanIntervalMs,
                         WaystoneHelperConfig::kScanIntervalMinMs,
                         WaystoneHelperConfig::kScanIntervalMaxMs);
    }

    void DrawAffixGroupSettings() {
        char header[64];
        std::snprintf(header, sizeof(header), "Affix Groups (%zu)###affix_groups",
                      m_settings.affixGroups.size());
        if (!ImGui::CollapsingHeader(header)) return;

        ImGui::Indent();
        ImGui::Checkbox("Enable affix group matching", &m_settings.enableAffixGroups);
        ImGui::Checkbox("Show affix group badges", &m_settings.showAffixGroupBadges);

        if (ImGui::Button("Add Group")) {
            WaystoneHelperConfig::AffixGroup g;
            g.id = m_settings.MakeId("g");
            g.name = "Group " + std::to_string(m_settings.affixGroups.size() + 1);
            m_settings.affixGroups.push_back(std::move(g));
        }
        ImGui::TextDisabled("Groups match selected affix families; matching groups draw a "
                            "colored count badge.");

        for (int i = 0; i < static_cast<int>(m_settings.affixGroups.size()); ++i)
            if (DrawAffixGroupEditor(i)) { --i; }

        ImGui::Unindent();
    }

    bool DrawAffixGroupEditor(int index) {
        auto& g = m_settings.affixGroups[index];
        ImGui::PushID(g.id.c_str());

        const int sel = static_cast<int>(g.selectedAffixIds.size());
        char label[128];
        std::snprintf(label, sizeof(label), "%s  [%d selected]###hdr", g.name.c_str(), sel);

        bool deleted = false;
        if (ImGui::TreeNode(label)) {
            ImGui::Checkbox("Enabled", &g.enabled);
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) deleted = true;

            char nameBuf[96];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", g.name.c_str());
            ImGui::SetNextItemWidth(240.f);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) g.name = nameBuf;

            ImGui::SetNextItemWidth(90.f);
            if (ImGui::InputInt("Min matched affixes", &g.minMatched))
                g.minMatched = std::clamp(g.minMatched, 1, WaystoneHelperConfig::kMinMatchedAffixMax);
            ColorEdit("Group color", g.color);

            DrawAffixSelectionList(g);
            ImGui::TreePop();
        }

        ImGui::PopID();
        if (deleted) m_settings.affixGroups.erase(m_settings.affixGroups.begin() + index);
        return deleted;
    }

    void DrawAffixSelectionList(WaystoneHelperConfig::AffixGroup& g) {
        std::string& search = m_affixSearch[g.id];
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s", search.c_str());
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputTextWithHint("##affixsearch", "filter affixes...", buf, sizeof(buf)))
            search = buf;
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) g.selectedAffixIds.clear();
        const std::string lf = WaystoneHelper::ToLowerCopy(search);

        ImGui::BeginChild("affixlist", ImVec2(0.f, 200.f), ImGuiChildFlags_Borders);
        std::string lastCategory;
        int shown = 0;
        for (const auto& a : m_data.Affixes()) {
            if (!lf.empty()
                && !WaystoneHelper::ContainsCI(a.ShortLabel(), lf)
                && !WaystoneHelper::ContainsCI(a.Id, lf)
                && !WaystoneHelper::ContainsCI(a.Category, lf))
                continue;
            if (a.Category != lastCategory) {
                lastCategory = a.Category;
                ImGui::SeparatorText(a.Category.c_str());
            }
            bool on = StringSelected(g.selectedAffixIds, a.Id);
            ImGui::PushID(a.Id.c_str());
            if (ImGui::Checkbox(a.ShortLabel().c_str(), &on))
                ToggleString(g.selectedAffixIds, a.Id, on);
            if (ImGui::IsItemHovered() && a.Label != a.ShortLabel())
                ImGui::SetTooltip("%s", a.Label.c_str());
            ImGui::PopID();
            ++shown;
        }
        if (shown == 0) ImGui::TextDisabled("No affixes match the filter.");
        ImGui::EndChild();
    }

    void DrawBorderSettings() {
        ImGui::SeparatorText("Border");
        ImGui::Checkbox("Draw border for matching maps", &m_settings.borderEnabled);
        HelpMarker("A border is drawn when the map meets ALL active conditions below: "
                   "every map-stat with a 'min' set (above), the min tier, and — if "
                   "checked — the target affix count. No condition set = no border.");
        ImGui::SameLine();
        ColorEdit("Border color", m_settings.borderColor);
        ImGui::SliderInt("Border thickness", &m_settings.borderThickness,
                         WaystoneHelperConfig::kBorderThicknessMin,
                         WaystoneHelperConfig::kBorderThicknessMax);

        char tac[64];
        std::snprintf(tac, sizeof(tac), "Require target affix count (%d+)",
                      m_settings.targetAffixCount);
        ImGui::Checkbox(tac, &m_settings.borderRequireAffixCount);
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::InputInt("Min tier (0=off)", &m_settings.borderMinTier, 0, 0))
            m_settings.borderMinTier = std::clamp(m_settings.borderMinTier, 0,
                                                  WaystoneHelperConfig::kMinTierMax);
    }

    void DrawDebugSettings() {
        if (!ImGui::CollapsingHeader("Debug / Data")) return;
        ImGui::Indent();
        ImGui::TextDisabled("%s", m_dataStatus.c_str());
        ImGui::TextDisabled("Last scan: visible %d, matched %d",
                            m_lastVisibleCount, m_lastMatchedCount);
        ImGui::Checkbox("Debug mode", &m_settings.debugMode);
        if (m_settings.debugMode) {
            ImGui::TextWrapped(
                "Writes debug/waystone-dump.txt each scan with every mod and every "
                "aggregated {key,value} pair, for discovering numeric stat keys.");
            if (ImGui::Button("Dump last hovered waystone stats"))
                DumpDebug();
            if (m_debugAddr)
                ImGui::TextDisabled("Last hovered: %s", m_debugName.c_str());
            else
                ImGui::TextDisabled("Last hovered: none yet.");
        }
        ImGui::Unindent();
    }

    void DumpDebug() {
        if (!m_debugAddr) {
            ctx()->Log.Info("[Waystone Helper] no hovered waystone to dump — hover one first.");
            return;
        }
        const std::string dump = WaystoneHelper::WaystoneScanner::BuildDebugDump(
            ctx(), m_debugAddr, m_debugName, m_debugPath);

        std::string filePath;
        try {
            const auto dir = DirectoryPath() / "debug";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            const auto path = dir / "waystone-dump.txt";
            std::ofstream out(path);
            if (out.is_open()) out << dump;
            filePath = path.string();
        } catch (...) {}

        std::size_t start = 0;
        while (start < dump.size()) {
            std::size_t nl = dump.find('\n', start);
            if (nl == std::string::npos) nl = dump.size();
            if (nl > start)
                ctx()->Log.Info(dump.substr(start, nl - start).c_str());
            start = nl + 1;
        }
        if (!filePath.empty())
            ctx()->Log.Info(("[Waystone Helper] dump written to " + filePath).c_str());
    }
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new WaystoneHelperPlugin(); }

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
