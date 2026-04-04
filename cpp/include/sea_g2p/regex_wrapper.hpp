#pragma once
// regex_wrapper.hpp — thin C++ wrapper around PCRE2 (maps to Rust's regex + fancy_regex crates)
// All patterns use PCRE2_UTF so they handle UTF-8 correctly.
// Inline flags like (?i), (?s), (?x), (?m) in the pattern are supported natively by PCRE2.

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <string_view>

namespace sea_g2p {

// ── Match ─────────────────────────────────────────────────────────────────────
// Stores one match result with all capture groups.
// Group 0 = full match, group N = Nth capture group (1-based, same as Rust).
struct Match {
    struct Group {
        size_t start{~size_t(0)};  // PCRE2_UNSET sentinel
        size_t end{~size_t(0)};
        bool matched() const noexcept { return start != ~size_t(0); }
    };

    std::string_view   subject;  // view of the matched subject
    std::vector<Group> groups;   // index 0 = full match

    bool        has(int idx) const noexcept;
    std::string get(int idx) const;          // empty string if not matched
    std::string get(const std::string& name) const; // named group
    size_t      start(int idx = 0) const noexcept;
    size_t      end  (int idx = 0) const noexcept;

    // Internal: named group name→index map (populated by Regex)
    std::vector<std::pair<std::string, int>> named_groups;
};

// ── Regex ─────────────────────────────────────────────────────────────────────
// Wraps a compiled PCRE2 pattern. Thread-safe for matching (immutable after construction).
// Supports both simple and lookbehind/lookahead patterns (PCRE2 backtracker).
class Regex {
public:
    explicit Regex(const std::string& pattern);
    ~Regex();

    // Non-copyable, movable
    Regex(const Regex&)            = delete;
    Regex& operator=(const Regex&) = delete;
    Regex(Regex&&) noexcept;
    Regex& operator=(Regex&&) noexcept;

    bool is_match(const std::string& text) const;

    // First match only (optional)
    std::optional<Match> find(const std::string& text, size_t offset = 0) const;

    // All non-overlapping matches (equivalent to Rust's find_iter / captures_iter)
    std::vector<Match> find_all(const std::string& text) const;

    // replace_all with literal string ($1, $2, … backreferences supported)
    std::string replace_all(const std::string& text, const std::string& repl) const;

    // replace_all with callback function (receives each Match, returns replacement)
    std::string replace_all(const std::string& text,
                            std::function<std::string(const Match&)> fn) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    Match make_match(std::string_view subject, const size_t* ovector, int rc) const;
};

// ── Helpers ───────────────────────────────────────────────────────────────────

// Escape all PCRE2 special characters in a literal string so it can be embedded
// safely inside a larger pattern.
std::string regex_escape(const std::string& s);

// Iterate over UTF-8 codepoints.  Returns each codepoint; advances ptr.
// ptr must be valid UTF-8; end marks one-past-end.
uint32_t utf8_next(const char*& ptr, const char* end) noexcept;

// Collect all codepoints from a UTF-8 string into a vector.
std::vector<uint32_t> utf8_codepoints(const std::string& s);

// Check whether a UTF-8 string contains the given codepoint.
bool utf8_contains_cp(const std::string& s, uint32_t cp);

}  // namespace sea_g2p
