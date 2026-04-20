// vi_normalizer.cpp — Vietnamese text normalizer (maps to src/vi_normalizer/mod.rs)
#include "sea_g2p/vi_normalizer.hpp"
#include "sea_g2p/regex_wrapper.hpp"
#include "sea_g2p/resources.hpp"
#include "sea_g2p/num2vi.hpp"
#include "sea_g2p/numerical.hpp"
#include "sea_g2p/datestime.hpp"
#include "sea_g2p/units.hpp"
#include "sea_g2p/misc.hpp"
#include "sea_g2p/technical.hpp"

#include <algorithm>
#include <cctype>
#include <future>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifdef SEA_G2P_HAS_ICU
#  include <unicode/normalizer2.h>
#  include <unicode/unistr.h>
#endif

namespace sea_g2p {

// ── Tier-1 regex (simple patterns) ───────────────────────────────────────────

static const Regex& re_extra_spaces()         { static const Regex r(R"([ \t\xA0]+)");               return r; }
static const Regex& re_extra_commas()         { static const Regex r(R"(,\s*,)");                    return r; }
static const Regex& re_multi_dot()            { static const Regex r(R"(\.[  .]*)");                 return r; }
static const Regex& re_comma_before_punct()   { static const Regex r(R"(,\s*([.!?;]))");             return r; }
static const Regex& re_space_before_punct()   { static const Regex r(R"(\s+([,.!?;:]))");            return r; }
static const Regex& re_missing_space_after_punct() { static const Regex r(R"(([.,!?;:])([^\s\d<]))"); return r; }
static const Regex& re_internal_en_tag()      { static const Regex r(R"((?i)(?s)(__start_en__.*?__end_en__|<en>.*?</en>))"); return r; }
static const Regex& re_dot_between_digits()   { static const Regex r(R"((\d+)\.(\d+))");             return r; }
static const Regex& re_entoken()              { static const Regex r(R"((?i)ENTOKEN\d+)");           return r; }
static const Regex& re_en_tag()               { static const Regex r(R"((?si)<en>.*?</en>)");        return r; }
static const Regex& re_context_tru()          {
    static const Regex r(R"((?i)\b(bằng|tính|kết quả)\s+(\d+(?:[.,]\d+)?)\s*[-–—]\s*(\d+(?:[.,]\d+)?)\b)");
    return r;
}
static const Regex& re_context_tru_post()     {
    static const Regex r(R"((?i)\b(\d+(?:[.,]\d+)?)\s*[-–—]\s*(\d+(?:[.,]\d+)?)\s+(bằng|tính|kết quả)\b)");
    return r;
}
static const Regex& re_context_den()          {
    static const Regex r(R"((?i)\b(từ|khoảng|trong)\s+(\d+(?:[.,]\d+)?)\s*[-–—]\s*(\d+(?:[.,]\d+)?)\b)");
    return r;
}
static const Regex& re_eq_minus()             { static const Regex r(R"(([\d./]+)\s*[-–—]\s*([\d./]+)\s*=)");  return r; }
static const Regex& re_eq_neg()               { static const Regex r(R"(=\s*[-–—](\d+(?:[./]\d+)?))");          return r; }
static const Regex& re_phone_with_dash()      {
    static const Regex r(R"(\b(0\d{2,3})[–\-—](\d{3,4})[–\-—](\d{4})\b)");
    return r;
}
static const Regex& re_power_of_ten_implicit() {
    static const Regex r(R"(\b10\^([-+]?\d+)\b)");
    return r;
}
static const Regex& re_to_sang()              { static const Regex r(R"(\s*(?:->|=>)\s*)");           return r; }
static const Regex& re_multi_comma()          { static const Regex r(R"(\b(\d+(?:,\d+){2,})\b)");    return r; }
static const Regex& re_numeric_dash_groups()  {
    static const Regex r(R"(\b\d+(?:[–\-—]\d+){2,}\b)");
    return r;
}
static const Regex& re_phone_space()          { static const Regex r(R"(\b0\d{2,3}(?:\s\d{3}){2}\b)"); return r; }

// ── Tier-2 regex (lookaround patterns) ───────────────────────────────────────

static const Regex& re_range()                {
    static const Regex r(
        R"((?<!\d)(?<!\d[,.])(?<![a-zA-Z])(\d{1,15}(?:[,.]\d{1,15})?)(\s*)[-–—](\s*)(\d{1,15}(?:[,.]\d{1,15})?)(?!\d)(?![.,]\d))");
    return r;
}
static const Regex& re_dash_to_comma()        {
    static const Regex r(R"((?<=\s)[-–—](?=\s))");
    return r;
}
static const Regex& re_float_with_comma()     {
    static const Regex r(R"((?<![\d.])(\d+(?:\.\d{3})*),(\d+)(%)?)");
    return r;
}
static const Regex& re_strip_dot_sep()        {
    static const Regex r(R"((?<![\d.])\d+(?:\.\d{3})+(?![\d.]))");
    return r;
}
static const Regex& re_long_num()             {
    static const Regex r(R"((?<!\d)(?<!\d[,.])([-–—]?)(\d{7,})(?!\d)(?![.,]\d))");
    return r;
}
static const Regex& re_camel_case()           {
    static const Regex r(R"((?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z]))");
    return r;
}
static const Regex& re_potential_concat()     {
    static const Regex r(R"(\b[a-zA-Z]{3,}\b)");
    return r;
}

// ── NFC normalization ─────────────────────────────────────────────────────────

static std::string to_nfc(const std::string& s) {
#ifdef SEA_G2P_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    const icu::Normalizer2* nfc = icu::Normalizer2::getNFCInstance(status);
    if (U_FAILURE(status)) return s;
    icu::UnicodeString us = icu::UnicodeString::fromUTF8(s);
    icu::UnicodeString normalized = nfc->normalize(us, status);
    if (U_FAILURE(status)) return s;
    std::string result;
    normalized.toUTF8String(result);
    return result;
#else
    return s;  // Assume input is already NFC
#endif
}

// ── Masking helpers ───────────────────────────────────────────────────────────

// Digits in mask are shifted to letters: 0→a, 1→b, … so the mask is word-char only.
static std::string make_mask(size_t idx) {
    std::string s = "mask";
    std::string num = std::to_string(idx);
    while (num.size() < 4) num = "0" + num;
    for (char c : num) s += (char)('a' + (c - '0'));
    s += "mask";
    return s;
}

// ── cleanup_whitespace ────────────────────────────────────────────────────────

static std::string cleanup_whitespace(const std::string& text) {
    std::string res = re_multi_dot().replace_all(text, ".");
    res = re_extra_spaces().replace_all(res, " ");
    res = re_extra_commas().replace_all(res, ",");
    res = re_comma_before_punct().replace_all(res, "$1");
    res = re_space_before_punct().replace_all(res, "$1");
    res = re_missing_space_after_punct().replace_all(res, "$1 $2");
    // trim spaces and leading/trailing commas
    while (!res.empty() && (res.front() == ' ' || res.front() == ',')) res.erase(res.begin());
    while (!res.empty() && (res.back()  == ' ' || res.back()  == ',')) res.pop_back();
    return res;
}

// ── split_concatenated_terms ─────────────────────────────────────────────────

static std::string split_concatenated_terms(const std::string& text) {
    return re_potential_concat().replace_all(text, [](const Match& caps) {
        const std::string& word = caps.get(0);
        // If it matches our acronym pattern, leave it alone
        auto m = re_acronym().find(word);
        if (m && m->get(0) == word) return word;
        // Otherwise split CamelCase
        return re_camel_case().replace_all(word, " ");
    });
}

// ── clean_vietnamese_text ─────────────────────────────────────────────────────

std::string clean_vietnamese_text(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> mask_map;  // (mask, original)

    auto protect = [&mask_map](std::string original) -> std::string {
        std::string mask = make_mask(mask_map.size());
        mask_map.emplace_back(mask, std::move(original));
        return mask;
    };

    std::string current = text;

    // Protect ENTOKEN placeholders
    current = re_entoken().replace_all(current, [&protect](const Match& caps) {
        std::string orig = caps.get(0);
        for (char& c : orig) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return protect(std::move(orig));
    });

    // Protect emails first
    current = re_email().replace_all(current, [&protect](const Match& caps) {
        return protect(normalize_emails(caps.get(0)));
    });

    // Protect technical strings
    current = re_technical().replace_all(current, [&protect](const Match& caps) {
        const std::string& orig = caps.get(0);
        // Check if it matches a known exception acronym
        auto m = re_acronyms_exceptions().find(orig);
        if (m && m->get(0) == orig) {
            auto it = combined_exceptions().find(orig);
            return protect(it != combined_exceptions().end() ? it->second : orig);
        }
        return protect(normalize_technical(orig));
    });

    current = split_concatenated_terms(current);

    // Core normalization passes
    current = expand_power_of_ten(current);

    current = re_multiply().replace_all(current, [](const Match& caps) {
        return expand_multiply_number(caps.get(0));
    });

    current = re_context_tru().replace_all(current,      " $1 $2 trừ $3 ");
    current = re_context_tru_post().replace_all(current, " $1 trừ $2 $3 ");
    current = re_context_den().replace_all(current,      " $1 $2 đến $3 ");

    current = re_eq_minus().replace_all(current, [](const Match& caps) {
        return caps.get(1) + " trừ " + caps.get(2) + " =";
    });
    current = re_eq_neg().replace_all(current, [](const Match& caps) {
        return "= âm " + caps.get(1);
    });

    current = expand_abbreviations(current);
    current = expand_scientific_notation(current);

    current = normalize_date(current);
    current = normalize_time(current);

    // Numeric dash groups: "1-2-3" → "một, hai, ba"
    current = re_numeric_dash_groups().replace_all(current, [](const Match& caps) {
        const std::string& matched = caps.get(0);
        // Split on dashes
        std::vector<std::string> parts;
        std::string part;
        for (char c : matched) {
            if (c == '-' || c == '\xe2' /* start of – or — in UTF-8, handled below */) {
                if (!part.empty()) { parts.push_back(part); part.clear(); }
            } else {
                part += c;
            }
        }
        // Handle multi-byte dashes via a simpler approach: just split on any dash char
        // Re-do with proper UTF-8 awareness
        parts.clear();
        std::string buf;
        const char* ptr = matched.data();
        const char* end = ptr + matched.size();
        while (ptr < end) {
            const char* prev = ptr;
            uint32_t cp = utf8_next(ptr, end);
            if (cp == '-' || cp == 0x2013 || cp == 0x2014) {
                if (!buf.empty()) { parts.push_back(buf); buf.clear(); }
            } else {
                buf.append(prev, ptr - prev);
            }
        }
        if (!buf.empty()) parts.push_back(buf);

        std::string r;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) r += ", ";
            r += n2w_single(parts[i]);
        }
        return r;
    });

    // Phone numbers with spaces: "0123 456 789"
    current = re_phone_space().replace_all(current, [](const Match& caps) {
        const std::string& matched = caps.get(0);
        // Split on whitespace
        std::istringstream ss(matched);
        std::string tok;
        std::vector<std::string> parts;
        while (ss >> tok) parts.push_back(tok);
        std::string r;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) r += ", ";
            r += n2w_single(parts[i]);
        }
        return r;
    });

    // Phone with dash: "0123-456-7890"
    current = re_phone_with_dash().replace_all(current, [](const Match& caps) {
        return " " + n2w_single(caps.get(1)) + ", "
                   + n2w_single(caps.get(2)) + ", "
                   + n2w_single(caps.get(3)) + " ";
    });

    // Implicit 10^ notation
    current = re_power_of_ten_implicit().replace_all(current, [](const Match& caps) {
        std::string exp = caps.get(1);
        if (!exp.empty() && exp[0] == '-')
            return "mười mũ trừ " + n2w(exp.substr(1));
        std::string ev = exp;
        ev.erase(std::remove(ev.begin(), ev.end(), '+'), ev.end());
        return "mười mũ " + n2w(ev);
    });

    // Numeric ranges: "10-20" → "mười đến hai mươi"
    current = re_range().replace_all(current, [](const Match& caps) {
        std::string n1_raw = caps.get(1);
        std::string s1     = caps.get(2);
        std::string s2     = caps.get(3);
        std::string n2_raw = caps.get(4);
        if (!s1.empty() && s2.empty()) return caps.get(0);
        std::string n1 = n1_raw, n2 = n2_raw;
        for (char& c : n1) if (c == ',' || c == '.') c = 0;
        n1.erase(std::remove(n1.begin(), n1.end(), (char)0), n1.end());
        for (char& c : n2) if (c == ',' || c == '.') c = 0;
        n2.erase(std::remove(n2.begin(), n2.end(), (char)0), n2.end());
        int diff = static_cast<int>(n1.size()) - static_cast<int>(n2.size());
        if (diff < 0) diff = -diff;
        if (diff <= 1) return " " + n1_raw + " đến " + n2_raw + " ";
        return " " + n1_raw + " " + n2_raw + " ";
    });

    current = re_dash_to_comma().replace_all(current, ",");
    current = re_to_sang().replace_all(current, " sang ");

    current = expand_scientific_notation(current);
    current = expand_compound_units(current);
    current = expand_units_and_currency(current);

    // Long numbers (7+ digits)
    current = re_long_num().replace_all(current, [](const Match& caps) {
        std::string neg     = caps.get(1);
        std::string num_str = caps.get(2);
        std::string neg_prefix = neg.empty() ? "" : "âm ";
        return " " + neg_prefix + n2w_single(num_str) + " ";
    });

    current = fix_english_style_numbers(current);

    // Multi-comma sequences: "1,2,3" → "một phẩy hai phẩy ba"
    current = re_multi_comma().replace_all(current, [](const Match& caps) {
        const std::string& matched = caps.get(1);
        std::vector<std::string> parts;
        std::istringstream ss(matched);
        std::string tok;
        while (std::getline(ss, tok, ',')) parts.push_back(tok);
        std::string r;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) r += " phẩy ";
            r += n2w_decimal(parts[i]);
        }
        return r;
    });

    // Float with comma decimal: "1.234,56" → "một nghìn hai trăm ba mươi tư phẩy năm sáu"
    current = re_float_with_comma().replace_all(current, [](const Match& caps) {
        std::string int_raw = caps.get(1);
        std::string dec_raw = caps.get(2);
        std::string pct     = caps.get(3);
        // Remove '.' from integer part (it's a thousands separator)
        std::string int_clean = int_raw;
        int_clean.erase(std::remove(int_clean.begin(), int_clean.end(), '.'), int_clean.end());
        std::string int_part = n2w(int_clean);
        // Trim trailing zeros from decimal
        std::string dec = dec_raw;
        while (!dec.empty() && dec.back() == '0') dec.pop_back();
        std::string res = dec.empty()
            ? int_part
            : int_part + " phẩy " + n2w_decimal(dec);
        if (!pct.empty()) res += " phần trăm";
        return " " + res + " ";
    });

    // Strip dot thousands separators: "1.234.567" → "1234567"
    current = re_strip_dot_sep().replace_all(current, [](const Match& caps) {
        std::string r = caps.get(0);
        r.erase(std::remove(r.begin(), r.end(), '.'), r.end());
        return r;
    });

    current = normalize_others(current);
    current = normalize_number_vi(current);

    // Protect internal <en> / __start_en__ tags
    current = re_internal_en_tag().replace_all(current, [&protect](const Match& caps) {
        return protect(caps.get(0));
    });

    current = expand_standalone_letters(current);

    // Dots between digits: "3.14" → "3 chấm 14"  (after number normalization)
    if (current.find('.') != std::string::npos) {
        current = re_dot_between_digits().replace_all(current, [](const Match& caps) {
            return caps.get(1) + " chấm " + caps.get(2);
        });
    }

    // Restore masked strings
    for (auto it = mask_map.rbegin(); it != mask_map.rend(); ++it) {
        const auto& [mask, original] = *it;
        auto pos = current.find(mask);
        while (pos != std::string::npos) {
            current.replace(pos, mask.size(), original);
            pos = current.find(mask, pos + original.size());
        }
        // Also try lowercase mask
        std::string mask_lo = mask;
        for (char& c : mask_lo) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        pos = current.find(mask_lo);
        while (pos != std::string::npos) {
            current.replace(pos, mask_lo.size(), original);
            pos = current.find(mask_lo, pos + original.size());
        }
    }

    // Restore __start_en__ / __end_en__ to <en> / </en>
    {
        std::string::size_type pos = 0;
        while ((pos = current.find("__start_en__", pos)) != std::string::npos) {
            current.replace(pos, 12, "<en>");
            pos += 4;
        }
        pos = 0;
        while ((pos = current.find("__end_en__", pos)) != std::string::npos) {
            current.replace(pos, 10, "</en>");
            pos += 5;
        }
    }

    // Replace underscores and hyphens with spaces
    std::replace(current.begin(), current.end(), '_', ' ');
    std::replace(current.begin(), current.end(), '-', ' ');

    current = cleanup_whitespace(current);

    // Lowercase everything
    // (A proper lowercase for Vietnamese requires UTF-8 aware tolower.
    //  Here we do ASCII tolower and leave Vietnamese chars as-is; they
    //  are already lowercase after normalization.)
    std::string lower;
    lower.reserve(current.size());
    const char* ptr = current.data();
    const char* end = ptr + current.size();
    while (ptr < end) {
        const char* prev = ptr;
        uint32_t cp = utf8_next(ptr, end);
        if (cp >= 'A' && cp <= 'Z') {
            lower += (char)(cp + 32);
        } else {
            lower.append(prev, ptr - prev);
        }
    }
    return lower;
}

