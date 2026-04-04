// regex_wrapper.cpp — PCRE2-backed implementation of regex_wrapper.hpp
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "sea_g2p/regex_wrapper.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace sea_g2p {

// ── Impl ──────────────────────────────────────────────────────────────────────
struct Regex::Impl {
    pcre2_code* code{nullptr};
    uint32_t    capture_count{0};
    // name table: name → group index
    std::vector<std::pair<std::string, int>> named;

    ~Impl() {
        if (code) { pcre2_code_free(code); code = nullptr; }
    }

    void build_name_table() {
        uint32_t name_count = 0, name_entry_size = 0;
        pcre2_pattern_info(code, PCRE2_INFO_NAMECOUNT,     &name_count);
        pcre2_pattern_info(code, PCRE2_INFO_NAMEENTRYSIZE, &name_entry_size);
        if (name_count == 0 || name_entry_size == 0) return;

        PCRE2_SPTR table = nullptr;
        pcre2_pattern_info(code, PCRE2_INFO_NAMETABLE, &table);
        if (!table) return;

        for (uint32_t i = 0; i < name_count; ++i) {
            const uint8_t* entry = table + i * name_entry_size;
            int group = (entry[0] << 8) | entry[1];
            std::string name(reinterpret_cast<const char*>(entry + 2));
            named.emplace_back(std::move(name), group);
        }
    }
};

// ── Match helpers ─────────────────────────────────────────────────────────────
bool Match::has(int idx) const noexcept {
    if (idx < 0 || idx >= static_cast<int>(groups.size())) return false;
    return groups[idx].matched();
}

std::string Match::get(int idx) const {
    if (!has(idx)) return {};
    const auto& g = groups[idx];
    return subject.substr(g.start, g.end - g.start);
}

std::string Match::get(const std::string& name) const {
    for (const auto& [n, idx] : named_groups) {
        if (n == name) return get(idx);
    }
    return {};
}

size_t Match::start(int idx) const noexcept {
    if (!has(idx)) return ~size_t(0);
    return groups[idx].start;
}

size_t Match::end(int idx) const noexcept {
    if (!has(idx)) return ~size_t(0);
    return groups[idx].end;
}

// ── Regex construction ────────────────────────────────────────────────────────
Regex::Regex(const std::string& pattern)
    : impl_(std::make_unique<Impl>())
{
    int        errcode  = 0;
    PCRE2_SIZE erroffset = 0;

    impl_->code = pcre2_compile(
        reinterpret_cast<PCRE2_SPTR>(pattern.c_str()),
        PCRE2_ZERO_TERMINATED,
        PCRE2_UTF,      // enable UTF-8 processing
        &errcode, &erroffset, nullptr);

    if (!impl_->code) {
        PCRE2_UCHAR buf[256];
        pcre2_get_error_message(errcode, buf, sizeof(buf));
        throw std::runtime_error(
            std::string("PCRE2 compile error at offset ")
            + std::to_string(erroffset) + ": "
            + reinterpret_cast<char*>(buf)
            + " in pattern: " + pattern);
    }

    pcre2_pattern_info(impl_->code, PCRE2_INFO_CAPTURECOUNT, &impl_->capture_count);
    impl_->build_name_table();
}

Regex::~Regex() = default;
Regex::Regex(Regex&&) noexcept = default;
Regex& Regex::operator=(Regex&&) noexcept = default;

// ── Match building ────────────────────────────────────────────────────────────
Match Regex::make_match(const std::string& subject,
                        const size_t* ovector, int rc) const
{
    Match m;
    m.subject     = subject;
    m.named_groups = impl_->named;

    int n_groups = static_cast<int>(impl_->capture_count) + 1;  // +1 for full match
    m.groups.resize(n_groups);

    for (int i = 0; i < rc && i < n_groups; ++i) {
        m.groups[i].start = ovector[2 * i];
        m.groups[i].end   = ovector[2 * i + 1];
    }
    // Groups beyond rc (didn't participate in the match) stay at sentinel.
    return m;
}

// ── is_match ──────────────────────────────────────────────────────────────────
bool Regex::is_match(const std::string& text) const {
    pcre2_match_data* md =
        pcre2_match_data_create_from_pattern(impl_->code, nullptr);
    int rc = pcre2_match(
        impl_->code,
        reinterpret_cast<PCRE2_SPTR>(text.c_str()), text.size(),
        0, 0, md, nullptr);
    pcre2_match_data_free(md);
    return rc >= 0;
}

// ── find ─────────────────────────────────────────────────────────────────────
std::optional<Match> Regex::find(const std::string& text, size_t offset) const {
    pcre2_match_data* md =
        pcre2_match_data_create_from_pattern(impl_->code, nullptr);
    int rc = pcre2_match(
        impl_->code,
        reinterpret_cast<PCRE2_SPTR>(text.c_str()), text.size(),
        offset, 0, md, nullptr);
    if (rc < 0) {
        pcre2_match_data_free(md);
        return std::nullopt;
    }
    const PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
    Match m = make_match(text, ov, rc);
    pcre2_match_data_free(md);
    return m;
}

