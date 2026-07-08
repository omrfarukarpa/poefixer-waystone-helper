#pragma once

#include "sdk/PluginSDK.h"

#include <optional>

namespace WaystoneHelper {

struct ScreenRect {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
};

inline bool GridOnScreen(const PluginSDK::Inventory& inv, float displayW, float displayH) {
    if (!inv.Grid.Valid || inv.Grid.CellSize <= 0.f) return false;
    if (displayW <= 0.f || displayH <= 0.f) return false;
    const float x = inv.Grid.GridScreenX;
    const float y = inv.Grid.GridScreenY;
    const float margin = 4.f;
    return x >= -margin && y >= -margin && x < displayW && y < displayH;
}

inline bool ItemOnScreen(const PluginSDK::InventoryItem& item, float displayW, float displayH) {
    if (!item.ScreenValid) return false;
    if (item.ScreenW <= 0.f || item.ScreenH <= 0.f) return false;
    if (displayW <= 0.f || displayH <= 0.f) return false;
    const float cx = item.ScreenX + item.ScreenW * 0.5f;
    const float cy = item.ScreenY + item.ScreenH * 0.5f;
    return cx >= 0.f && cy >= 0.f && cx < displayW && cy < displayH;
}

inline bool GridLayoutPlausible(const PluginSDK::Inventory& inv, float displayW) {
    if (inv.TotalBoxesY < 6) return false;
    if (inv.Grid.CellSize > 0.f
        && static_cast<float>(inv.TotalBoxesX) * inv.Grid.CellSize > displayW)
        return false;
    return true;
}

inline std::optional<ScreenRect> ResolveItemRect(const PluginSDK::Inventory& inv,
                                                 const PluginSDK::InventoryItem& item,
                                                 float displayW, float displayH) {
    if (ItemOnScreen(item, displayW, displayH)) {
        return ScreenRect{item.ScreenX, item.ScreenY, item.ScreenW, item.ScreenH};
    }
    if (inv.Grid.Valid && GridOnScreen(inv, displayW, displayH)
        && inv.Grid.CellSize > 0.f && GridLayoutPlausible(inv, displayW)) {
        const float cell = inv.Grid.CellSize;
        return ScreenRect{
            inv.Grid.GridScreenX + static_cast<float>(item.SlotX) * cell,
            inv.Grid.GridScreenY + static_cast<float>(item.SlotY) * cell,
            static_cast<float>(item.Width) * cell,
            static_cast<float>(item.Height) * cell};
    }
    return std::nullopt;
}

}
