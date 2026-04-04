#pragma once
// resources.hpp — static lookup tables (maps to src/vi_normalizer/resources.rs)

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sea_g2p {

// Getter functions (lazily initialized on first call, thread-safe in C++11+).

// Vietnamese letter names: "a"→"a", "b"→"bê", etc.
const std::unordered_map<std::string, std::string>& vi_letter_names();

// Measurement unit abbreviation → Vietnamese expansion
const std::unordered_map<std::string, std::string>& measurement_key_vi();

// Currency abbreviation / symbol → Vietnamese expansion
const std::unordered_map<std::string, std::string>& currency_key();

// Vietnamese acronym exceptions: "TP.HCM" → "thành phố hồ chí minh"
const std::unordered_map<std::string, std::string>& acronyms_exceptions_vi();

// Technical term exceptions: "JSON" → "__start_en__j son__end_en__"
const std::unordered_map<std::string, std::string>& technical_terms();

// Combined: acronyms_exceptions_vi + technical_terms
const std::unordered_map<std::string, std::string>& combined_exceptions();

// Domain suffix map: "com"→"com", "vn"→"__start_en__v n__end_en__", …
const std::unordered_map<std::string, std::string>& domain_suffix_map();

// Currency symbol map: "$"→"__start_en__u s d__end_en__", …
const std::unordered_map<std::string, std::string>& currency_symbol_map();

// Roman numeral char → value
const std::unordered_map<char, int>& roman_numerals();

// Simple abbreviation replacements: "v.v"→" vân vân", …
const std::unordered_map<std::string, std::string>& abbrs();

// Symbol → Vietnamese word(s)
const std::unordered_map<uint32_t, std::string>& symbols_map();

// Superscript digit → Vietnamese word
const std::unordered_map<uint32_t, std::string>& superscripts_map();

// Subscript digit → Vietnamese word
const std::unordered_map<uint32_t, std::string>& subscripts_map();

// Acronyms that should be read as words (not spelled out)
const std::unordered_set<std::string>& word_like_acronyms();

// Common email domain → Vietnamese reading
const std::unordered_map<std::string, std::string>& common_email_domains();

// Context keyword sets used for date / math disambiguation
const std::unordered_set<std::string>& date_keywords();
const std::unordered_set<std::string>& math_keywords();

}  // namespace sea_g2p
