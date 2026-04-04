#pragma once
// datestime.hpp — date/time normalization (maps to src/vi_normalizer/datestime.rs)

#include <string>

namespace sea_g2p {

// Normalize date patterns to Vietnamese words.
// e.g. "25/12/2024" → "ngày hai mươi lăm tháng mười hai năm hai nghìn không trăm hai mươi tư"
std::string normalize_date(const std::string& text);

// Normalize time patterns to Vietnamese words.
// e.g. "10:30" → "mười giờ ba mươi phút"
std::string normalize_time(const std::string& text);

}  // namespace sea_g2p
