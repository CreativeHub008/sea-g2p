// misc.cpp — acronym/symbol/letter expansion (maps to src/vi_normalizer/misc.rs)
#include "sea_g2p/misc.hpp"
#include "sea_g2p/regex_wrapper.hpp"
#include "sea_g2p/num2vi.hpp"
#include "sea_g2p/resources.hpp"
#include "sea_g2p/technical.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sea_g2p {

static const std::string VI_UPPER = "ĐĂÂÊÔƠƯ";

// ── Regex patterns ────────────────────────────────────────────────────────────

const Regex& re_acronyms_exceptions() {
    static const Regex re = []() {
        std::vector<std::string> keys;
        for (const auto& [k, _] : combined_exceptions()) keys.push_back(k);
        std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) {
            return a.size() > b.size();
        });
        std::string pat;
        for (const auto& k : keys) {
            if (!pat.empty()) pat += "|";
            pat += "\\b" + regex_escape(k) + "\\b";
        }
        return Regex(pat);
    }();
    return re;
}

const Regex& re_acronym() {
    // Matches 2+ consecutive capital letters (ASCII or Vietnamese upper)
    static const Regex r(
        R"(\b(?=[A-ZĐĂÂÊÔƠƯ][A-ZĐĂÂÊÔƠƯa-zđăâêôơư0-9]*[A-ZĐĂÂÊÔƠƯ])(?:[A-ZĐĂÂÊÔƠƯ][a-zđăâêôơư]?\d*){2,}\b)");
    return r;
}

const Regex& domain_suffixes_re() {
    static const Regex r(R"((?i)\.(com|vn|net|org|edu|gov|io|biz|info)\b)");
    return r;
}