// ── find_all ─────────────────────────────────────────────────────────────────
std::vector<Match> Regex::find_all(const std::string& text) const {
    std::vector<Match> results;
    pcre2_match_data* md =
        pcre2_match_data_create_from_pattern(impl_->code, nullptr);

    size_t offset = 0;
    const size_t len = text.size();
    while (offset <= len) {
        int rc = pcre2_match(
            impl_->code,
            reinterpret_cast<PCRE2_SPTR>(text.c_str()), len,
            offset, 0, md, nullptr);
        if (rc < 0) break;

        const PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        results.push_back(make_match(text, ov, rc));

        size_t new_offset = ov[1];
        if (new_offset == ov[0]) {
            // Zero-length match — advance by one UTF-8 code unit to avoid infinite loop.
            // Skip over a multi-byte UTF-8 sequence correctly.
            if (offset < len) {
                uint8_t b = static_cast<uint8_t>(text[offset]);
                if      ((b & 0x80) == 0x00) offset += 1;
                else if ((b & 0xE0) == 0xC0) offset += 2;
                else if ((b & 0xF0) == 0xE0) offset += 3;
                else                          offset += 4;
            } else {
                break;
            }
        } else {
            offset = new_offset;
        }
    }
    pcre2_match_data_free(md);
    return results;
}

// ── replace_all helpers ───────────────────────────────────────────────────────

// Expand $N or ${N} backreferences in a replacement string.
static std::string apply_repl_string(const std::string& repl, const Match& m) {
    std::string out;
    out.reserve(repl.size() + 32);
    for (size_t i = 0; i < repl.size(); ++i) {
        if (repl[i] == '$' && i + 1 < repl.size()) {
            char next = repl[i + 1];
            if (std::isdigit(static_cast<unsigned char>(next))) {
                int grp = next - '0';
                out += m.get(grp);
                ++i;
                continue;
            } else if (next == '{') {
                // ${NN}
                size_t j = i + 2;
                while (j < repl.size() && std::isdigit(static_cast<unsigned char>(repl[j]))) ++j;
                if (j < repl.size() && repl[j] == '}' && j > i + 2) {
                    int grp = std::stoi(repl.substr(i + 2, j - (i + 2)));
                    out += m.get(grp);
                    i = j;
                    continue;
                }
            }
        }
        out += repl[i];
    }
    return out;
}

// ── replace_all (string replacement) ─────────────────────────────────────────
std::string Regex::replace_all(const std::string& text, const std::string& repl) const {
    return replace_all(text, [&](const Match& m) {
        return apply_repl_string(repl, m);
    });
}

// ── replace_all (callback) ────────────────────────────────────────────────────
std::string Regex::replace_all(const std::string& text,
                                std::function<std::string(const Match&)> fn) const
{
    std::string result;
    result.reserve(text.size());

    pcre2_match_data* md =
        pcre2_match_data_create_from_pattern(impl_->code, nullptr);

    size_t offset  = 0;
    size_t prev    = 0;
    const size_t len = text.size();

    while (offset <= len) {
        int rc = pcre2_match(
            impl_->code,
            reinterpret_cast<PCRE2_SPTR>(text.c_str()), len,
            offset, 0, md, nullptr);
        if (rc < 0) break;

        const PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        size_t match_start = ov[0];
        size_t match_end   = ov[1];

        // Append unmatched text before this match
        result.append(text, prev, match_start - prev);

        // Apply replacement
        Match m = make_match(text, ov, rc);
        result += fn(m);

        prev   = match_end;
        size_t new_offset = match_end;
        if (new_offset == match_start) {
            if (offset < len) {
                // Append current char literally and skip
                uint8_t b = static_cast<uint8_t>(text[offset]);
                size_t  step;
                if      ((b & 0x80) == 0x00) step = 1;
                else if ((b & 0xE0) == 0xC0) step = 2;
                else if ((b & 0xF0) == 0xE0) step = 3;
                else                          step = 4;
                result.append(text, offset, step);
                prev   = offset + step;
                offset = prev;
            } else {
                break;
            }
        } else {
            offset = new_offset;
        }
    }
    result.append(text, prev, std::string::npos);
    pcre2_match_data_free(md);
    return result;
}

// ── regex_escape ─────────────────────────────────────────────────────────────
std::string regex_escape(const std::string& s) {
    static const char special[] = R"(\.^$*+?{}[]|()/-)";
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        if (std::strchr(special, c)) out += '\\';
        out += c;
    }
    return out;
}

// ── UTF-8 helpers ─────────────────────────────────────────────────────────────
uint32_t utf8_next(const char*& ptr, const char* end) noexcept {
    if (ptr >= end) return 0;
    auto b0 = static_cast<uint8_t>(*ptr++);
    if ((b0 & 0x80) == 0x00) return b0;
    if ((b0 & 0xE0) == 0xC0) {
        uint32_t cp = b0 & 0x1F;
        if (ptr < end) cp = (cp << 6) | (static_cast<uint8_t>(*ptr++) & 0x3F);
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0) {
        uint32_t cp = b0 & 0x0F;
        for (int i = 0; i < 2 && ptr < end; ++i)
            cp = (cp << 6) | (static_cast<uint8_t>(*ptr++) & 0x3F);
        return cp;
    }
    // 4-byte
    uint32_t cp = b0 & 0x07;
    for (int i = 0; i < 3 && ptr < end; ++i)
        cp = (cp << 6) | (static_cast<uint8_t>(*ptr++) & 0x3F);
    return cp;
}

std::vector<uint32_t> utf8_codepoints(const std::string& s) {
    std::vector<uint32_t> out;
    const char* ptr = s.data();
    const char* end = ptr + s.size();
    while (ptr < end) out.push_back(utf8_next(ptr, end));
    return out;
}

bool utf8_contains_cp(const std::string& s, uint32_t cp) {
    const char* ptr = s.data();
    const char* end = ptr + s.size();
    while (ptr < end) {
        if (utf8_next(ptr, end) == cp) return true;
    }
    return false;
}

}  // namespace sea_g2p
