// datestime.cpp — date/time normalization (maps to src/vi_normalizer/datestime.rs)
#include "sea_g2p/datestime.hpp"
#include "sea_g2p/regex_wrapper.hpp"
#include "sea_g2p/num2vi.hpp"
#include "sea_g2p/resources.hpp"

#include <algorithm>
#include <sstream>

namespace sea_g2p {

static const int DAY_IN_MONTH[12] = {31,29,31,30,31,30,31,31,30,31,30,31};

static bool is_valid_date(const std::string& day_s, const std::string& month_s) {
    try {
        int d = std::stoi(day_s), m = std::stoi(month_s);
        if (m < 1 || m > 12) return false;
        return d >= 1 && d <= DAY_IN_MONTH[m - 1];
    } catch (...) { return false; }
}

static std::string month_word(int m) {
    return (m == 4) ? "tư" : n2w(std::to_string(m));
}

// Return words around a match position for context detection
static std::vector<std::string> get_context_words(
    const std::string& text, size_t start, size_t end_, size_t window)
{
    // left part
    std::istringstream lss(text.substr(0, start));
    std::vector<std::string> left_words;
    std::string w;
    while (lss >> w) left_words.push_back(w);

    std::istringstream rss(text.substr(end_));
    std::vector<std::string> right_words;
    while (rss >> w) right_words.push_back(w);

    std::vector<std::string> ctx;
    size_t ls = left_words.size();
    size_t lstart = ls > window ? ls - window : 0;
    for (size_t i = lstart; i < ls; ++i) {
        auto& wrd = left_words[i];
        std::string clean;
        for (char c : wrd) {
            if (std::string(",.!?;()[]{}").find(c) == std::string::npos) clean += c;
        }
        // lowercase ASCII part
        for (char& c : clean) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (!clean.empty()) ctx.push_back(clean);
    }
    size_t rn = std::min(right_words.size(), window);
    for (size_t i = 0; i < rn; ++i) {
        auto& wrd = right_words[i];
        std::string clean;
        for (char c : wrd) {
            if (std::string(",.!?;()[]{}").find(c) == std::string::npos) clean += c;
        }
        for (char& c : clean) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (!clean.empty()) ctx.push_back(clean);
    }
    return ctx;
}

static const char* norm_time_part(const std::string& s) {
    // Strip leading zeros, return pointer to first non-zero or "0"
    static thread_local std::string buf;
    buf = s;
    size_t i = 0;
    while (i < buf.size() - 1 && buf[i] == '0') ++i;
    buf = buf.substr(i);
    return buf.c_str();
}

// ── Regex patterns ─────────────────────────────────────────────────────────

static const Regex& re_iso_fix() {
    static const Regex r(R"((\d{2})T(\d{2})|(\d{2})Z\b)");
    return r;
}
static const Regex& re_yyyy_mm_dd() {
    static const Regex r(R"((?<![a-zA-Z\d])(?<![a-zA-Z\d][.,])(\d{4})-(\d{2})-(\d{2})(?!\d|[.,]\d))");
    return r;
}
static const Regex& re_full_date() {
    // DD/MM/YYYY or DD-MM-YYYY or DD.MM.YYYY
    static const Regex r(R"((?<![a-zA-Z\d])(?<![a-zA-Z\d][.,])(\d{1,2})(\/|-|\.)(\d{1,2})(\/|-|\.)(\d{4})(?!\d|[.,]\d))");
    return r;
}
static const Regex& re_day_month() {
    // DD/MM or DD-MM
    static const Regex r(R"((?<![a-zA-Z\d])(?<![a-zA-Z\d][.,])(\d{1,2})(\/|-)(\d{1,2})(?!\d|[.,]\d))");
    return r;
}
static const Regex& re_month_year() {
    static const Regex r(R"((?<![a-zA-Z\d])(?<![a-zA-Z\d][.,])(\d{1,2})(\/|-|\.)(\d{4})(?!\d|[.,]\d))");
    return r;
}
static const Regex& re_period_year() {
    static const Regex r(R"((?i)\b([a-zA-Z]\d*)/(\d{4})\b)");
    return r;
}
static const Regex& re_full_time() {
    static const Regex r(R"((?i)\b(\d+)(g|:|h)(\d{1,2})(p|:|m)(\d{1,2})(?:\s*(giây|s|g))?\b)");
    return r;
}
static const Regex& re_time() {
    static const Regex r(R"((?ix)\b(\d+)(g|h|:)(\d{1,2})(?:\s*(phút|p|m|giây|s|g))?\b(?![.,]\d))");
    return r;
}
static const Regex& re_hour_context() {
    static const Regex r(R"((?i)\b(\d+)g\s*(sáng|trưa|chiều|tối|khuya)\b)");
    return r;
}
static const Regex& re_luc_hour() {
    static const Regex r(R"((?i)\blúc\s*(\d+)g\b)");
    return r;
}
static const Regex& re_redundant_ngay()  { static const Regex r(R"((?i)\bngày\s+ngày\b)");   return r; }
static const Regex& re_redundant_thang() { static const Regex r(R"((?i)\btháng\s+tháng\b)"); return r; }
static const Regex& re_redundant_nam()   { static const Regex r(R"((?i)\bnăm\s+năm\b)");     return r; }
static const Regex& re_redundant_hom()   { static const Regex r(R"((?i)\bhôm\s+ngày\b)");    return r; }

// ── normalize_date ────────────────────────────────────────────────────────────

std::string normalize_date(const std::string& text) {
    std::string result = text;

    // ISO T/Z separators
    result = re_iso_fix().replace_all(result, [](const Match& caps) {
        if (caps.has(1))
            return caps.get(1) + " T " + caps.get(2);
        return caps.get(3) + " Z ";
    });

    // YYYY-MM-DD
    result = re_yyyy_mm_dd().replace_all(result, [](const Match& caps) {
        std::string year = caps.get(1), month = caps.get(2), day = caps.get(3);
        if (!is_valid_date(day, month)) return caps.get(0);
        int m = std::stoi(month), d = std::stoi(day);
        return "ngày " + n2w(std::to_string(d)) + " tháng " + month_word(m)
               + " năm " + n2w(year);
    });

    // Letter/Q + year: "Q3/2024" → "quý ba hai nghìn …"
    result = re_period_year().replace_all(result, [](const Match& caps) {
        std::string code = caps.get(1);
        std::string year = caps.get(2);
        std::string code_lower = code;
        for (char& c : code_lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string prefix;
        if (code_lower[0] == 'q' && code.size() <= 2) {
            std::string q_num = code.size() > 1 ? code.substr(1) : "";
            prefix = "quý " + (q_num.empty() ? "" : n2w(q_num));
        } else {
            std::vector<std::string> parts;
            for (char c : code) {
                std::string cl(1, static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    parts.push_back(n2w(std::string(1, c)));
                } else {
                    auto it = vi_letter_names().find(cl);
                    parts.push_back(it != vi_letter_names().end() ? it->second : cl);
                }
            }
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i) prefix += ' ';
                prefix += parts[i];
            }
        }
        // Trim prefix
        while (!prefix.empty() && prefix.front() == ' ') prefix.erase(prefix.begin());
        while (!prefix.empty() && prefix.back()  == ' ') prefix.pop_back();
        return prefix + " " + n2w_decimal(year);
    });

