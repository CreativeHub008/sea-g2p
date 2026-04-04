#pragma once
// units.hpp — unit/currency expansion (maps to src/vi_normalizer/units.rs)

#include <string>

namespace sea_g2p {

// Expand a number string that may contain E-notation, mixed separators, etc.
// into Vietnamese words.
std::string expand_number_with_sep(const std::string& num_str);

// Expand compound units like "km/h" or "30km/h" → Vietnamese
std::string expand_compound_units(const std::string& text);

// Expand unit/currency abbreviations that follow a number
std::string expand_units_and_currency(const std::string& text);

// Fix English-style numbers (comma thousands separator): 1,234,567 → remove commas
std::string fix_english_style_numbers(const std::string& text);

// Expand "10^3"-style power-of-ten expressions
std::string expand_power_of_ten(const std::string& text);

// Expand E-notation: "1.5e10" → "một chấm năm nhân mười mũ mười"
std::string expand_scientific_notation(const std::string& text);

}  // namespace sea_g2p
