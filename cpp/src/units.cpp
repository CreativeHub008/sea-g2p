// units.cpp — unit/currency expansion (maps to src/vi_normalizer/units.rs)
#include "sea_g2p/units.hpp"
#include "sea_g2p/regex_wrapper.hpp"
#include "sea_g2p/num2vi.hpp"
#include "sea_g2p/resources.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sea_g2p {

// ── Internal helpers ──────────────────────────────────────────────────────────

static std::string expand_scientific(const std::string& num_str) {
    std::string lower = num_str;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto e_idx = lower.find('e');
    std::string base = num_str.substr(0, e_idx);
    std::string exp  = num_str.substr(e_idx + 1);

    std::string base_norm;
    if (base.find('.') != std::string::npos) {
        auto sep = base.find('.');
        std::string dec = base.substr(sep + 1);
        // trim trailing zeros
        while (!dec.empty() && dec.back() == '0') dec.pop_back();
        base_norm = dec.empty()
            ? n2w(base.substr(0, sep))
            : n2w(base.substr(0, sep)) + " chấm " + n2w_decimal(dec);
    } else if (base.find(',') != std::string::npos) {
        auto sep = base.find(',');
        std::string dec = base.substr(sep + 1);
        while (!dec.empty() && dec.back() == '0') dec.pop_back();
        base_norm = dec.empty()
            ? n2w(base.substr(0, sep))
            : n2w(base.substr(0, sep)) + " phẩy " + n2w_decimal(dec);
    } else {
        std::string clean = base;
        clean.erase(std::remove(clean.begin(), clean.end(), ','), clean.end());
        clean.erase(std::remove(clean.begin(), clean.end(), '.'), clean.end());
        base_norm = n2w(clean);
    }

    std::string exp_val = exp;
    // Strip leading '+'
    if (!exp_val.empty() && exp_val[0] == '+') exp_val = exp_val.substr(1);
    std::string exp_norm = exp_val[0] == '-'
        ? "trừ " + n2w(exp_val.substr(1))
        : n2w(exp_val);

    return base_norm + " nhân mười mũ " + exp_norm;
}

static std::string expand_mixed_sep(const std::string& num_str) {
    auto r_dot   = num_str.rfind('.');
    auto r_comma = num_str.rfind(',');

    std::string clean;
    std::string dec_part;

    if (r_dot != std::string::npos && (r_comma == std::string::npos || r_dot > r_comma)) {
        // dot is decimal separator
        std::string tmp = num_str;
        tmp.erase(std::remove(tmp.begin(), tmp.end(), ','), tmp.end());
        auto sep = tmp.find('.');
        clean    = tmp.substr(0, sep);
        dec_part = tmp.substr(sep + 1);
    } else {
        // comma is decimal separator
        std::string tmp = num_str;
        tmp.erase(std::remove(tmp.begin(), tmp.end(), '.'), tmp.end());
        auto sep = tmp.find(',');
        clean    = tmp.substr(0, sep);
        dec_part = tmp.substr(sep + 1);
    }
    while (!dec_part.empty() && dec_part.back() == '0') dec_part.pop_back();
    if (dec_part.empty()) return n2w(clean);
    return n2w(clean) + " phẩy " + n2w_decimal(dec_part);
}

static std::string expand_single_sep(const std::string& num_str) {
    if (num_str.find(',') != std::string::npos) {
        auto parts_count = std::count(num_str.begin(), num_str.end(), ',');
        auto comma_pos   = num_str.find(',');
        std::string after = num_str.substr(comma_pos + 1);
        if (parts_count > 1 || after.size() == 3) {
            std::string r = num_str;
            r.erase(std::remove(r.begin(), r.end(), ','), r.end());
            return n2w(r);
        }
        while (!after.empty() && after.back() == '0') after.pop_back();
        if (after.empty()) return n2w(num_str.substr(0, comma_pos));
        return n2w(num_str.substr(0, comma_pos)) + " phẩy " + n2w_decimal(after);
    }
    auto dot_pos = num_str.find('.');
    std::string after = num_str.substr(dot_pos + 1);
    if (std::count(num_str.begin(), num_str.end(), '.') > 1 || after.size() == 3) {
        std::string r = num_str;
        r.erase(std::remove(r.begin(), r.end(), '.'), r.end());
        return n2w(r);
    }
    while (!after.empty() && after.back() == '0') after.pop_back();
    if (after.empty()) return n2w(num_str.substr(0, dot_pos));
    return n2w(num_str.substr(0, dot_pos)) + " chấm " + n2w_decimal(after);
}

// ── expand_number_with_sep ────────────────────────────────────────────────────

std::string expand_number_with_sep(const std::string& num_str) {
    if (num_str.empty()) return {};
    std::string lower = num_str;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.find('e') != std::string::npos) return expand_scientific(num_str);

    bool has_comma = num_str.find(',') != std::string::npos;
    bool has_dot   = num_str.find('.') != std::string::npos;
    if (has_comma && has_dot) return expand_mixed_sep(num_str);
    if (has_comma || has_dot) return expand_single_sep(num_str);
    return n2w(num_str);
}

