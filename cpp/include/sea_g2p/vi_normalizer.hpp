#pragma once
// vi_normalizer.hpp — Vietnamese text normalizer public API
//                     (maps to src/vi_normalizer/mod.rs + Normalizer struct)

#include <memory>
#include <string>
#include <vector>

namespace sea_g2p {

class Regex;

// Full Vietnamese text normalization pipeline:
// 1. Protect emails, URLs, technical strings
// 2. Split CamelCase
// 3. Expand numbers, dates, times, units, acronyms, symbols
// 4. Restore protected strings
// Returns lowercase normalized text with <en>…</en> markup for English words.
std::string clean_vietnamese_text(const std::string& text);

// High-level normalizer class (mirrors Rust's Normalizer).
// Currently supports lang="vi" only.
class Normalizer {
public:
    explicit Normalizer(std::string lang = "vi", std::string ignore = "");
    ~Normalizer();

    const std::string& lang() const noexcept { return lang_; }
    const std::string& ignore() const noexcept { return ignore_; }

    // Normalize a single text string.
    std::string normalize(const std::string& text) const;

    // Normalize a batch of strings.
    // Uses std::thread internally if compiled with thread support.
    std::vector<std::string> normalize_batch(const std::vector<std::string>& texts) const;

private:
    std::string lang_;
    std::string ignore_;
    std::unique_ptr<Regex> ignore_re_;
};

}  // namespace sea_g2p
