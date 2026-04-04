#pragma once
// technical.hpp — URL/email/path normalization (maps to src/vi_normalizer/technical.rs)

#include <string>
#include "sea_g2p/regex_wrapper.hpp"

namespace sea_g2p {

// Regex that matches URLs, file paths, IPv6 addresses, domain names, etc.
const Regex& re_technical();

// Regex that matches email addresses.
const Regex& re_email();

// Convert a technical string (URL, path, …) to a spoken Vietnamese form.
std::string normalize_technical(const std::string& text);

// Convert an email address to a spoken Vietnamese form.
std::string normalize_emails(const std::string& text);

// Normalize slash-separated fraction-like patterns: "3/4" → "ba trên tư"
std::string normalize_slashes(const std::string& text);

}  // namespace sea_g2p
