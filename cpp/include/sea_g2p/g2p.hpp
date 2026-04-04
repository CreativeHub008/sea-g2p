#pragma once
// g2p.hpp — G2P engine public API (maps to src/g2p/mod.rs)

#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sea_g2p {

// ── PhonemeDict ───────────────────────────────────────────────────────────────
// Memory-mapped reader for the binary SEAP phoneme dictionary.
//
// Binary layout (little-endian):
//   [0..3]   magic "SEAP"
//   [4..7]   reserved
//   [8..11]  string_count  (u32)
//   [12..15] merged_count  (u32)
//   [16..19] common_count  (u32)
//   [20..23] string_offsets_pos (u32)  — byte offset of string index table
//   [24..27] merged_pos    (u32)  — byte offset of merged table
//   [28..31] common_pos    (u32)  — byte offset of common table
//
//   [32+]    string pool   — null-terminated UTF-8 strings
//   [string_offsets_pos+] — u32[string_count]: offsets into string pool (from byte 32)
//   [merged_pos+]  — (word_id u32, phoneme_id u32) × merged_count, sorted by word
//   [common_pos+]  — (word_id u32, vi_id u32, en_id u32) × common_count, sorted by word
class PhonemeDict {
public:
    explicit PhonemeDict(const std::string& path);
    ~PhonemeDict();

    PhonemeDict(const PhonemeDict&)            = delete;
    PhonemeDict& operator=(const PhonemeDict&) = delete;
    PhonemeDict(PhonemeDict&&) noexcept;
    PhonemeDict& operator=(PhonemeDict&&) noexcept;

    // Binary search on the merged table. Returns phoneme string or empty.
    std::string lookup_merged(const std::string& word) const;

    // Binary search on the common table. Returns (vi_phoneme, en_phoneme) or nullopt.
    std::optional<std::pair<std::string, std::string>> lookup_common(
        const std::string& word) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    const char* get_string(uint32_t id) const noexcept;
};

// ── Token ─────────────────────────────────────────────────────────────────────
struct Token {
    std::string lang;          // "vi", "en", "common", "punct"
    std::string content;       // original text
    std::string phone;         // resolved phoneme (empty = not resolved yet)
    // For "common" tokens the phone field contains "\x1F{vi}\x1F{en}\x1F"
    bool        is_explicit_en{false};  // inside an <en>…</en> tag
    bool        phone_set{false};       // whether 'phone' has been assigned
};

// ── G2PEngine ─────────────────────────────────────────────────────────────────
// Thread-safe G2P engine.  Multiple threads may call phonemize() concurrently.
class G2PEngine {
public:
    explicit G2PEngine(const std::string& dict_path);
    ~G2PEngine();

    G2PEngine(const G2PEngine&)            = delete;
    G2PEngine& operator=(const G2PEngine&) = delete;

    // Convert normalized text to IPA-like phoneme string.
    std::string phonemize(const std::string& text) const;

    // Parallel batch phonemization using std::async.
    std::vector<std::string> phonemize_batch(const std::vector<std::string>& texts) const;

private:
    PhonemeDict dict_;

    // Thread-safe caches  (mutable so phonemize() can be const)
    mutable std::shared_mutex merged_mutex_;
    mutable std::unordered_map<std::string, std::string> merged_cache_;

    mutable std::shared_mutex common_mutex_;
    mutable std::unordered_map<std::string,
                               std::pair<std::string, std::string>> common_cache_;

    mutable std::shared_mutex miss_merged_mutex_;
    mutable std::unordered_set<std::string> missing_merged_;

    mutable std::shared_mutex miss_common_mutex_;
    mutable std::unordered_set<std::string> missing_common_;

    mutable std::shared_mutex seg_mutex_;
    mutable std::unordered_map<std::string, std::string> segmentation_cache_;
    // empty string in segmentation_cache_ means "no segmentation found"
    // (use seg_tried_ to distinguish "not tried" from "tried and failed")
    mutable std::unordered_set<std::string> seg_tried_;

    // ── Cache helpers ──────────────────────────────────────────────────────
    std::string cached_lookup_merged(const std::string& word) const;
    std::optional<std::pair<std::string, std::string>>
        cached_lookup_common(const std::string& word) const;

    // Resolve phoneme for a single word given its language context.
    std::string resolve_segment_phone(const std::string& segment,
                                      const std::string& lang) const;

    // DP-based OOV segmentation.  Returns empty if unsuccessful.
    std::string segment_oov(const std::string& word,
                            const std::string& lang) const;

    // Char-by-char fallback.
    std::string char_fallback(const std::string& content,
                              const std::string& lang) const;

    // Language propagation for "common" tokens.
    void propagate_language(std::vector<Token>& tokens) const;
};

}  // namespace sea_g2p