// ── Normalizer ────────────────────────────────────────────────────────────────

Normalizer::Normalizer(std::string lang, std::string ignore) 
    : lang_(std::move(lang)), ignore_(std::move(ignore)) 
{
    if (!ignore_.empty()) {
        std::vector<uint32_t> cps = utf8_codepoints(ignore_);
        std::string pattern = "(?si)(";
        bool first = true;
        for (size_t i = 0; i + 1 < cps.size(); i += 2) {
            if (!first) pattern += "|";
            first = false;

            // Pattern for pair cps[i]...cps[i+1]
            // We need to convert each codepoint back to UTF-8 to escape it
            auto cp_to_utf8 = [](uint32_t cp) {
                std::string s;
                if (cp < 0x80) s += (char)cp;
                else if (cp < 0x800) {
                    s += (char)(0xc0 | (cp >> 6));
                    s += (char)(0x80 | (cp & 0x3f));
                } else if (cp < 0x10000) {
                    s += (char)(0xe0 | (cp >> 12));
                    s += (char)(0x80 | ((cp >> 6) & 0x3f));
                    s += (char)(0x80 | (cp & 0x3f));
                } else {
                    s += (char)(0xf0 | (cp >> 18));
                    s += (char)(0x80 | ((cp >> 12) & 0x3f));
                    s += (char)(0x80 | ((cp >> 6) & 0x3f));
                    s += (char)(0x80 | (cp & 0x3f));
                }
                return s;
            };

            pattern += regex_escape(cp_to_utf8(cps[i])) + ".*?" + regex_escape(cp_to_utf8(cps[i+1]));
        }
        pattern += ")";
        if (pattern != "(?si)() ") { // simplified check
             try {
                ignore_re_ = std::make_unique<Regex>(pattern);
             } catch (...) {}
        }
    }
}

