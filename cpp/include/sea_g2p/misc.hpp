#pragma once
// misc.hpp — acronym/symbol/letter expansion (maps to src/vi_normalizer/misc.rs)

#include <string>
#include "sea_g2p/regex_wrapper.hpp"

namespace sea_g2p {

// Regex matching known exception acronyms + technical terms
const Regex& re_acronyms_exceptions();

// Regex matching general acronym patterns (2+ consecutive capitals with optional Vi letters)
const Regex& re_acronym();

// Regex matching domain suffixes
const Regex& domain_suffixes_re();

// Expand Roman numerals to Arabic numbers: "XIV" → "mười bốn"
std::string expand_roman(const std::string& match_str);

// Expand unit power notation: "km^2" → "ki lô mét mũ hai"
std::string expand_unit_powers(const std::string& text);

// Expand "chữ X" → letter name
std::string expand_letter(const std::string& text);

// Replace abbreviations (v.v → vân vân, etc.)
std::string expand_abbreviations(const std::string& text);

// Expand standalone single letters: "A" → "a"
std::string expand_standalone_letters(const std::string& text);

// Expand sequences of capital letters (acronyms): "VN" → "__start_en__v n__end_en__"
std::string normalize_acronyms(const std::string& text);

// Expand "5G" → "năm gờ"
std::string expand_alphanumeric(const std::string& text);

// Expand math symbols and Greek letters to Vietnamese words
std::string expand_symbols(const std::string& text);

// Expand prime notation: "A'" → "a phẩy"
std::string expand_prime(const std::string& text);

// Expand temperature: "36°C" → "ba mươi sáu độ xê"
std::string expand_temperatures(const std::string& text);

// Main normalization function combining all the above for "other" patterns
std::string normalize_others(const std::string& text);

}  // namespace sea_g2p
