#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace WaystoneHelper {

inline std::string ToLowerCopy(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

inline bool ContainsCI(std::string_view hay, std::string_view needleLower) {
    if (needleLower.empty()) return true;
    if (needleLower.size() > hay.size()) return false;
    for (size_t i = 0; i + needleLower.size() <= hay.size(); ++i) {
        size_t j = 0;
        for (; j < needleLower.size(); ++j) {
            const char c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(hay[i + j])));
            if (c != needleLower[j]) break;
        }
        if (j == needleLower.size()) return true;
    }
    return false;
}

inline std::string NormalizeIdentifier(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value)
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

inline bool LooksLikeWaystone(const std::string& path, const std::string& baseType) {
    if (ContainsCI(baseType, "waystone")) return true;
    if (ContainsCI(path, "waystone")) return true;
    if (ContainsCI(path, "/maps/")) return true;
    return false;
}

inline int ParseWaystoneTier(const std::string& path) {
    const std::string lower = ToLowerCopy(path);
    const std::string key = "tier";
    size_t pos = lower.find(key);
    while (pos != std::string::npos) {
        size_t p = pos + key.size();
        if (p < lower.size() && std::isdigit(static_cast<unsigned char>(lower[p]))) {
            int val = 0;
            while (p < lower.size() && std::isdigit(static_cast<unsigned char>(lower[p]))) {
                val = val * 10 + (lower[p] - '0');
                ++p;
            }
            return val;
        }
        pos = lower.find(key, pos + 1);
    }
    return 0;
}

}
