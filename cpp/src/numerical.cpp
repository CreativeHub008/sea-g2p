// numerical.cpp — numeric text normalization (maps to src/vi_normalizer/numerical.rs)
#include "sea_g2p/numerical.hpp"
#include "sea_g2p/num2vi.hpp"

#include <algorithm>
#include <cctype>

namespace sea_g2p {

// ── Regex patterns ────────────────────────────────────────────────────────────

const Regex& re_multiply() {
    static const Regex r(
        R"((?i)\d+(?:\s*[a-zA-Zμµ²³°]+\d*)?(?:\s*[x×]\s*\d+(?:\s*[a-zA-Zμµ²³°]+\d*)?)+)");
    return r;
}

static const Regex& re_expand_multiply() {
    static const Regex r(
        R"((?i)(?<=\d|[a-zA-Zμµ²³°])\s*[x×]\s*(?=\d))");
    return r;
}

static const Regex& re_ordinal() {
    static const Regex r(R"((?i)(thứ|hạng)(\s+)(\d+)\b)");
    return r;
}

static const Regex& re_phone() {
    static const Regex r(R"(((\+84|84|0|0084)(3|5|7|8|9)[0-9]{8}))");
    return r;
}

static const Regex& re_dot_sep() {
    static const Regex r(R"(\d+(\.\d{3})+)");
    return r;
}

static const Regex& re_number() {
    // General number: optional negative sign, digits with various separators
    static const Regex r(
        R"((?<!\d)(?P<neg>[-–—])?(\d+(?:,\d+|(?:\.\d{3})+(?!\d)|\.\d+|(?:\s\d{3})+(?!\d))?)(?!\d))");
    return r;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string normalize_dot_sep(const std::string& number) {
    auto m = re_dot_sep().find(number);
    if (m && m->get(0) == number) {
        std::string r = number;
        r.erase(std::remove(r.begin(), r.end(), '.'), r.end());
        return r;
    }
    return number;
}

// ── num_to_words ──────────────────────────────────────────────────────────────

std::string num_to_words(const std::string& number, bool negative) {
    // Leading-zero integers → digit-by-digit
    bool has_dot   = number.find('.') != std::string::npos;
    bool has_comma = number.find(',') != std::string::npos;
    bool has_space = number.find(' ') != std::string::npos;

    if (!has_dot && !has_comma && !has_space
        && number[0] == '0' && number.size() > 1) {
        std::string prefix = negative ? "âm " : "";
        std::string r = prefix + n2w_single(number);
        // trim
        while (!r.empty() && r.front() == ' ') r.erase(r.begin());
        while (!r.empty() && r.back()  == ' ') r.pop_back();
        return r;
    }

    if (has_dot) {
        bool is_thousands = false;
        auto m = re_dot_sep().find(number);
        if (m) is_thousands = (m->get(0) == number);

        if (!is_thousands) {
            auto sep = number.find('.');
            if (sep != std::string::npos) {
                std::string int_p = number.substr(0, sep);
                std::string dec_p = number.substr(sep + 1);
                std::string prefix = negative ? "âm " : "";
                std::string r = prefix + n2w(int_p) + " chấm " + n2w_decimal(dec_p);
                while (!r.empty() && r.front() == ' ') r.erase(r.begin());
                while (!r.empty() && r.back()  == ' ') r.pop_back();
                return r;
            }
        }
    }

    // Remove dots as thousands separator, remove spaces
    std::string no_dots = normalize_dot_sep(number);
    no_dots.erase(std::remove(no_dots.begin(), no_dots.end(), ' '), no_dots.end());

    if (no_dots.find(',') != std::string::npos) {
        auto sep = no_dots.find(',');
        std::string int_p = no_dots.substr(0, sep);
        std::string dec_p = no_dots.substr(sep + 1);
        std::string prefix = negative ? "âm " : "";
        std::string r = prefix + n2w(int_p) + " phẩy " + n2w_decimal(dec_p);
        while (!r.empty() && r.front() == ' ') r.erase(r.begin());
        while (!r.empty() && r.back()  == ' ') r.pop_back();
        return r;
    }

    if (negative) {
        std::string r = "âm " + n2w(no_dots);
        while (!r.empty() && r.front() == ' ') r.erase(r.begin());
        while (!r.empty() && r.back()  == ' ') r.pop_back();
        return r;
    }
    return n2w(no_dots);
}

// ── expand_multiply_number ────────────────────────────────────────────────────

std::string expand_multiply_number(const std::string& text) {
    return re_expand_multiply().replace_all(text, " nhân ");
}

// ── normalize_number_vi ───────────────────────────────────────────────────────

std::string normalize_number_vi(const std::string& text) {
    std::string result = text;

    // Ordinals: "thứ 5" → "thứ năm", "thứ 1" → "thứ nhất", "thứ 4" → "thứ tư"
    result = re_ordinal().replace_all(result, [](const Match& caps) {
        std::string prefix = caps.get(1);
        std::string space  = caps.get(2);
        std::string number = caps.get(3);
        if (number == "1") return prefix + space + "nhất";
        if (number == "4") return prefix + space + "tư";
        return prefix + space + n2w(number);
    });

    // Phone numbers
    result = re_phone().replace_all(result, [](const Match& caps) {
        return n2w_single(caps.get(0));
    });

    // General numbers
    std::string temp = result;
    result = re_number().replace_all(temp, [&temp](const Match& caps) {
        const std::string& full = caps.get(0);
        size_t start = caps.start(0);

        // Check what character precedes this match in the subject
        char prefix_char = 0;
        if (start > 0 && start <= temp.size()) {
            // Walk back one byte (ASCII safe for this purpose)
            prefix_char = temp[start - 1];
        }

        std::string neg_sym = caps.get("neg");
        std::string number_str = caps.get(2);

        bool is_neg = false;
        if (!neg_sym.empty()) {
            bool prefix_ok = (prefix_char == 0
                || std::isspace(static_cast<unsigned char>(prefix_char))
                || std::string("([;,. ").find(prefix_char) != std::string::npos);
            if (prefix_ok) is_neg = true;
        }

        std::string word = num_to_words(number_str, is_neg);
        if (!neg_sym.empty() && !is_neg) {
            return neg_sym + word;
        }
        return " " + word + " ";
    });

    return result;
}

}  // namespace sea_g2p
