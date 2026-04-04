#pragma once
// num2vi.hpp — number-to-Vietnamese-words conversion (maps to src/vi_normalizer/num2vi.rs)

#include <string>

namespace sea_g2p {

// Single digit → Vietnamese word ("0"→"không", …, "9"→"chín")
const char* digit_to_vi(char digit);

// Convert a 1–3 digit string to Vietnamese words.
// e.g. "123"→"một trăm hai mươi ba", "005"→"không trăm lẻ năm"
std::string n2w_hundreds(const std::string& numbers);

// Convert an arbitrary non-negative integer string to Vietnamese words.
// Handles groups of thousands/millions/billions.
std::string n2w_large_number(const std::string& numbers);

// Main entry: convert an integer string (possibly with non-digit prefix stripped)
// to Vietnamese words.  Leading zeros trigger single-digit reading.
// e.g. "1234"→"một nghìn hai trăm ba mươi tư", "07"→"không bảy"
std::string n2w(const std::string& number);

// Digit-by-digit reading (phone numbers, zip codes, …).
// "+84xxx" prefix is normalized to "0xxx" first.
std::string n2w_single(const std::string& number);

// Read digits one by one for decimal part, applying Vietnamese phonetic rules
// (trailing '5' after non-zero → "lăm").
std::string n2w_decimal(const std::string& number);

}  // namespace sea_g2p