    // DD/MM/YYYY
    result = re_full_date().replace_all(result, [](const Match& caps) {
        std::string day = caps.get(1), month = caps.get(3), year = caps.get(5);
        if (!is_valid_date(day, month)) return caps.get(0);
        int m = std::stoi(month), d = std::stoi(day);
        return "ngày " + n2w(std::to_string(d)) + " tháng " + month_word(m)
               + " năm " + n2w(year);
    });

    // MM/YYYY
    result = re_month_year().replace_all(result, [](const Match& caps) {
        std::string month_s = caps.get(1), year_s = caps.get(3);
        int m = 0, y = 0;
        try { m = std::stoi(month_s); y = std::stoi(year_s); } catch (...) {}
        if (m < 1 || m > 12 || y > 2500) return caps.get(0);
        return "tháng " + month_word(m) + " năm " + n2w(year_s);
    });

    // DD/MM (context-sensitive)
    std::string current = result;
    result = re_day_month().replace_all(current, [&current](const Match& caps) {
        std::string day_s = caps.get(1), month_s = caps.get(3);
        int a = 0, b = 0;
        try { a = std::stoi(day_s); b = std::stoi(month_s); } catch (...) {}

        bool is_valid = is_valid_date(day_s, month_s);
        auto ctx = get_context_words(current, caps.start(0), caps.end(0), 3);

        bool month_leading_zero = month_s.size() > 1 && month_s[0] == '0';
        bool day_leading_zero   = day_s.size() > 1   && day_s[0]   == '0';

        if (is_valid && (month_leading_zero || day_leading_zero)) {
            return "ngày " + n2w(std::to_string(a)) + " tháng " + month_word(b);
        }

        if (is_valid) {
            for (const auto& w : ctx) {
                if (date_keywords().count(w))
                    return "ngày " + n2w(std::to_string(a)) + " tháng " + month_word(b);
            }
        }

        static const std::string math_syms[] = {"+","-","*","x","×","/","=",">","<","≥","≤","≈","±"};
        bool math_ctx = false;
        for (const auto& w : ctx) {
            if (math_keywords().count(w)) { math_ctx = true; break; }
            for (const auto& s : math_syms) { if (w == s) { math_ctx = true; break; } }
            if (math_ctx) break;
        }
        if (math_ctx) return n2w(day_s) + " trên " + n2w(month_s);

        if (!is_valid) {
            if (day_s[0] == '0' || month_s[0] == '0')
                return n2w(day_s) + " trên " + n2w(month_s);
            return caps.get(0);
        }
        return n2w(day_s) + " trên " + n2w(month_s);
    });

