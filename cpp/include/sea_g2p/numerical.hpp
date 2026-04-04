#pragma once
// numerical.hpp — numeric text normalization (maps to src/vi_normalizer/numerical.rs)

#include <string>
#include "sea_g2p/regex_wrapper.hpp"

namespace sea_g2p {

// The multiplication regex is exported for use in vi_normalizer.cpp
const Regex& re_multiply();

// Convert a number string with optional decimal separator to Vietnamese words.
// negative=true prepends "âm ".
std::string num_to_words(const std::string& number, bool negative);

// Expand the "x" / "×" multiplication separator to " nhân " in a matched string.
std::string expand_multiply_number(const std::string& text);

// Full numeric normalization pass for Vietnamese text:
// handles ordinals, phone numbers, general numbers.
std::string normalize_number_vi(const std::string& text);

}  // namespace sea_g2p
