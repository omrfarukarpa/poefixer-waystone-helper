#pragma once

#include "PanelDetector.h"
#include "WaystoneScore.h"
#include "../config/Settings.h"

#include <imgui.h>

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace WaystoneHelper {

inline ImU32 ColF(const float c[4]) {
    return IM_COL32(static_cast<int>(c[0] * 255.f + 0.5f),
                    static_cast<int>(c[1] * 255.f + 0.5f),
                    static_cast<int>(c[2] * 255.f + 0.5f),
                    static_cast<int>(c[3] * 255.f + 0.5f));
}

inline float BadgeFontSize(const WaystoneHelperConfig::Settings& s) {
    const float fs = ImGui::GetFontSize() * s.badgeScale;
    return fs > 8.f ? fs : 8.f;
}

inline ImVec2 MeasureBadgeText(const char* text, float fontSize) {
    ImFont* font = ImGui::GetFont();
    return font->CalcTextSizeA(fontSize, FLT_MAX, -1.f, text);
}

inline ImVec2 BadgeSize(const char* text, float fontSize) {
    const ImVec2 t = MeasureBadgeText(text, fontSize);
    const float w = t.x + 8.f, h = t.y + 3.f;
    return ImVec2(w > 18.f ? w : 18.f, h > 15.f ? h : 15.f);
}

inline ScreenRect DrawBadge(ImDrawList* dl, ImVec2 pos, const char* text, float fontSize,
                            const float frame[4], const float bg[4], const float textCol[4]) {
    const ImVec2 sz = BadgeSize(text, fontSize);
    const ImVec2 br(pos.x + sz.x, pos.y + sz.y);
    dl->AddRectFilled(pos, br, ColF(bg), 2.f);
    dl->AddRect(pos, br, ColF(frame), 2.f, 0, 1.f);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2(pos.x + 4.f, pos.y + 1.f),
                ColF(textCol), text);
    return ScreenRect{pos.x, pos.y, sz.x, sz.y};
}

inline bool RectsIntersect(const ScreenRect& a, const ScreenRect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

inline ImVec2 FindBadgeSlot(const ScreenRect& item, ImVec2 size, float y, bool rightSide,
                            const std::vector<ScreenRect>& occupied) {
    ImVec2 pos(rightSide ? item.x + item.w - size.x - 2.f : item.x + 2.f, y);
    for (int attempt = 0; attempt < 16; ++attempt) {
        ScreenRect cand{pos.x, pos.y, size.x, size.y};
        bool clash = false;
        for (const auto& o : occupied)
            if (RectsIntersect(cand, o)) { clash = true; break; }
        if (!clash) return pos;
        pos.y += size.y + 2.f;
    }
    return pos;
}

inline void DrawBorderHighlight(ImDrawList* dl, const ScreenRect& r, const float color[4],
                                int thickness) {
    if (thickness < WaystoneHelperConfig::kBorderThicknessMin)
        thickness = WaystoneHelperConfig::kBorderThicknessMin;
    if (thickness > WaystoneHelperConfig::kBorderThicknessMax)
        thickness = WaystoneHelperConfig::kBorderThicknessMax;
    const int scale = thickness - 1;
    const float ix = static_cast<float>(static_cast<int>(r.x) + 1 + static_cast<int>(0.5f * scale));
    const float iy = static_cast<float>(static_cast<int>(r.y) + 1 + static_cast<int>(0.5f * scale));
    const float iw = static_cast<float>(static_cast<int>(r.w) - 1 - scale);
    const float ih = static_cast<float>(static_cast<int>(r.h) - 1 - scale);
    dl->AddRect(ImVec2(ix, iy), ImVec2(ix + iw, iy + ih), ColF(color), 0.f, 0,
                static_cast<float>(thickness));
}

inline void RenderWaystone(const ScreenRect& r, const Score& e,
                           const WaystoneHelperConfig::Settings& s) {
    if (!e.matched) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float fontSize = BadgeFontSize(s);
    const float lineH = fontSize + 3.f;

    if (e.HasBorder())
        DrawBorderHighlight(dl, r, e.BorderColor(), s.borderThickness);

    std::vector<ScreenRect> occupied;

    float leftY = r.y + 2.f;
    if (e.hasAffixCountBadge) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", e.affixCount);
        ScreenRect b = DrawBadge(dl, ImVec2(r.x + 2.f, r.y + 2.f), buf, fontSize,
                                 s.affixCountBadgeColor, s.badgeBackgroundColor,
                                 s.affixCountBadgeColor);
        occupied.push_back(b);
        leftY += lineH + 2.f;
    }

    for (const auto& gm : e.affixGroupBadges) {
        const std::string label = gm.BadgeLabel();
        const ImVec2 sz = BadgeSize(label.c_str(), fontSize);
        const ImVec2 pos = FindBadgeSlot(r, sz, leftY, false, occupied);
        ScreenRect b = DrawBadge(dl, pos, label.c_str(), fontSize, gm.color,
                                 s.badgeBackgroundColor, gm.color);
        occupied.push_back(b);
        const float next = pos.y + sz.y + 2.f;
        leftY = next > leftY + lineH + 2.f ? next : leftY + lineH + 2.f;
    }

    float rightY = r.y + 2.f;
    for (const auto& st : e.importantStats) {
        std::string label = st.label;
        if (st.value > 0) label += std::to_string(st.value);
        const ImVec2 sz = BadgeSize(label.c_str(), fontSize);
        const ImVec2 pos = FindBadgeSlot(r, sz, rightY, true, occupied);
        ScreenRect b = DrawBadge(dl, pos, label.c_str(), fontSize, st.color,
                                 s.badgeBackgroundColor, st.color);
        occupied.push_back(b);
        const float next = pos.y + sz.y + 2.f;
        rightY = next > rightY + lineH + 2.f ? next : rightY + lineH + 2.f;
    }
}