static const Regex& re_roman_number() {
    static const Regex r(
        R"(\b(?=[IVXLCDM]{2,})(?:M{0,4}(?:CM|CD|D?C{0,3})(?:XC|XL|L?X{0,3})(?:IX|IV|V?I{0,3}))(?<=[IVXLCDM])\b)");
    return r;
}
static const Regex& re_standalone_letter() {
    static const Regex r(R"((?<!['''])\b([a-zA-Z])\b(\.?))");
    return r;
}
static const Regex& re_version() {
    static const Regex r(R"((?<![-–—])\b(\d+(?:\.\d+){2,})\b)");
    return r;
}
static const Regex& re_prime() {
    static const Regex r(R"((\b[a-zA-Z0-9])['\x{2019}](?!\w))");
    return r;
}
static const Regex& re_prime_digit() {
    static const Regex r(R"((?<=\d)(['\x{2019}]+|[\x22\x{201D}]))");
    return r;
}
static const Regex& re_letter() {
    static const Regex r(R"((?i)(chữ|chữ cái|kí tự|ký tự)\s+(['"]?)([a-z])(['"]?)\b)");
    return r;
}
static const Regex& re_alphanumeric() {
    static const Regex r(R"(\b(\d+)([a-zA-Z])\b)");
    return r;
}
static const Regex& re_letter_digit() {
    static const Regex r(R"(\b([a-zA-Z])(\d+)\b)");
    return r;
}
static const Regex& re_brackets() {
    static const Regex r(R"([\(\[\{]\s*(.*?)\s*[\)\]\}])");
    return r;
}
static const Regex& re_strip_brackets() {
    static const Regex r(R"([\[\]\(\)\{\}])");
    return r;
}
static const Regex& re_temp_c_neg() { static const Regex r(R"((?i)-(\d+(?:[.,]\d+)?)\s*°\s*c\b)"); return r; }
static const Regex& re_temp_f_neg() { static const Regex r(R"((?i)-(\d+(?:[.,]\d+)?)\s*°\s*f\b)"); return r; }
static const Regex& re_temp_c()     { static const Regex r(R"((?i)(\d+(?:[.,]\d+)?)\s*°\s*c\b)");  return r; }
static const Regex& re_temp_f()     { static const Regex r(R"((?i)(\d+(?:[.,]\d+)?)\s*°\s*f\b)");  return r; }
static const Regex& re_degree()     { static const Regex r(R"(°)");                                  return r; }
static const Regex& re_standard_colon() {
    static const Regex r(R"((?<![.,\d])\b(\d+):(\d+(?:\.\d+)?)\b(?![.,\d]))");
    return r;
}
static const Regex& re_clean_others() {
    static const Regex r(
        R"([^a-zA-Z0-9\sàáảãạăắằẳẵặâấầẩẫậèéẻẽẹêếềểễệìíỉĩịòóỏõọôốồổỗộơớờởỡợùúủũụưứừửữựỳýỷỹỵđÀÁẢÃẠĂẮẰẲẴẶÂẤẦẨẪẬÈÉẺẼẸÊẾỀỂỄỆÌÍỈĨỊÒÓỎÕỌÔỐỒỔỖỘƠỚỜỞỠỢÙÚỦŨỤƯỨỪỬỮỰỲÝỶỸỴĐ.,!?_'''\-])");
    return r;
}
static const Regex& re_clean_quotes() {
    static const Regex r(R"([""„])");
    return r;
}
static const Regex& re_clean_quotes_edges() {
    static const Regex r(R"((^|[\s.,!?;:])[\x{2018}\x{2019}']+|[\x{2018}\x{2019}']+($|[\s.,!?;:]))");
    return r;
}
static const Regex& re_colon_semicolon() {
    static const Regex r(R"([:;])");
    return r;
}
static const Regex& re_unit_powers() {
    static const Regex r(R"(\b([a-zA-Z]+)\^([-+]?\d+)\b)");
    return r;
}
static const Regex& re_acronyms_split() {
    static const Regex r(R"(([.!?]+(?:\s+|$)))");
    return r;
}

// ── expand_roman ──────────────────────────────────────────────────────────────

std::string expand_roman(const std::string& match_str) {
    if (match_str.empty()) return {};
    int result = 0;
    std::string upper = match_str;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    const auto& rn = roman_numerals();
    for (size_t i = 0; i < upper.size(); ++i) {
        int val = 0;
        auto it = rn.find(upper[i]);
        if (it != rn.end()) val = it->second;
        int next_val = 0;
        if (i + 1 < upper.size()) {
            auto itn = rn.find(upper[i+1]);
            if (itn != rn.end()) next_val = itn->second;
        }
        if (val < next_val) result -= val;
        else result += val;
    }
    if (result == 0) return match_str;
    return " " + n2w(std::to_string(result)) + " ";
}

// ── expand_unit_powers ────────────────────────────────────────────────────────

std::string expand_unit_powers(const std::string& text) {
    return re_unit_powers().replace_all(text, [](const Match& caps) {
        std::string base  = caps.get(1);
        std::string power = caps.get(2);
        std::string power_val = power;
        std::string power_norm;
        if (!power_val.empty() && power_val[0] == '-')
            power_norm = "trừ " + n2w(power_val.substr(1));
        else {
            if (!power_val.empty() && power_val[0] == '+') power_val = power_val.substr(1);
            power_norm = n2w(power_val);
        }
        std::string bl = base;
        for (char& c : bl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::string full_base;
        auto it = measurement_key_vi().find(bl);
        if (it != measurement_key_vi().end()) full_base = it->second;
        else {
            auto it2 = currency_key().find(bl);
            full_base = (it2 != currency_key().end()) ? it2->second : base;
        }
        return " " + full_base + " mũ " + power_norm + " ";
    });
}

// ── expand_letter ─────────────────────────────────────────────────────────────

std::string expand_letter(const std::string& text) {
    return re_letter().replace_all(text, [](const Match& caps) {
        std::string prefix = caps.get(1);
        std::string ch     = caps.get(3);
        std::string cl = ch;
        for (char& c : cl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = vi_letter_names().find(cl);
        if (it != vi_letter_names().end())
            return prefix + " " + it->second + " ";
        return caps.get(0);
    });
}

// ── expand_abbreviations ──────────────────────────────────────────────────────

std::string expand_abbreviations(const std::string& text) {
    std::string result = text;
    for (const auto& [k, v] : abbrs()) {
        std::string::size_type pos = 0;
        while ((pos = result.find(k, pos)) != std::string::npos) {
            result.replace(pos, k.size(), v);
            pos += v.size();
        }
    }
    return result;
}

// ── expand_standalone_letters ─────────────────────────────────────────────────

std::string expand_standalone_letters(const std::string& text) {
    return re_standalone_letter().replace_all(text, [](const Match& caps) {
        std::string char_raw = caps.get(1);
        std::string dot      = caps.get(2);
        std::string cl = char_raw;
        for (char& c : cl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = vi_letter_names().find(cl);
        if (it != vi_letter_names().end()) {
            bool is_upper = std::isupper(static_cast<unsigned char>(char_raw[0]));
            if (is_upper && dot == ".") return " " + it->second + " ";
            return " " + it->second + dot + " ";
        }
        return caps.get(0);
    });
}

// ── normalize_acronyms ────────────────────────────────────────────────────────

std::string normalize_acronyms(const std::string& text) {
    // Split on sentence-ending punctuation
    std::vector<std::string> final_parts;
    size_t last = 0;
    for (const auto& m : re_acronyms_split().find_all(text)) {
        final_parts.push_back(text.substr(last, m.start(0) - last));
        final_parts.push_back(m.get(0));
        last = m.end(0);
    }
    final_parts.push_back(text.substr(last));

    std::string result;
    for (size_t i = 0; i < final_parts.size(); i += 2) {
        const std::string& s   = final_parts[i];
        const std::string& sep = (i + 1 < final_parts.size()) ? final_parts[i+1] : "";

        if (s.empty()) { result += sep; continue; }

        // Check if the whole segment is all-caps
        std::istringstream wss(s);
        std::vector<std::string> words;
        std::string w;
        while (wss >> w) words.push_back(w);
        bool is_all_caps = !words.empty();
        for (const auto& wrd : words) {
            bool has_alpha = false, all_up = true;
            for (char c : wrd) {
                if (std::isalpha(static_cast<unsigned char>(c))) has_alpha = true;
                if (!std::isupper(static_cast<unsigned char>(c)) && std::isalpha(static_cast<unsigned char>(c))) all_up = false;
            }
            if (!has_alpha || !all_up) { is_all_caps = false; break; }
        }

        std::string processed = s;
        if (!is_all_caps) {
            processed = re_acronym().replace_all(processed, [](const Match& caps) {
                const std::string& word = caps.get(0);
                // All digits? skip
                bool all_dig = true;
                for (char c : word) if (!std::isdigit(static_cast<unsigned char>(c))) { all_dig = false; break; }
                if (all_dig) return word;

                // Word-like acronyms (e.g. UNESCO)
                if (word_like_acronyms().count(word))
                    return "__start_en__" + [&word]() {
                        std::string r = word;
                        for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        return r;
                    }() + "__end_en__";

                // Has Vietnamese letters or mixed case?
                bool has_vi = false, is_mixed = false, has_sub = false;
                bool any_lower = false, any_upper = false;
                for (char c : word) {
                    if (!isascii(c)) has_vi = true;
                    if (std::islower(static_cast<unsigned char>(c))) any_lower = true;
                    if (std::isupper(static_cast<unsigned char>(c))) any_upper = true;
                    if ((uint8_t)c >= 0xE2) has_sub = true; // rough subscript check
                }
                is_mixed = any_lower && any_upper;

                if (has_vi || is_mixed) {
                    // Char-by-char with letter names
                    std::vector<std::string> parts;
                    const char* ptr = word.data();
                    const char* end = ptr + word.size();
                    while (ptr < end) {
                        uint32_t cp = utf8_next(ptr, end);
                        if (cp >= '0' && cp <= '9') {
                            parts.push_back(n2w_single(std::string(1, (char)cp)));
                        } else {
                            // Build UTF-8 string for this codepoint, lowercase
                            std::string cl;
                            uint32_t lo = cp;
                            if (cp >= 'A' && cp <= 'Z') lo = cp + 32;
                            // For non-ASCII lowercase we rely on looking it up in vi_letter_names
                            // Re-encode to UTF-8
                            if (lo < 0x80) cl = std::string(1, (char)lo);
                            else if (lo < 0x800) {
                                cl += (char)(0xC0 | (lo >> 6));
                                cl += (char)(0x80 | (lo & 0x3F));
                            } else {
                                cl += (char)(0xE0 | (lo >> 12));
                                cl += (char)(0x80 | ((lo >> 6) & 0x3F));
                                cl += (char)(0x80 | (lo & 0x3F));
                            }
                            auto it = vi_letter_names().find(cl);
                            parts.push_back(it != vi_letter_names().end() ? it->second : cl);
                        }
                    }
                    std::string r;
                    for (size_t i = 0; i < parts.size(); ++i) { if (i) r += ' '; r += parts[i]; }
                    return r;
                }

                // Has digits → char-by-char
                bool has_digit = false;
                for (char c : word) if (std::isdigit(static_cast<unsigned char>(c))) { has_digit = true; break; }
                if (has_digit) {
                    std::vector<std::string> parts;
                    for (char c : word) {
                        if (std::isdigit(static_cast<unsigned char>(c)))
                            parts.push_back(n2w_single(std::string(1, c)));
                        else {
                            std::string cl(1, (char)std::tolower(static_cast<unsigned char>(c)));
                            auto it = vi_letter_names().find(cl);
                            parts.push_back(it != vi_letter_names().end() ? it->second : cl);
                        }
                    }
                    std::string r;
                    for (size_t i = 0; i < parts.size(); ++i) { if (i) r += ' '; r += parts[i]; }
                    return r;
                }

                // Pure ASCII caps → spell out letters
                std::string spaced;
                for (char c : word) {
                    if (std::isalnum(static_cast<unsigned char>(c))) {
                        if (!spaced.empty()) spaced += ' ';
                        spaced += (char)std::tolower(static_cast<unsigned char>(c));
                    }
                }
                if (!spaced.empty())
                    return "__start_en__" + spaced + "__end_en__";
                return word;
            });
        }
        result += processed + sep;
    }
    return result;
}

// ── expand_alphanumeric ───────────────────────────────────────────────────────

std::string expand_alphanumeric(const std::string& text) {
    return re_alphanumeric().replace_all(text, [&text](const Match& caps) {
        std::string num = caps.get(1);
        std::string ch  = caps.get(2);
        std::string cl  = ch;
        for (char& c : cl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = vi_letter_names().find(cl);
        if (it != vi_letter_names().end()) {
            std::string pronunciation = it->second;
            // Special: "d" in road context → "đê"
            std::string tl = text;
            for (char& c : tl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (cl == "d" && (tl.find("quốc lộ") != std::string::npos || tl.find("ql") != std::string::npos))
                pronunciation = "đê";
            return num + " " + pronunciation;
        }
        return caps.get(0);
    });
}

// ── expand_symbols ────────────────────────────────────────────────────────────

std::string expand_symbols(const std::string& text) {
    // Replace "<>" first (special two-char case)
    std::string res;
    {
        std::string::size_type pos = 0;
        const std::string from = "<>";
        const std::string to   = " khác ";
        while ((pos = text.find(from, pos)) != std::string::npos) {
            res.append(text, (res.empty() ? 0 : res.size()), 0); // no-op workaround
            break;
        }
        res = text;
        pos = 0;
        while ((pos = res.find(from, pos)) != std::string::npos) {
            res.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    std::string out;
    const char* ptr = res.data();
    const char* end = ptr + res.size();
    const auto& sm  = symbols_map();
    const auto& sup = superscripts_map();
    const auto& sub = subscripts_map();

    while (ptr < end) {
        const char* prev = ptr;
        uint32_t cp = utf8_next(ptr, end);

        auto it = sm.find(cp);
        if (it != sm.end()) { out += it->second; continue; }
        auto it2 = sup.find(cp);
        if (it2 != sup.end()) { out += it2->second; continue; }
        auto it3 = sub.find(cp);
        if (it3 != sub.end()) { out += it3->second; continue; }

        // Output as-is (re-append the raw bytes)
        out.append(prev, ptr - prev);
    }
    return out;
}

// ── expand_prime ─────────────────────────────────────────────────────────────

std::string expand_prime(const std::string& text) {
    std::string res = re_prime().replace_all(text, [](const Match& caps) {
        std::string val = caps.get(1);
        std::string vl  = val;
        for (char& c : vl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::string name;
        if (std::isdigit(static_cast<unsigned char>(val[0]))) {
            name = n2w_single(vl);
        } else {
            auto it = vi_letter_names().find(vl);
            name = it != vi_letter_names().end() ? it->second : vl;
        }
        return name + " phẩy";
    });

    res = re_prime_digit().replace_all(res, [](const Match& caps) {
        std::string q = caps.get(1);
        if (q == "\"" || q == "\xE2\x80\x9D" /* " */ || q.size() > 1)
            return std::string(" phẩy phẩy ");
        return std::string(" phẩy ");
    });
    return res;
}

// ── expand_temperatures ──────────────────────────────────────────────────────

std::string expand_temperatures(const std::string& text) {
    std::string res = re_temp_c_neg().replace_all(text, "âm $1 độ xê");
    res = re_temp_f_neg().replace_all(res,   "âm $1 độ ép");
    res = re_temp_c().replace_all(res,       "$1 độ xê");
    res = re_temp_f().replace_all(res,       "$1 độ ép");
    res = re_degree().replace_all(res,       " độ ");
    return res;
}

// ── normalize_others ──────────────────────────────────────────────────────────

std::string normalize_others(const std::string& text) {
    // Replace known exceptions
    std::string res = re_acronyms_exceptions().replace_all(text, [](const Match& caps) {
        auto it = combined_exceptions().find(caps.get(0));
        return it != combined_exceptions().end() ? it->second : caps.get(0);
    });

    res = normalize_slashes(res);

    // Expand domain suffixes
    res = domain_suffixes_re().replace_all(res, [](const Match& caps) {
        std::string suf = caps.get(1);
        std::string sl  = suf;
        for (char& c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = domain_suffix_map().find(sl);
        std::string full = it != domain_suffix_map().end() ? it->second : "";
        return " chấm " + (full.empty() ? suf : full) + " ";
    });

    res = re_roman_number().replace_all(res, [](const Match& caps) {
        return expand_roman(caps.get(0));
    });

    res = expand_letter(res);
    res = expand_alphanumeric(res);

    res = re_letter_digit().replace_all(res, [](const Match& caps) {
        std::string l = caps.get(1);
        std::string d = caps.get(2);
        std::string cl = l;
        for (char& c : cl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = vi_letter_names().find(cl);
        if (it != vi_letter_names().end())
            return it->second + " " + n2w(d);
        return caps.get(0);
    });

    res = expand_prime(res);
    res = expand_unit_powers(res);
    res = re_clean_quotes().replace_all(res, "");
    res = re_clean_quotes_edges().replace_all(res, "$1 $2");
    res = expand_symbols(res);
    res = re_brackets().replace_all(res, ", $1, ");
    res = re_strip_brackets().replace_all(res, " ");
    res = expand_temperatures(res);
    res = normalize_acronyms(res);

    // Version numbers: 1.2.3 → "một chấm hai chấm ba"
    res = re_version().replace_all(res, [](const Match& caps) {
        std::string v = caps.get(1);
        // Split on '.'
        std::vector<std::string> parts;
        std::istringstream ss(v);
        std::string tok;
        while (std::getline(ss, tok, '.')) parts.push_back(tok);
        std::string r;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) r += " chấm ";
            // Read digit by digit
            std::string seg;
            for (char c : parts[i]) {
                if (!seg.empty()) seg += ' ';
                seg += n2w_single(std::string(1, c));
            }
            r += seg;
        }
        return r;
    });

    // Numeric ratios: 2:1 → "hai trên một", 9001:2015 → "9001, 2015"
    res = re_standard_colon().replace_all(res, [](const Match& caps) {
        std::string n1 = caps.get(1);
        std::string n2 = caps.get(2);
        uint64_t n1_val = 0;
        try { n1_val = std::stoull(n1); } catch (...) {}
        if (n1_val < 1000 && n2.find('.') == std::string::npos)
            return " " + n1 + " trên " + n2 + " ";
        return n1 + ", " + n2;
    });

    res = re_colon_semicolon().replace_all(res, ", ");
    res = re_clean_others().replace_all(res, " ");
    return res;
}

}  // namespace sea_g2p
