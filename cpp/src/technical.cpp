// technical.cpp — URL/email/path normalization (maps to src/vi_normalizer/technical.rs)
#include "sea_g2p/technical.hpp"
#include "sea_g2p/regex_wrapper.hpp"
#include "sea_g2p/num2vi.hpp"
#include "sea_g2p/resources.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sea_g2p {

// ── Regex patterns ────────────────────────────────────────────────────────────

const Regex& re_technical() {
    static const Regex r(R"((?ix)
        \b(?:https?|ftp)://[A-Za-z0-9.\-_~:/?#\[\]@!$&'()*+,;=]+\b
        |
        \b(?:www\.)[A-Za-z0-9.\-_~:/?#\[\]@!$&'()*+,;=]+\b
        |
        \b[A-Za-z0-9.\-]+(?:\.com|\.vn|\.net|\.org|\.gov|\.io|\.biz|\.info)(?:/[A-Za-z0-9.\-_~:/?#\[\]@!$&'()*+,;=]*)?\b
        |
        (?<!\w)/[a-zA-Z0-9._\-/]{2,}\b
        |
        \b[a-zA-Z]:\\[a-zA-Z0-9._\\\-]+\b
        |
        \b[a-zA-Z0-9._\-]+\.(?:txt|log|tar|gz|zip|sh|py|js|cpp|h|json|xml|yaml|yml|md|csv|pdf|docx|xlsx|exe|dll|so|config)\b
        |
        \b[a-zA-Z][a-zA-Z0-9]*(?:[._\-][a-zA-Z0-9]+){2,}\b
        |
        \b[a-fA-F0-9]{1,4}(?::[a-fA-F0-9]{1,4}){3,7}\b
    )");
    return r;
}

const Regex& re_email() {
    static const Regex r(R"((?i)\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)");
    return r;
}

static const Regex& re_tech_split() {
    static const Regex r(R"([./:?&=/_ \-\\#])");
    return r;
}
static const Regex& re_email_split() {
    static const Regex r(R"([._\-+])");
    return r;
}
static const Regex& re_sub_tokens() {
    static const Regex r(R"([a-zA-Z]+|\d+)");
    return r;
}
static const Regex& re_slash_number() {
    static const Regex r(R"((?<![a-zA-Z\d,.])(\d+)/(\d+)(?![\d,.]))");
    return r;
}
static const Regex& re_neg_frac() {
    static const Regex r(R"((?:=|\s)-((\d+)/(\d+)))");
    return r;
}
static const Regex& re_slash_alnum() {
    static const Regex r(R"((?<![a-zA-Z\d,.])(\d+)/(\d+[a-zA-Z][a-zA-Z0-9]*))");
    return r;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// Normalise a single alphanumeric segment: letters→English tag, digits→individual words
static std::string norm_segment(const std::string& s) {
    if (s.empty()) return {};
    // All ASCII digits
    bool all_digit = true;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) { all_digit = false; break; }
    }
    if (all_digit) return n2w(s);

    // All ASCII alphanumeric
    bool all_ascii_alnum = true;
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) || !isascii(c))
        { all_ascii_alnum = false; break; }
    }

    if (all_ascii_alnum) {
        auto toks = re_sub_tokens().find_all(s);
        if (toks.size() > 1) {
            std::string r;
            for (const auto& t : toks) {
                std::string tv = t.get(0);
                bool td = true;
                for (char c : tv) if (!std::isdigit(static_cast<unsigned char>(c))) { td = false; break; }
                if (!r.empty()) r += ' ';
                if (td) r += n2w(tv);
                else {
                    std::string vl = tv;
                    for (char& c : vl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    r += "__start_en__" + vl + "__end_en__";
                }
            }
            return r;
        }
        std::string sl = s;
        for (char& c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return "__start_en__" + sl + "__end_en__";
    }

    // Has non-ASCII: char-by-char
    std::string res;
    const char* ptr = s.data();
    const char* end = ptr + s.size();
    while (ptr < end) {
        uint32_t cp = utf8_next(ptr, end);
        if (cp >= '0' && cp <= '9') {
            if (!res.empty()) res += ' ';
            res += n2w_single(std::string(1, static_cast<char>(cp)));
        } else if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')) {
            char cl = static_cast<char>(std::tolower(static_cast<unsigned char>(cp)));
            auto it = vi_letter_names().find(std::string(1, cl));
            if (!res.empty()) res += ' ';
            res += it != vi_letter_names().end() ? it->second : std::string(1, cl);
        } else {
            // re-encode codepoint back to UTF-8
            if (!res.empty()) res += ' ';
            if (cp < 0x80) {
                res += static_cast<char>(cp);
            } else if (cp < 0x800) {
                res += static_cast<char>(0xC0 | (cp >> 6));
                res += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                res += static_cast<char>(0xE0 | (cp >> 12));
                res += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                res += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
    }
    return res;
}

// Split string by regex into [text, sep, text, sep, …] interleaved
static std::vector<std::string> split_with_seps(const Regex& re, const std::string& s) {
    std::vector<std::string> parts;
    size_t last = 0;
    for (const auto& m : re.find_all(s)) {
        parts.push_back(s.substr(last, m.start(0) - last));
        parts.push_back(m.get(0));
        last = m.end(0);
    }
    parts.push_back(s.substr(last));
    return parts;
}

// Process tech-style segments with "." looking up domain suffixes
static std::string process_tech_segments(const std::vector<std::string>& segs) {
    std::vector<std::string> res;
    size_t idx = 0;
    while (idx < segs.size()) {
        const std::string& s = segs[idx];
        if (s.empty()) { ++idx; continue; }

        if (s == ".") {
            // Look ahead for domain suffix
            std::string next_seg;
            for (size_t j = idx + 1; j < segs.size(); ++j) {
                if (!segs[j].empty() && segs[j] != "." && segs[j] != "/" && segs[j] != "\\"
                    && segs[j] != "-" && segs[j] != "_" && segs[j] != ":"
                    && segs[j] != "?" && segs[j] != "&" && segs[j] != "="
                    && segs[j] != "#") {
                    next_seg = segs[j];
                    break;
                }
            }
            std::string nsl = next_seg;
            for (char& c : nsl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!next_seg.empty() && domain_suffix_map().count(nsl)) {
                res.push_back("chấm");
                res.push_back(domain_suffix_map().at(nsl));
                ++idx;
                // Skip past the domain suffix segment
                while (idx < segs.size()) {
                    std::string sl = segs[idx];
                    for (char& c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (sl == nsl) { ++idx; break; }
                    ++idx;
                }
                continue;
            }
            res.push_back("chấm");
        } else if (s == "/" || s == "\\") {
            res.push_back("gạch");
        } else if (s == "-") {
            res.push_back("gạch ngang");
        } else if (s == "_") {
            res.push_back("gạch dưới");
        } else if (s == ":") {
            res.push_back("hai chấm");
        } else if (s == "?") {
            res.push_back("hỏi");
        } else if (s == "&") {
            res.push_back("và");
        } else if (s == "=") {
            res.push_back("bằng");
        } else if (s == "#") {
            res.push_back("thăng");
        } else if (s == " ") {
            // skip spaces in split separators
        } else {
            // Check domain suffix
            std::string sl = s;
            for (char& c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (domain_suffix_map().count(sl)) {
                res.push_back(domain_suffix_map().at(sl));
            } else {
                // Check if all ASCII alphanumeric
                bool all_asc = true;
                for (char c : s)
                    if (!std::isalnum(static_cast<unsigned char>(c)) || !isascii(c))
                    { all_asc = false; break; }

                if (all_asc) {
                    bool all_dig = true;
                    for (char c : s)
                        if (!std::isdigit(static_cast<unsigned char>(c))) { all_dig = false; break; }
                    if (all_dig) {
                        // digit-by-digit
                        std::string r;
                        for (char c : s) {
                            if (!r.empty()) r += ' ';
                            r += n2w_single(std::string(1, c));
                        }
                        res.push_back(r);
                    } else {
                        auto toks = re_sub_tokens().find_all(s);
                        if (toks.size() > 1) {
                            for (const auto& t : toks) {
                                std::string tv = t.get(0);
                                bool td = true;
                                for (char c : tv)
                                    if (!std::isdigit(static_cast<unsigned char>(c))) { td = false; break; }
                                if (td) {
                                    std::string r;
                                    for (char c : tv) {
                                        if (!r.empty()) r += ' ';
                                        r += n2w_single(std::string(1, c));
                                    }
                                    res.push_back(r);
                                } else {
                                    std::string vl = tv;
                                    for (char& c : vl)
                                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                                    bool all_upper = true;
                                    for (char c : tv) if (!std::isupper(static_cast<unsigned char>(c))) { all_upper = false; break; }
                                    if ((all_upper && tv.size() <= 4) || tv.size() <= 2)
                                        vl = [&vl]() {
                                            std::string r;
                                            for (size_t i = 0; i < vl.size(); ++i) {
                                                if (i) r += ' ';
                                                r += vl[i];
                                            }
                                            return r;
                                        }();
                                    res.push_back("__start_en__" + vl + "__end_en__");
                                }
                            }
                        } else {
                            std::string vl = sl;
                            bool all_upper = true;
                            for (char c : s) if (!std::isupper(static_cast<unsigned char>(c))) { all_upper = false; break; }
                            if ((all_upper && s.size() <= 4) || s.size() <= 2)
                                vl = [&vl]() {
                                    std::string r;
                                    for (size_t i = 0; i < vl.size(); ++i) {
                                        if (i) r += ' ';
                                        r += vl[i];
                                    }
                                    return r;
                                }();
                            res.push_back("__start_en__" + vl + "__end_en__");
                        }
                    }
                } else {
                    // Non-ASCII: letter-by-letter
                    const char* ptr = s.data();
                    const char* end = ptr + s.size();
                    while (ptr < end) {
                        uint32_t cp = utf8_next(ptr, end);
                        if (std::isalnum(static_cast<unsigned char>(cp < 128 ? (char)cp : 0))) {
                            if (cp >= '0' && cp <= '9') {
                                res.push_back(n2w_single(std::string(1, static_cast<char>(cp))));
                            } else {
                                char cl = static_cast<char>(std::tolower(static_cast<unsigned char>(cp)));
                                auto it = vi_letter_names().find(std::string(1, cl));
                                res.push_back(it != vi_letter_names().end()
                                    ? it->second : std::string(1, cl));
                            }
                        }
                    }
                }
            }
        }
        ++idx;
    }

    // Join and collapse double spaces
    std::string out;
    for (const auto& r : res) {
        if (!out.empty() && !r.empty()) out += ' ';
        out += r;
    }
    // Collapse multiple spaces
    std::string final_out;
    bool last_sp = false;
    for (char c : out) {
        if (c == ' ') { if (!last_sp) final_out += c; last_sp = true; }
        else { final_out += c; last_sp = false; }
    }
    while (!final_out.empty() && final_out.front() == ' ') final_out.erase(final_out.begin());
    while (!final_out.empty() && final_out.back()  == ' ') final_out.pop_back();
    return final_out;
}

// ── normalize_technical ───────────────────────────────────────────────────────

std::string normalize_technical(const std::string& text) {
    return re_technical().replace_all(text, [](const Match& caps) {
        const std::string& orig = caps.get(0);
        std::string rest = orig;
        std::vector<std::string> res;

        std::string orig_lower = orig;
        for (char& c : orig_lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        auto proto_idx = orig_lower.find("://");
        if (proto_idx != std::string::npos) {
            std::string protocol = orig.substr(0, proto_idx);
            // Short protocols: spell out letters
            bool all_up = true;
            for (char c : protocol)
                if (!std::isupper(static_cast<unsigned char>(c))) { all_up = false; break; }
            std::string p_norm;
            if ((all_up && protocol.size() <= 4) || protocol.size() <= 3) {
                std::string pl = protocol;
                for (char& c : pl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (size_t i = 0; i < pl.size(); ++i) {
                    if (i) p_norm += ' ';
                    p_norm += pl[i];
                }
            } else {
                p_norm = orig_lower.substr(0, proto_idx);
            }
            res.push_back("__start_en__" + p_norm + "__end_en__");
            rest = orig.substr(proto_idx + 3);
        } else if (!orig.empty() && orig[0] == '/') {
            res.push_back("gạch");
            rest = orig.substr(1);
        }

        auto segs = split_with_seps(re_tech_split(), rest);
        std::string seg_result = process_tech_segments(segs);

        if (!seg_result.empty()) {
            if (!res.empty()) res.push_back(" ");
            res.push_back(seg_result);
        }

        std::string out;
        for (const auto& r : res) out += r;
        // Collapse spaces
        std::string final_out;
        bool last_sp = false;
        for (char c : out) {
            if (c == ' ') { if (!last_sp) final_out += c; last_sp = true; }
            else { final_out += c; last_sp = false; }
        }
        while (!final_out.empty() && final_out.front() == ' ') final_out.erase(final_out.begin());
        while (!final_out.empty() && final_out.back()  == ' ') final_out.pop_back();
        return final_out;
    });
}

// ── normalize_emails ──────────────────────────────────────────────────────────

std::string normalize_emails(const std::string& text) {
    return re_email().replace_all(text, [](const Match& caps) {
        const std::string& email = caps.get(0);
        auto at = email.find('@');
        if (at == std::string::npos) return email;
        std::string user_part   = email.substr(0, at);
        std::string domain_part = email.substr(at + 1);

        // Process a segment of the user/domain part
        auto process_part = [&](const std::string& p, bool is_domain) -> std::string {
            auto segs = split_with_seps(re_email_split(), p);
            std::vector<std::string> res;
            size_t idx = 0;
            while (idx < segs.size()) {
                const std::string& s = segs[idx];
                if (s.empty()) { ++idx; continue; }
                if (s == ".") {
                    if (is_domain) {
                        // Look ahead for domain suffix
                        std::string next_seg;
                        size_t peek_idx = idx + 1;
                        while (peek_idx < segs.size()) {
                            if (!segs[peek_idx].empty()
                                && segs[peek_idx] != "."
                                && segs[peek_idx] != "_"
                                && segs[peek_idx] != "-"
                                && segs[peek_idx] != "+") {
                                next_seg = segs[peek_idx];
                                break;
                            }
                            ++peek_idx;
                        }
                        std::string nsl = next_seg;
                        for (char& c : nsl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (!next_seg.empty() && domain_suffix_map().count(nsl)) {
                            res.push_back("chấm");
                            res.push_back(domain_suffix_map().at(nsl));
                            idx = peek_idx + 1;
                            continue;
                        }
                    }
                    res.push_back("chấm");
                } else if (s == "_") {
                    res.push_back("gạch dưới");
                } else if (s == "-") {
                    res.push_back("gạch ngang");
                } else if (s == "+") {
                    res.push_back("cộng");
                } else {
                    res.push_back(norm_segment(s));
                }
                ++idx;
            }
            std::string out;
            for (size_t i = 0; i < res.size(); ++i) {
                if (i) out += ' ';
                out += res[i];
            }
            return out;
        };

        std::string user_norm = process_part(user_part, false);
        std::string dp_lower  = domain_part;
        for (char& c : dp_lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string domain_norm;
        auto it = common_email_domains().find(dp_lower);
        if (it != common_email_domains().end()) {
            domain_norm = it->second;
        } else {
            domain_norm = process_part(domain_part, true);
        }

        std::string r = user_norm + " a còng " + domain_norm;
        // Collapse double spaces
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
}

// ── normalize_slashes ─────────────────────────────────────────────────────────

std::string normalize_slashes(const std::string& text) {
    // Negative fractions: "= -3/4" → "= âm 3/4"
    std::string res = [&text]() {
        static const Regex re_neg_f(R"((?:=|\s)-((\d+)/(\d+)))");
        return re_neg_f.replace_all(text, [&text](const Match& caps) {
            std::string full  = caps.get(0);
            std::string frac  = caps.get(1);
            std::string prefix = (full[0] == '=') ? "= âm " : " âm ";
            return prefix + frac;
        });
    }();

    // 225/45R17 style
    res = re_slash_alnum().replace_all(res, [](const Match& caps) {
        std::string n1    = caps.get(1);
        std::string alnum = caps.get(2);  // e.g. "45R17"
        auto toks = re_sub_tokens().find_all(alnum);
        std::vector<std::string> spoken;
        for (const auto& t : toks) {
            std::string tv = t.get(0);
            bool all_dig = true;
            for (char c : tv) if (!std::isdigit(static_cast<unsigned char>(c))) { all_dig = false; break; }
            if (all_dig) {
                spoken.push_back(n2w(tv));
            } else {
                std::string r;
                for (char c : tv) {
                    std::string cl(1, static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                    auto it = vi_letter_names().find(cl);
                    if (!r.empty()) r += ' ';
                    r += it != vi_letter_names().end() ? it->second : cl;
                }
                spoken.push_back(r);
            }
        }
        std::string alnum_spoken;
        for (size_t i = 0; i < spoken.size(); ++i) {
            if (i) alnum_spoken += ' ';
            alnum_spoken += spoken[i];
        }
        return n2w(n1) + " trên " + alnum_spoken;
    });

    // Plain fraction: "3/4" → "ba trên tư"
    res = re_slash_number().replace_all(res, [](const Match& caps) {
        return n2w(caps.get(1)) + " trên " + n2w(caps.get(2));
    });

    return res;
}

}  // namespace sea_g2p