Normalizer::~Normalizer() = default;

std::string Normalizer::normalize(const std::string& text) const {
    if (text.empty()) return {};

    // NFC normalization
    std::string nfc_text = to_nfc(text);

    // Protect <en> tags before normalization
    std::vector<std::string> en_contents;
    std::string current = re_en_tag().replace_all(nfc_text, [&en_contents](const Match& caps) {
        en_contents.push_back(caps.get(0));
        return "ENTOKEN" + std::to_string(en_contents.size() - 1);
    });

    if (ignore_re_) {
        current = ignore_re_->replace_all(current, [&en_contents](const Match& caps) {
            en_contents.push_back(caps.get(0));
            return "ENTOKEN" + std::to_string(en_contents.size() - 1);
        });
    }

    current = clean_vietnamese_text(current);

    // Collapse extra spaces
    current = re_extra_spaces().replace_all(current, " ");
    while (!current.empty() && current.front() == ' ') current.erase(current.begin());
    while (!current.empty() && current.back()  == ' ') current.pop_back();

    // Restore <en> tags
    if (!en_contents.empty()) {
        for (size_t idx = 0; idx < en_contents.size(); ++idx) {
            std::string placeholder = "entoken" + std::to_string(idx);  // lowercased by clean_vi
            std::string::size_type pos = current.find(placeholder);
            while (pos != std::string::npos) {
                current.replace(pos, placeholder.size(), en_contents[idx]);
                pos = current.find(placeholder, pos + en_contents[idx].size());
            }
        }
    }

    current = re_extra_spaces().replace_all(current, " ");
    while (!current.empty() && current.front() == ' ') current.erase(current.begin());
    while (!current.empty() && current.back()  == ' ') current.pop_back();
    return current;
}

std::vector<std::string> Normalizer::normalize_batch(
    const std::vector<std::string>& texts) const
{
    std::vector<std::string> results(texts.size());
    unsigned int nthreads = std::max(1u, std::thread::hardware_concurrency());
    size_t n = texts.size();

    if (n <= nthreads || nthreads == 1) {
        for (size_t i = 0; i < n; ++i)
            results[i] = normalize(texts[i]);
        return results;
    }

    // Parallel batch using std::async
    std::vector<std::future<void>> futures;
    size_t chunk = (n + nthreads - 1) / nthreads;
    for (size_t t = 0; t < nthreads; ++t) {
        size_t start = t * chunk;
        size_t end   = std::min(start + chunk, n);
        if (start >= end) break;
        futures.push_back(std::async(std::launch::async, [&, start, end]() {
            for (size_t i = start; i < end; ++i)
                results[i] = normalize(texts[i]);
        }));
    }
    for (auto& f : futures) f.get();
    return results;
}

}  // namespace sea_g2p