// ── Lazy unit maps (merged measurement + currency) ────────────────────────────

static const std::unordered_map<std::string, std::string>& all_units_map() {
    static const std::unordered_map<std::string, std::string> m = []() {
        std::unordered_map<std::string, std::string> tmp;
        for (const auto& [k, v] : measurement_key_vi()) {
            std::string kl = k;
            for (char& c : kl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            tmp[kl] = v;
        }
        for (const auto& [k, v] : currency_key()) {
            if (k == "%") continue;
            std::string kl = k;
            for (char& c : kl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            tmp[kl] = v;
        }
        tmp["m"] = "mét";
        return tmp;
    }();
    return m;
}

static std::string units_re_pattern() {
    static std::string pat = []() {
        std::vector<std::string> keys;
        for (const auto& [k, _] : measurement_key_vi()) keys.push_back(k);
        for (const auto& [k, _] : currency_key())
            if (k != "%") keys.push_back(k);
        // Sort by length descending (longest match first)
        std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) {
            return a.size() > b.size();
        });
        std::string p;
        for (const auto& k : keys) {
            if (!p.empty()) p += "|";
            p += regex_escape(k);
        }
        return p;
    }();
    return pat;
}

static std::string get_unit(const std::string& u) {
    if (u == "M") return "triệu";
    if (u == "m") return "mét";
    std::string ul = u;
    for (char& c : ul) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto it = all_units_map().find(ul);
    return it != all_units_map().end() ? it->second : u;
}

// ── Regex patterns (built lazily) ─────────────────────────────────────────────

static const Regex& re_compound_unit() {
    static const Regex r(R"((?i)\b(?:(\d+(?:[.,]\d+)*))?\s*([a-zμµ²³°]+)\/([a-zμµ²³°0-9]+)\b)");
    return r;
}
static const Regex& re_units_with_num() {
    static const Regex r = []() {
        std::string pat = R"((?i)(?<![a-zA-Z\d.,])(\d+(?:[.,]\d+)*)(?:\s*(tỷ|triệu|nghìn|ngàn))?\s*()" + units_re_pattern() + R"()\b)";
        return Regex(pat);
    }();
    return r;
}
static const Regex& re_standalone_unit() {
    static const Regex r(R"((?i)(?<![\d.,])\b(km|cm|mm|kg|mg|usd|vnd|ph)\b)");
    return r;
}
static const Regex& re_currency_prefix_symbol() {
    static const Regex r(R"((?i)([$€¥£₩])\s*(\d+(?:[.,]\d+)*)(?:\s*(tỷ|triệu|nghìn|ngàn))?)");
    return r;
}
static const Regex& re_currency_suffix_symbol() {
    static const Regex r(R"((?i)(\d+(?:[.,]\d+)*)(?:\s*(tỷ|triệu|nghìn|ngàn))?\s*([$€¥£₩]))");
    return r;
}
static const Regex& re_percentage() {
    static const Regex r(R"((?i)(\d+(?:[.,]\d+)*)\s*%)");
    return r;
}
static const Regex& re_english_style_numbers() {
    static const Regex r(R"((?i)\b\d{1,3}(?:,\d{3})+(?:\.\d+)?\b)");
    return r;
}
static const Regex& re_power_of_ten() {
    static const Regex r(R"((?i)\b(\d+(?:[.,]\d+)?)\s*[x*×]\s*10\^([-+]?\d+)\b)");
    return r;
}
static const Regex& re_scientific_notation() {
    static const Regex r(R"((?i)([-–—])?(\d+(?:[.,]\d+)?e[+-]?\d+))");
    return r;
}

// ── Public functions ──────────────────────────────────────────────────────────

