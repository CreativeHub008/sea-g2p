// num2vi.cpp — number-to-Vietnamese-words conversion (maps to src/vi_normalizer/num2vi.rs)
#include "sea_g2p/num2vi.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace sea_g2p {

const char* digit_to_vi(char digit) {
    switch (digit) {
        case '0': return "không";
        case '1': return "một";
        case '2': return "hai";
        case '3': return "ba";
        case '4': return "bốn";
        case '5': return "năm";
        case '6': return "sáu";
        case '7': return "bảy";
        case '8': return "tám";
        case '9': return "chín";
        default:  return "";
    }
}

std::string n2w_hundreds(const std::string& numbers) {
    if (numbers.empty() || numbers == "000") return {};

    // Zero-pad to exactly 3 chars
    std::string n = numbers;
    while (n.size() < 3) n = "0" + n;
    // Take only last 3 chars if longer (shouldn't happen)
    if (n.size() > 3) n = n.substr(n.size() - 3);

    char h = n[0], t = n[1], u = n[2];

    std::vector<std::string> res;

    // Hundreds
    if (h != '0') {
        res.push_back(std::string(digit_to_vi(h)) + " trăm");
    } else if (numbers.size() == 3) {
        res.push_back("không trăm");
    }

    // Tens
    if (t == '0') {
        if (u != '0' && (h != '0' || numbers.size() == 3)) {
            res.push_back("lẻ");
        }
    } else if (t == '1') {
        res.push_back("mười");
    } else {
        res.push_back(std::string(digit_to_vi(t)) + " mươi");
    }

    // Units
    if (u != '0') {
        if (u == '1' && t != '0' && t != '1') {
            res.push_back("mốt");
        } else if (u == '5' && t != '0') {
            res.push_back("lăm");
        } else if (u == '4' && t != '0' && t != '1') {
            res.push_back("tư");
        } else {
            res.push_back(digit_to_vi(u));
        }
    }

    std::string out;
    for (size_t i = 0; i < res.size(); ++i) {
        if (i) out += ' ';
        out += res[i];
    }
    return out;
}

std::string n2w_large_number(const std::string& raw) {
    // Strip leading zeros
    size_t start = 0;
    while (start < raw.size() - 1 && raw[start] == '0') ++start;
    std::string numbers = raw.substr(start);
    if (numbers.empty()) return digit_to_vi('0');

    // Split into groups of 3 from the right
    std::vector<std::string> groups;
    int len = static_cast<int>(numbers.size());
    for (int i = len; i > 0; i -= 3) {
        int s = std::max(0, i - 3);
        groups.push_back(numbers.substr(s, i - s));
    }
    // groups[0] = least significant, groups.back() = most significant

    static const char* suffixes[] = {"", " nghìn", " triệu", " tỷ"};

    std::vector<std::string> parts;
    for (int i = 0; i < static_cast<int>(groups.size()); ++i) {
        const auto& grp = groups[i];
        if (grp == "000") continue;

        std::string word = n2w_hundreds(grp);
        if (word.empty()) continue;

        int suffix_idx = i % 4;
        const char* suf = suffix_idx < 4 ? suffixes[suffix_idx] : "";
        int ty_count = i / 4;

        word += suf;
        for (int t = 0; t < ty_count; ++t) word += " tỷ";
        parts.push_back(word);
    }

    if (parts.empty()) return digit_to_vi('0');

    std::reverse(parts.begin(), parts.end());
    std::string result;
    for (const auto& p : parts) {
        if (!result.empty()) result += ' ';
        result += p;
    }
    // Trim
    while (!result.empty() && result.front() == ' ') result.erase(result.begin());
    while (!result.empty() && result.back()  == ' ') result.pop_back();
    return result;
}

std::string n2w(const std::string& number) {
    // Keep only ASCII digits
    std::string clean;
    for (char c : number) {
        if (std::isdigit(static_cast<unsigned char>(c))) clean += c;
    }
    if (clean.empty()) return number;

    // "07" style: two digits with leading zero → "không bảy"
    if (clean.size() == 2 && clean[0] == '0') {
        return std::string("không ") + digit_to_vi(clean[1]);
    }

    return n2w_large_number(clean);
}

std::string n2w_single(const std::string& number) {
    std::string num_str = number;
    // Normalize +84 prefix
    if (num_str.size() > 3 && num_str.substr(0, 3) == "+84") {
        num_str = "0" + num_str.substr(3);
    }

    std::vector<std::string> parts;
    for (char c : num_str) {
        if (std::isdigit(static_cast<unsigned char>(c)))
            parts.push_back(digit_to_vi(c));
    }
    if (parts.empty()) return number;

    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += ' ';
        out += parts[i];
    }
    return out;
}

std::string n2w_decimal(const std::string& number) {
    std::string clean;
    for (char c : number) {
        if (std::isdigit(static_cast<unsigned char>(c))) clean += c;
    }
    if (clean.empty()) return number;

    std::vector<std::string> res;
    for (size_t i = 0; i < clean.size(); ++i) {
        char d = clean[i];
        if (d == '5' && i == clean.size() - 1 && i > 0 && clean[i-1] != '0') {
            res.push_back("lăm");
        } else {
            const char* w = digit_to_vi(d);
            if (*w) res.push_back(w);
        }
    }

    std::string out;
    for (size_t i = 0; i < res.size(); ++i) {
        if (i) out += ' ';
        out += res[i];
    }
    return out;
}

}  // namespace sea_g2p
