#pragma once
#include <string_view>
#include <string>
#include <algorithm>
#include <variant>
#include <cctype>

inline static std::string_view Trim(std::string_view v) {
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) {
        v.remove_prefix(1);
    }
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back()))) {
        v.remove_suffix(1);
    }
    return v;
}

inline static std::string ToLower(std::string_view v) {
    std::string s(v);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}