std::string expand_units_and_currency(const std::string& text) {
    std::string result = text;

    // Currency with prefix symbol: $100
    result = re_currency_prefix_symbol().replace_all(result, [](const Match& caps) {
        std::string symbol = caps.get(1);
        std::string num    = caps.get(2);
        std::string mag    = caps.get(3);
        auto it = currency_symbol_map().find(symbol);
        std::string full = it != currency_symbol_map().end() ? it->second : "";
        std::string r = expand_number_with_sep(num)
                      + (mag.empty() ? "" : " " + mag)
                      + (full.empty() ? "" : " " + full);
        // collapse double spaces
        std::string out;
        bool last_sp = false;
        for (char c : r) {
            if (c == ' ') { if (!last_sp) out += c; last_sp = true; }
            else { out += c; last_sp = false; }
        }
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back()  == ' ') out.pop_back();
        return out;
    });

    // Currency with suffix symbol: 100$
    result = re_currency_suffix_symbol().replace_all(result, [](const Match& caps) {
        std::string num    = caps.get(1);
        std::string mag    = caps.get(2);
        std::string symbol = caps.get(3);
        auto it = currency_symbol_map().find(symbol);
        std::string full = it != currency_symbol_map().end() ? it->second : "";
        std::string r = expand_number_with_sep(num)
                      + (mag.empty() ? "" : " " + mag)
                      + (full.empty() ? "" : " " + full);
        std::string out;
        bool last_sp = false;
        for (char c : r) {
            if (c == ' ') { if (!last_sp) out += c; last_sp = true; }
            else { out += c; last_sp = false; }
        }
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back()  == ' ') out.pop_back();
        return out;
    });

    // Percentage
    result = re_percentage().replace_all(result, [](const Match& caps) {
        return expand_number_with_sep(caps.get(1)) + " phần trăm";
    });

    // Number + unit: "100km" → "một trăm ki lô mét"
    result = re_units_with_num().replace_all(result, [](const Match& caps) {
        std::string num  = caps.get(1);
        std::string mag  = caps.get(2);
        std::string unit = caps.get(3);
        // Skip uppercase G (handled as letter later)
        if (unit == "G") return caps.get(0);
        std::string full = get_unit(unit);
        std::string r = expand_number_with_sep(num)
                      + (mag.empty() ? "" : " " + mag)
                      + " " + full;
        std::string out;
        bool last_sp = false;
        for (char c : r) {
            if (c == ' ') { if (!last_sp) out += c; last_sp = true; }
            else { out += c; last_sp = false; }
        }
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back()  == ' ') out.pop_back();
        return out;
    });

    // Standalone units (safe set only)
    result = re_standalone_unit().replace_all(result, [](const Match& caps) {
        std::string unit = caps.get(1);
        std::string ul = unit;
        for (char& c : ul) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = all_units_map().find(ul);
        std::string full = it != all_units_map().end() ? it->second : unit;
        return " " + full + " ";
    });

    return result;
}

std::string expand_compound_units(const std::string& text) {
    return re_compound_unit().replace_all(text, [](const Match& caps) {
        std::string num_str = caps.get(1);
        std::string u1_raw  = caps.get(2);
        std::string u2_raw  = caps.get(3);

        if (num_str.empty()) {
            std::string u1l = u1_raw, u2l = u2_raw;
            for (char& c : u1l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (char& c : u2l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            bool u1_is = all_units_map().count(u1l) > 0;
            bool u2_is = all_units_map().count(u2l) > 0;
            // Heuristic for single-letter ratios like P/E
            if (u1_raw.size() == 1 && u2_raw.size() == 1 && (!u1_is || !u2_is)) {
                auto it1 = vi_letter_names().find(u1l);
                auto it2 = vi_letter_names().find(u2l);
                std::string l1 = it1 != vi_letter_names().end() ? it1->second : u1_raw;
                std::string l2 = it2 != vi_letter_names().end() ? it2->second : u2_raw;
                return " " + l1 + " trên " + l2 + " ";
            }
            return " " + get_unit(u1_raw) + " trên " + get_unit(u2_raw) + " ";
        }
        return expand_number_with_sep(num_str)
               + " " + get_unit(u1_raw) + " trên " + get_unit(u2_raw) + " ";
    });
}

std::string fix_english_style_numbers(const std::string& text) {
    return re_english_style_numbers().replace_all(text, [](const Match& caps) {
        std::string val = caps.get(0);
        bool has_comma = val.find(',') != std::string::npos;
        bool has_dot   = val.find('.') != std::string::npos;
        int comma_count = static_cast<int>(std::count(val.begin(), val.end(), ','));
        if (comma_count > 1
            || (has_comma && has_dot
                && val.find(',') < val.find('.'))) {
            std::string r = val;
            r.erase(std::remove(r.begin(), r.end(), ','), r.end());
            if (has_dot) {
                // Replace last '.' with ','  — English → European format
                auto pos = r.rfind('.');
                if (pos != std::string::npos) r[pos] = ',';
            }
            return r;
        }
        if (has_comma && has_dot) {
            std::string r = val;
            r.erase(std::remove(r.begin(), r.end(), ','), r.end());
            auto pos = r.rfind('.');
            if (pos != std::string::npos) r[pos] = ',';
            return r;
        }
        return val;
    });
}

std::string expand_power_of_ten(const std::string& text) {
    return re_power_of_ten().replace_all(text, [](const Match& caps) {
        std::string base = caps.get(1);
        std::string exp  = caps.get(2);
        std::string exp_val = exp;
        if (!exp_val.empty() && exp_val[0] == '+') exp_val = exp_val.substr(1);
        std::string exp_norm = exp_val[0] == '-'
            ? "trừ " + n2w(exp_val.substr(1))
            : n2w(exp_val);
        return " " + expand_number_with_sep(base) + " nhân mười mũ " + exp_norm + " ";
    });
}

std::string expand_scientific_notation(const std::string& text) {
    return re_scientific_notation().replace_all(text, [](const Match& caps) {
        std::string neg     = caps.get(1);
        std::string num_str = caps.get(2);
        std::string expanded = expand_number_with_sep(num_str);
        if (!neg.empty()) return " âm " + expanded + " ";
        return " " + expanded + " ";
    });
}

}  // namespace sea_g2p