struct PanelLine {
    std::string text;
    ImU32 color = IM_COL32_WHITE;
    float indent = 0.f;
};

inline void PushLine(std::vector<PanelLine>& lines, const std::string& text, ImU32 col,
                     float indent = 0.f) {
    if (lines.size() < 20) lines.push_back(PanelLine{text, col, indent});
}

inline void DrawHoverBreakdown(const ScreenRect& item, const std::string& title,
                               const Score& e, const WaystoneHelperConfig::Settings& s) {
    std::vector<PanelLine> lines;
    const ImU32 gold = IM_COL32(255, 215, 0, 255);
    const ImU32 muted = IM_COL32(180, 190, 200, 230);
    const ImU32 header = IM_COL32(210, 220, 225, 235);
    const ImU32 white = IM_COL32_WHITE;

    PushLine(lines, "WAYSTONE HELPER", gold);
    if (!title.empty()) PushLine(lines, title, white);

    {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Affixes: %d / %d", e.affixCount, s.targetAffixCount);
        PushLine(lines, buf, e.hasTargetAffixCount ? ColF(s.affixCountBadgeColor) : muted);
    }

    if (!e.importantStats.empty()) {
        PushLine(lines, "Map stats", header);
        for (const auto& st : e.importantStats) {
            std::string t = "  " + st.label;
            if (st.value > 0) t += std::to_string(st.value);
            PushLine(lines, t, ColF(st.color), 8.f);
        }
    }

    if (!e.trackedGroupMatches.empty()) {
        PushLine(lines, "Affix groups", header);
        for (const auto& gm : e.trackedGroupMatches)
            PushLine(lines, "  " + gm.name + " " + gm.BadgeLabel(), ColF(gm.color), 8.f);
    }

    if (!e.borderRules.empty()) {
        PushLine(lines, "Border rules", header);
        for (const auto& rm : e.borderRules) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "  %s %d/%d", rm.name.c_str(), rm.matched, rm.selected);
            PushLine(lines, buf, ColF(rm.color), 8.f);
        }
    }

    if (lines.empty()) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float pad = 8.f;
    const float lineH = ImGui::GetTextLineHeight() + 1.f;
    float width = 0.f;
    for (const auto& l : lines) {
        const float w = ImGui::CalcTextSize(l.text.c_str()).x + l.indent;
        if (w > width) width = w;
    }
    width += pad * 2.f;
    if (width < 160.f) width = 160.f;
    if (width > 520.f) width = 520.f;
    const float height = lines.size() * lineH + pad * 2.f;

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    float x = item.x + item.w + 10.f;
    if (x + width > disp.x) x = item.x - width - 10.f;
    if (x < 0.f) x = 4.f;
    if (x + width > disp.x) x = disp.x - width - 4.f;
    float y = item.y;
    if (y + height > disp.y) y = disp.y - height - 4.f;
    if (y < 0.f) y = 4.f;

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), IM_COL32(0, 0, 0, 225), 3.f);
    dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), IM_COL32(180, 180, 180, 180), 3.f, 0, 1.f);

    float ty = y + pad;
    for (const auto& l : lines) {
        dl->AddText(ImVec2(x + pad + l.indent, ty), l.color, l.text.c_str());
        ty += lineH;
    }
}

}