    result = re_redundant_ngay().replace_all(result,  "ngày");
    result = re_redundant_thang().replace_all(result, "tháng");
    result = re_redundant_nam().replace_all(result,   "năm");
    result = re_redundant_hom().replace_all(result,   "hôm");

    return result;
}

// ── normalize_time ────────────────────────────────────────────────────────────

std::string normalize_time(const std::string& text) {
    std::string result = text;

    // Full time: 10:30:15 or 10g30p15s
    result = re_full_time().replace_all(result, [](const Match& caps) {
        std::string h = caps.get(1), m = caps.get(3), s = caps.get(5);
        // norm_time_part uses thread_local buf; call separately
        std::string hs = n2w(h[0] == '0' && h.size() > 1 ? h.substr(h.find_first_not_of('0')) : h);
        if (hs.empty()) hs = "không";
        std::string ms_str = m;
        while (ms_str.size() > 1 && ms_str[0] == '0') ms_str.erase(ms_str.begin());
        std::string ss_str = s;
        while (ss_str.size() > 1 && ss_str[0] == '0') ss_str.erase(ss_str.begin());
        return n2w(hs) + " giờ " + n2w(ms_str) + " phút " + n2w(ss_str) + " giây";
    });

    // Simple time: 10:30, 14h30
    result = re_time().replace_all(result, [](const Match& caps) {
        std::string full  = caps.get(0);
        std::string h_str = caps.get(1);
        std::string sep   = caps.get(2);
        std::string m_str = caps.get(3);
        std::string suf_raw = caps.get(4);
        std::string suffix;
        for (char c : suf_raw)
            suffix += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        int h_int = -1, m_int = -1;
        try { h_int = std::stoi(h_str); } catch (...) {}
        try { m_int = std::stoi(m_str); } catch (...) {}

        // ":" requires exactly 2-digit minutes unless suffix exists
        if (sep == ":" && m_str.size() != 2 && suffix.empty()) return full;

        // Chemistry hint: skip single-letter sep with no suffix + single-digit minute
        std::string sep_lo = sep;
        for (char& c : sep_lo) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if ((sep_lo == "h" || sep_lo == "g") && suffix.empty() && m_str.size() == 1)
            return full;

        if (m_int >= 0 && m_int < 60) {
            bool is_min_sec = (sep == ":" && h_int >= 24);
            // Strip leading zeros
            std::string h_norm = h_str;
            while (h_norm.size() > 1 && h_norm[0] == '0') h_norm.erase(h_norm.begin());
            std::string m_norm = m_str;
            while (m_norm.size() > 1 && m_norm[0] == '0') m_norm.erase(m_norm.begin());

            std::string h_word = is_min_sec ? n2w(h_str) : n2w(h_norm);
            std::string m_word = n2w(m_norm);
            std::string h_unit = is_min_sec ? "phút" : "giờ";
            std::string m_unit = is_min_sec ? "giây" : "phút";
            return h_word + " " + h_unit + " " + m_word + " " + m_unit;
        }
        return full;
    });

    result = re_hour_context().replace_all(result, [](const Match& caps) {
        return n2w(caps.get(1)) + " giờ " + caps.get(2);
    });

    result = re_luc_hour().replace_all(result, [](const Match& caps) {
        return "lúc " + n2w(caps.get(1)) + " giờ";
    });

    return result;
}

}  // namespace sea_g2p
