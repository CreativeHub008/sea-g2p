// g2p.cpp — G2P engine implementation (maps to src/g2p/mod.rs)
#include "sea_g2p/g2p.hpp"
#include "sea_g2p/regex_wrapper.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <functional>
#include <future>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <cstdlib> // std::getenv
#include <iostream>

namespace sea_g2p {

// ── Platform-specific memory-mapped file ─────────────────────────────────────

struct PhonemeDict::Impl {
    const uint8_t* data{nullptr};
    size_t         size{0};
    uint32_t       string_count{0};
    uint32_t       merged_count{0};
    uint32_t       common_count{0};
    size_t         string_offsets_pos{0};
    size_t         merged_pos{0};
    size_t         common_pos{0};

#ifdef _WIN32
    HANDLE hFile{INVALID_HANDLE_VALUE};
    HANDLE hMap{nullptr};
#else
    int    fd{-1};
#endif
};

static uint32_t read_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

PhonemeDict::PhonemeDict(const std::string& path)
    : impl_(std::make_unique<Impl>())
{
#ifdef _WIN32
    impl_->hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (impl_->hFile == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open dictionary: " + path);

    LARGE_INTEGER sz;
    GetFileSizeEx(impl_->hFile, &sz);
    impl_->size = static_cast<size_t>(sz.QuadPart);

    impl_->hMap = CreateFileMappingA(impl_->hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!impl_->hMap) {
        CloseHandle(impl_->hFile);
        throw std::runtime_error("Cannot map dictionary: " + path);
    }
    impl_->data = static_cast<const uint8_t*>(
        MapViewOfFile(impl_->hMap, FILE_MAP_READ, 0, 0, 0));
    if (!impl_->data) {
        CloseHandle(impl_->hMap);
        CloseHandle(impl_->hFile);
        throw std::runtime_error("Cannot view dictionary map: " + path);
    }
#else
    impl_->fd = ::open(path.c_str(), O_RDONLY);
    if (impl_->fd < 0)
        throw std::runtime_error("Cannot open dictionary: " + path);
    struct stat st;
    if (::fstat(impl_->fd, &st) != 0) {
        ::close(impl_->fd);
        throw std::runtime_error("Cannot stat dictionary: " + path);
    }
    impl_->size = static_cast<size_t>(st.st_size);
    impl_->data = static_cast<const uint8_t*>(
        ::mmap(nullptr, impl_->size, PROT_READ, MAP_PRIVATE, impl_->fd, 0));
    if (impl_->data == MAP_FAILED) {
        ::close(impl_->fd);
        throw std::runtime_error("Cannot mmap dictionary: " + path);
    }
#endif

    if (impl_->size < 32 || std::memcmp(impl_->data, "SEAP", 4) != 0) {
        // Unmap and throw
        this->~PhonemeDict();
        impl_ = std::make_unique<Impl>();  // reset to avoid double-free in dtor
        throw std::runtime_error("Invalid dictionary format (expected SEAP header)");
    }

    impl_->string_count        = read_le32(impl_->data + 8);
    impl_->merged_count        = read_le32(impl_->data + 12);
    impl_->common_count        = read_le32(impl_->data + 16);
    impl_->string_offsets_pos  = read_le32(impl_->data + 20);
    impl_->merged_pos          = read_le32(impl_->data + 24);
    impl_->common_pos          = read_le32(impl_->data + 28);
}

PhonemeDict::~PhonemeDict() {
    if (!impl_) return;
#ifdef _WIN32
    if (impl_->data)  UnmapViewOfFile(impl_->data);
    if (impl_->hMap)  CloseHandle(impl_->hMap);
    if (impl_->hFile != INVALID_HANDLE_VALUE) CloseHandle(impl_->hFile);
#else
    if (impl_->data && impl_->data != MAP_FAILED)
        ::munmap(const_cast<uint8_t*>(impl_->data), impl_->size);
    if (impl_->fd >= 0) ::close(impl_->fd);
#endif
}

PhonemeDict::PhonemeDict(PhonemeDict&&) noexcept = default;
PhonemeDict& PhonemeDict::operator=(PhonemeDict&&) noexcept = default;

const char* PhonemeDict::get_string(uint32_t id) const noexcept {
    if (id >= impl_->string_count) return "";
    size_t off_ptr = impl_->string_offsets_pos + static_cast<size_t>(id) * 4;
    uint32_t offset = read_le32(impl_->data + off_ptr);
    return reinterpret_cast<const char*>(impl_->data + 32 + offset);
}

std::string PhonemeDict::lookup_merged(const std::string& word) const {
    int32_t low = 0, high = static_cast<int32_t>(impl_->merged_count) - 1;
    while (low <= high) {
        int32_t mid = (low + high) / 2;
        size_t ptr  = impl_->merged_pos + static_cast<size_t>(mid) * 8;
        uint32_t w_id = read_le32(impl_->data + ptr);
        const char* current_word = get_string(w_id);
        int cmp = word.compare(current_word);
        if (cmp == 0) {
            uint32_t p_id = read_le32(impl_->data + ptr + 4);
            return get_string(p_id);
        } else if (cmp > 0) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return {};
}

std::optional<std::pair<std::string, std::string>>
PhonemeDict::lookup_common(const std::string& word) const {
    int32_t low = 0, high = static_cast<int32_t>(impl_->common_count) - 1;
    while (low <= high) {
        int32_t mid = (low + high) / 2;
        size_t ptr  = impl_->common_pos + static_cast<size_t>(mid) * 12;
        uint32_t w_id = read_le32(impl_->data + ptr);
        const char* current_word = get_string(w_id);
        int cmp = word.compare(current_word);
        if (cmp == 0) {
            uint32_t vi_id = read_le32(impl_->data + ptr + 4);
            uint32_t en_id = read_le32(impl_->data + ptr + 8);
            return std::make_pair(std::string(get_string(vi_id)),
                                  std::string(get_string(en_id)));
        } else if (cmp > 0) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return std::nullopt;
}

// ── Regex patterns ────────────────────────────────────────────────────────────

// Vietnamese diacritic characters (for language detection)
static const std::string VI_ACCENTS =
    "àáảãạăằắẳẵặâầấẩẫậèéẻẽẹêềếểễệìíỉĩịòóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵđ";

// Vowel set: English + Vietnamese
static const std::string VOWELS =
    "aeiouyàáảãạăằắẳẵặâầấẩẫậèéẻẽẹêềếểễệìíỉĩịòóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵ";

// Pre-compiled sets of codepoints for fast lookup
static const std::vector<uint32_t>& vi_accent_cps() {
    static const std::vector<uint32_t> cps = utf8_codepoints(VI_ACCENTS);
    return cps;
}
static const std::vector<uint32_t>& vowel_cps() {
    static const std::vector<uint32_t> cps = utf8_codepoints(VOWELS);
    return cps;
}

static bool is_vi_accent_cp(uint32_t cp) {
    for (uint32_t v : vi_accent_cps()) if (v == cp) return true;
    return false;
}
static bool is_vowel_cp(uint32_t cp) {
    for (uint32_t v : vowel_cps()) if (v == cp) return true;
    return false;
}

static const Regex& re_token() {
    // Group 1: <en>…</en>, Group 2: word, Group 3: punct
    static const Regex r(R"((?i)(<en>.*?</en>)|(\w+(?:['']\w+)*)|([^\w\s]))");
    return r;
}
static const Regex& re_tag_content() {
    // Group 1: word, Group 2: punct
    static const Regex r(R"((\w+(?:['']\w+)*)|([^\w\s]))");
    return r;
}
static const Regex& re_tag_strip() {
    static const Regex r(R"((?i)</?en>)");
    return r;
}

// ── has_vowel_and_consonant ───────────────────────────────────────────────────

static bool has_vowel_and_consonant(const std::string& s) {
    bool has_v = false, has_c = false;
    const char* ptr = s.data();
    const char* end = ptr + s.size();
    while (ptr < end) {
        uint32_t cp = utf8_next(ptr, end);
        // Lowercase ASCII
        uint32_t lo = cp;
        if (cp >= 'A' && cp <= 'Z') lo = cp + 32;

        if (is_vowel_cp(lo)) {
            has_v = true;
        } else {
            // Is it alphabetic?
            bool alpha = (lo >= 'a' && lo <= 'z') || lo > 127;
            if (alpha) has_c = true;
        }
        if (has_v && has_c) return true;
    }
    return false;
}

// ── String utilities ──────────────────────────────────────────────────────────

static std::string to_lower_ascii(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    const char* ptr = s.data();
    const char* end = ptr + s.size();
    while (ptr < end) {
        const char* prev = ptr;
        uint32_t cp = utf8_next(ptr, end);
        if (cp >= 'A' && cp <= 'Z')
            r += static_cast<char>(cp + 32);
        else
            r.append(prev, ptr - prev);
    }
    return r;
}

static std::string trim_str(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && s[a] == ' ') ++a;
    size_t b = s.size();
    while (b > a && s[b-1] == ' ') --b;
    return s.substr(a, b - a);
}

static std::string replace_all_str(std::string s,
                                    const std::string& from, const std::string& to) {
    std::string::size_type pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

static std::string get_executable_dir() {
    char path[1024];
#ifdef _WIN32
    DWORD size = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (size == 0 || size == sizeof(path)) return "";
    std::string s(path, size);
    size_t last_slash = s.find_last_of("\\/");
    if (last_slash != std::string::npos) return s.substr(0, last_slash);
#else
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        std::string s(path);
        size_t last_slash = s.find_last_of("/");
        if (last_slash != std::string::npos) return s.substr(0, last_slash);
    }
#endif
    return "";
}

static bool file_exists_reg(const std::string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
#endif
}

static std::string find_dictionary(const std::string& provided_path) {
    if (!provided_path.empty() && file_exists_reg(provided_path)) return provided_path;

    // 1. Env
    const char* env_path = std::getenv("SEA_G2P_DICT_PATH");
    if (env_path && file_exists_reg(env_path)) return env_path;

    // 2. Executable dir
    std::string exe_dir = get_executable_dir();
    if (!exe_dir.empty()) {
        std::string p = exe_dir + "/sea_g2p.bin";
        if (file_exists_reg(p)) return p;
#ifdef _WIN32
        p = exe_dir + "\\sea_g2p.bin";
        if (file_exists_reg(p)) return p;
#endif
    }

    // 3. CWD
    if (file_exists_reg("sea_g2p.bin")) return "sea_g2p.bin";
    
    // 4. Fallback search /python/sea_g2p/sea_g2p.bin relative to exe for dev
    if (!exe_dir.empty()) {
        std::string p = exe_dir + "/../../python/sea_g2p/sea_g2p.bin";
        if (file_exists_reg(p)) return p;
    }

    return provided_path.empty() ? "sea_g2p.bin" : provided_path;
}

// ── G2PEngine ─────────────────────────────────────────────────────────────────

G2PEngine::G2PEngine() : G2PEngine("") {}

G2PEngine::G2PEngine(const std::string& dict_path)
    : dict_(find_dictionary(dict_path))
{
    merged_cache_.reserve(2048);
    common_cache_.reserve(1024);
    segmentation_cache_.reserve(512);
}

G2PEngine::~G2PEngine() = default;

// ── Cache helpers ─────────────────────────────────────────────────────────────

std::string G2PEngine::cached_lookup_merged(const std::string& word) const {
    // Fast read path
    {
        std::shared_lock lk(merged_mutex_);
        auto it = merged_cache_.find(word);
        if (it != merged_cache_.end()) return it->second;
    }
    {
        std::shared_lock lk(miss_merged_mutex_);
        if (missing_merged_.count(word)) return {};
    }
    std::string val = dict_.lookup_merged(word);
    if (!val.empty()) {
        std::unique_lock lk(merged_mutex_);
        if (merged_cache_.size() >= 10000) merged_cache_.clear();
        merged_cache_[word] = val;
        return val;
    } else {
        std::unique_lock lk(miss_merged_mutex_);
        if (missing_merged_.size() < 50000) missing_merged_.insert(word);
        return {};
    }
}

std::optional<std::pair<std::string, std::string>>
G2PEngine::cached_lookup_common(const std::string& word) const {
    {
        std::shared_lock lk(common_mutex_);
        auto it = common_cache_.find(word);
        if (it != common_cache_.end()) return it->second;
    }
    {
        std::shared_lock lk(miss_common_mutex_);
        if (missing_common_.count(word)) return std::nullopt;
    }
    auto val = dict_.lookup_common(word);
    if (val) {
        std::unique_lock lk(common_mutex_);
        if (common_cache_.size() >= 5000) common_cache_.clear();
        common_cache_[word] = *val;
        return val;
    } else {
        std::unique_lock lk(miss_common_mutex_);
        if (missing_common_.size() < 50000) missing_common_.insert(word);
        return std::nullopt;
    }
}

// ── resolve_segment_phone ─────────────────────────────────────────────────────

std::string G2PEngine::resolve_segment_phone(const std::string& segment,
                                              const std::string& lang) const
{
    std::string lw = to_lower_ascii(segment);

    std::string merged = cached_lookup_merged(lw);
    if (!merged.empty()) {
        return trim_str(replace_all_str(merged, "<en>", ""));
    }

    auto common = cached_lookup_common(lw);
    if (common) {
        std::string phone;
        if (lang == "en" && !common->second.empty()) {
            phone = trim_str(replace_all_str(common->second, "<en>", ""));
        } else if (!common->first.empty()) {
            phone = trim_str(common->first);
        } else {
            phone = trim_str(replace_all_str(common->second, "<en>", ""));
        }
        return phone;
    }

    return {};
}

// ── segment_oov ───────────────────────────────────────────────────────────────

std::string G2PEngine::segment_oov(const std::string& word,
                                    const std::string& lang) const
{
    std::string cache_key = word + "_" + lang;
    {
        std::shared_lock lk(seg_mutex_);
        auto it = seg_tried_.find(cache_key);
        if (it != seg_tried_.end()) {
            auto it2 = segmentation_cache_.find(cache_key);
            return it2 != segmentation_cache_.end() ? it2->second : std::string{};
        }
    }

    // Build codepoint list
    std::vector<uint32_t> cps = utf8_codepoints(word);
    size_t n = cps.size();

    // Also need byte boundaries for each codepoint to extract substrings
    std::vector<size_t> byte_pos;
    {
        const char* ptr = word.data();
        const char* end = ptr + word.size();
        byte_pos.push_back(0);
        while (ptr < end) {
            uint8_t b = static_cast<uint8_t>(*ptr);
            size_t len;
            if      ((b & 0x80) == 0x00) len = 1;
            else if ((b & 0xE0) == 0xC0) len = 2;
            else if ((b & 0xF0) == 0xE0) len = 3;
            else                          len = 4;
            ptr += len;
            byte_pos.push_back(ptr - word.data());
        }
    }

    // dp[i] = phoneme string for word[0..i] (empty string when not reachable,
    // use dp_set to know if it was set)
    std::vector<std::string> dp(n + 1);
    std::vector<bool> dp_set(n + 1, false);
    dp[0] = {};
    dp_set[0] = true;

    for (size_t i = 0; i < n; ++i) {
        if (!dp_set[i]) continue;
        for (size_t j = n; j > i; --j) {
            std::string seg = word.substr(byte_pos[i], byte_pos[j] - byte_pos[i]);
            if (!has_vowel_and_consonant(seg)) continue;
            std::string phone = resolve_segment_phone(seg, lang);
            if (phone.empty()) continue;
            if (!dp_set[j]) {
                std::string new_phone = dp[i].empty() ? phone : dp[i] + " " + phone;
                dp[j] = new_phone;
                dp_set[j] = true;
            }
            // Don't overwrite once set (longer segment already found)
        }
    }

    std::string result = dp_set[n] ? dp[n] : std::string{};

    {
        std::unique_lock lk(seg_mutex_);
        if (segmentation_cache_.size() >= 5000) {
            segmentation_cache_.clear();
            seg_tried_.clear();
        }
        seg_tried_.insert(cache_key);
        if (!result.empty()) segmentation_cache_[cache_key] = result;
    }

    return result;
}

// ── char_fallback ─────────────────────────────────────────────────────────────

std::string G2PEngine::char_fallback(const std::string& content,
                                      const std::string& lang) const
{
    std::string result;
    const char* ptr = content.data();
    const char* end = ptr + content.size();
    while (ptr < end) {
        const char* prev = ptr;
        uint32_t cp = utf8_next(ptr, end);
        // Build lowercase UTF-8 of this codepoint
        std::string cl;
        if (cp >= 'A' && cp <= 'Z') cl = std::string(1, (char)(cp + 32));
        else cl.append(prev, ptr - prev);

        std::string p = cached_lookup_merged(cl);
        if (!p.empty()) {
            result += trim_str(replace_all_str(p, "<en>", ""));
        } else {
            auto common = cached_lookup_common(cl);
            if (common) {
                if (lang == "en" && !common->second.empty())
                    result += trim_str(replace_all_str(common->second, "<en>", ""));
                else if (!common->first.empty())
                    result += trim_str(common->first);
                else
                    result += trim_str(replace_all_str(common->second, "<en>", ""));
            } else {
                result += cl;
            }
        }
    }
    return result;
}

// ── propagate_language ────────────────────────────────────────────────────────

void G2PEngine::propagate_language(std::vector<Token>& tokens) const {
    size_t n = tokens.size();
    size_t i = 0;

    auto is_stop_punct = [](const Token& t) {
        if (t.content.empty()) return false;
        // Single-codepoint check
        const char* ptr = t.content.data();
        const char* end = ptr + t.content.size();
        uint32_t cp = utf8_next(ptr, end);
        if (ptr != end) return false;  // multi-codepoint
        static const std::string stops = ".!?;:()[]{}";
        return stops.find(static_cast<char>(cp)) != std::string::npos;
    };

    while (i < n) {
        if (tokens[i].lang == "common") {
            size_t start = i;
            while (i < n && tokens[i].lang == "common") ++i;
            size_t end_  = i - 1;

            std::string left_anchor;
            size_t left_dist = 999;
            for (size_t l = start; l-- > 0;) {
                if (is_stop_punct(tokens[l])) break;
                if (tokens[l].lang == "vi" || tokens[l].lang == "en") {
                    left_anchor = tokens[l].lang;
                    left_dist   = start - l;
                    break;
                }
            }

            std::string right_anchor;
            size_t right_dist = 999;
            for (size_t r = end_ + 1; r < n; ++r) {
                if (is_stop_punct(tokens[r])) break;
                if (tokens[r].lang == "vi" || tokens[r].lang == "en") {
                    right_anchor = tokens[r].lang;
                    right_dist   = r - end_;
                    break;
                }
            }

            std::string final_lang;
            if (!left_anchor.empty() && !right_anchor.empty()) {
                final_lang = (right_dist <= left_dist) ? right_anchor : left_anchor;
            } else if (!left_anchor.empty()) {
                final_lang = left_anchor;
            } else if (!right_anchor.empty()) {
                final_lang = right_anchor;
            } else {
                final_lang = "vi";
            }

            for (size_t idx = start; idx <= end_; ++idx)
                tokens[idx].lang = final_lang;
        } else {
            ++i;
        }
    }
}

// ── phonemize ─────────────────────────────────────────────────────────────────

std::string G2PEngine::phonemize(const std::string& text) const {
    std::vector<Token> tokens;

    for (const Match& cap : re_token().find_all(text)) {
        if (cap.has(1)) {
            // <en>…</en> block
            std::string en_content = re_tag_strip().replace_all(cap.get(1), "");
            // trim
            while (!en_content.empty() && en_content.front() == ' ')
                en_content.erase(en_content.begin());
            while (!en_content.empty() && en_content.back()  == ' ')
                en_content.pop_back();

            for (const Match& scap : re_tag_content().find_all(en_content)) {
                if (scap.has(1)) {
                    const std::string& word = scap.get(1);
                    std::string lw = to_lower_ascii(word);
                    std::string phone_val;

                    std::string merged = cached_lookup_merged(lw);
                    if (!merged.empty()) {
                        phone_val = trim_str(replace_all_str(merged, "<en>", ""));
                    } else {
                        auto common = cached_lookup_common(lw);
                        if (common && !common->second.empty())
                            phone_val = trim_str(replace_all_str(common->second, "<en>", ""));
                    }

                    Token tok;
                    tok.lang           = "en";
                    tok.content        = word;
                    tok.phone          = phone_val;
                    tok.phone_set      = !phone_val.empty();
                    tok.is_explicit_en = true;
                    tokens.push_back(std::move(tok));

                } else if (scap.has(2)) {
                    Token tok;
                    tok.lang           = "punct";
                    tok.content        = scap.get(2);
                    tok.phone          = scap.get(2);
                    tok.phone_set      = true;
                    tok.is_explicit_en = true;
                    tokens.push_back(std::move(tok));
                }
            }

        } else if (cap.has(2)) {
            const std::string& word = cap.get(2);
            std::string lw = to_lower_ascii(word);

            std::string merged = cached_lookup_merged(lw);
            if (!merged.empty()) {
                bool is_en = merged.find("<en>") != std::string::npos;
                Token tok;
                tok.lang      = is_en ? "en" : "vi";
                tok.content   = word;
                tok.phone     = trim_str(replace_all_str(merged, "<en>", ""));
                tok.phone_set = true;
                tokens.push_back(std::move(tok));
            } else {
                auto common = cached_lookup_common(lw);
                if (common) {
                    // Store vi and en phonemes separated by \x1F
                    Token tok;
                    tok.lang      = "common";
                    tok.content   = word;
                    tok.phone     = "\x1F" + trim_str(common->first)
                                  + "\x1F" + trim_str(replace_all_str(common->second, "<en>", ""))
                                  + "\x1F";
                    tok.phone_set = true;
                    tokens.push_back(std::move(tok));
                } else {
                    // OOV — detect language by diacritics
                    bool has_vi = false;
                    const char* ptr = lw.data();
                    const char* end = ptr + lw.size();
                    while (ptr < end) {
                        if (is_vi_accent_cp(utf8_next(ptr, end))) { has_vi = true; break; }
                    }
                    Token tok;
                    tok.lang      = has_vi ? "vi" : "en";
                    tok.content   = word;
                    tok.phone_set = false;
                    tokens.push_back(std::move(tok));
                }
            }

        } else if (cap.has(3)) {
            Token tok;
            tok.lang      = "punct";
            tok.content   = cap.get(3);
            tok.phone     = cap.get(3);
            tok.phone_set = true;
            tokens.push_back(std::move(tok));
        }
    }

    propagate_language(tokens);

    std::vector<std::string> result;
    result.reserve(tokens.size());

    for (const Token& t : tokens) {
        if (t.lang == "punct") {
            result.push_back(t.content);
            continue;
        }

        std::string phone;
        if (t.phone_set) {
            const std::string& p = t.phone;
            if (!p.empty() && p.front() == '\x1F' && p.back() == '\x1F') {
                // Common token: extract vi or en part
                size_t sep1 = p.find('\x1F', 1);
                if (t.lang == "en") {
                    phone = (sep1 + 1 < p.size() - 1)
                          ? p.substr(sep1 + 1, p.size() - 1 - (sep1 + 1))
                          : std::string{};
                    // Special rule for "a" not in explicit <en> tag
                    if (to_lower_ascii(t.content) == "a" && !t.is_explicit_en)
                        phone = "ɐ";
                } else {
                    phone = p.substr(1, sep1 - 1);
                }
            } else {
                phone = p;
                // Special rule for 'a'
                if (t.lang == "en"
                    && to_lower_ascii(t.content) == "a"
                    && !t.is_explicit_en) {
                    phone = "ɐ";
                }
            }
        } else {
            // OOV fallback chain
            std::string lw = to_lower_ascii(t.content);
            phone = segment_oov(lw, t.lang);
            if (phone.empty()) phone = char_fallback(t.content, t.lang);
        }

        result.push_back(trim_str(phone));
    }

    // Join tokens
    std::string joined;
    for (size_t i = 0; i < result.size(); ++i) {
        if (i) joined += ' ';
        joined += result[i];
    }

    // Fix spaces before punctuation
    joined = replace_all_str(joined, " .", ".");
    joined = replace_all_str(joined, " ,", ",");
    joined = replace_all_str(joined, " !", "!");
    joined = replace_all_str(joined, " ?", "?");
    joined = replace_all_str(joined, " ;", ";");
    joined = replace_all_str(joined, " :", ":");
    return joined;
}

// ── phonemize_batch ───────────────────────────────────────────────────────────

std::vector<std::string> G2PEngine::phonemize_batch(
    const std::vector<std::string>& texts) const
{
    std::vector<std::string> results(texts.size());
    unsigned int nthreads = std::max(1u, std::thread::hardware_concurrency());
    size_t n = texts.size();

    if (n <= nthreads || nthreads == 1) {
        for (size_t i = 0; i < n; ++i)
            results[i] = phonemize(texts[i]);
        return results;
    }

    std::vector<std::future<void>> futures;
    size_t chunk = (n + nthreads - 1) / nthreads;
    for (size_t t = 0; t < nthreads; ++t) {
        size_t start = t * chunk;
        size_t end_  = std::min(start + chunk, n);
        if (start >= end_) break;
        futures.push_back(std::async(std::launch::async, [&, start, end_]() {
            for (size_t i = start; i < end_; ++i)
                results[i] = phonemize(texts[i]);
        }));
    }
    for (auto& f : futures) f.get();
    return results;
}

}  // namespace sea_g2p
