// resources.cpp — static lookup tables (maps to src/vi_normalizer/resources.rs)
#include "sea_g2p/resources.hpp"

namespace sea_g2p {

const std::unordered_map<std::string, std::string>& vi_letter_names() {
    static const std::unordered_map<std::string, std::string> m = {
        {"a","a"},       {"b","bê"},      {"c","xê"},      {"d","đê"},
        {"đ","đê"},      {"e","e"},        {"ê","ê"},        {"f","ép"},
        {"g","gờ"},      {"h","hát"},      {"i","i"},        {"j","giây"},
        {"k","ca"},      {"l","lờ"},       {"m","mờ"},       {"n","nờ"},
        {"o","ô"},       {"ô","ô"},        {"ơ","ơ"},        {"p","pê"},
        {"q","qui"},     {"r","rờ"},       {"s","ét"},       {"t","tê"},
        {"u","u"},       {"ư","ư"},        {"v","vê"},       {"w","đắp liu"},
        {"x","ích"},     {"y","y"},        {"z","dét"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& measurement_key_vi() {
    static const std::unordered_map<std::string, std::string> m = {
        {"km","ki lô mét"},         {"dm","đê xi mét"},
        {"cm","xen ti mét"},        {"mm","mi li mét"},
        {"nm","na nô mét"},         {"µm","mic rô mét"},
        {"μm","mic rô mét"},        {"m","mét"},
        {"kg","ki lô gam"},         {"g","gam"},
        {"µg","mic rô gam"},        {"mg","mi li gam"},
        {"km2","ki lô mét vuông"},  {"m2","mét vuông"},
        {"cm2","xen ti mét vuông"}, {"mm2","mi li mét vuông"},
        {"ha","héc ta"},
        {"km3","ki lô mét khối"},   {"m3","mét khối"},
        {"cm3","xen ti mét khối"},  {"mm3","mi li mét khối"},
        {"l","lít"},                {"dl","đê xi lít"},
        {"ml","mi li lít"},         {"hl","héc tô lít"},
        {"kw","ki lô oát"},         {"mw","mê ga oát"},
        {"gw","gi ga oát"},         {"kwh","ki lô oát giờ"},
        {"kWh","ki lô oát giờ"},    {"mwh","mê ga oát giờ"},
        {"wh","oát giờ"},
        {"hz","héc"},               {"khz","ki lô héc"},
        {"mhz","mê ga héc"},        {"ghz","gi ga héc"},
        {"pa","__start_en__pascal__end_en__"},
        {"kpa","__start_en__kilopascal__end_en__"},
        {"mpa","__start_en__megapascal__end_en__"},
        {"bar","__start_en__bar__end_en__"},
        {"mbar","__start_en__millibar__end_en__"},
        {"atm","__start_en__atmosphere__end_en__"},
        {"psi","__start_en__p s i__end_en__"},
        {"j","__start_en__joule__end_en__"},
        {"kj","__start_en__kilojoule__end_en__"},
        {"cal","__start_en__calorie__end_en__"},
        {"kcal","__start_en__kilocalorie__end_en__"},
        {"h","giờ"},    {"p","phút"},   {"s","giây"},
        {"sqm","mét vuông"}, {"cum","mét khối"},
        {"gb","__start_en__gigabyte__end_en__"},
        {"mb","__start_en__megabyte__end_en__"},
        {"kb","__start_en__kilobyte__end_en__"},
        {"tb","__start_en__terabyte__end_en__"},
        {"db","__start_en__decibel__end_en__"},
        {"oz","__start_en__ounce__end_en__"},
        {"lb","__start_en__pound__end_en__"},
        {"lbs","__start_en__pounds__end_en__"},
        {"ft","__start_en__feet__end_en__"},
        {"in","__start_en__inch__end_en__"},
        {"dpi","__start_en__d p i__end_en__"},
        {"ph","pê hát"},
        {"gbps","__start_en__gigabits per second__end_en__"},
        {"mbps","__start_en__megabits per second__end_en__"},
        {"kbps","__start_en__kilobits per second__end_en__"},
        {"gallon","__start_en__gallon__end_en__"},
        {"mol","mol"},  {"ms","mi li giây"},
        {"M","triệu"},  {"B","tỷ"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& currency_key() {
    static const std::unordered_map<std::string, std::string> m = {
        {"usd","__start_en__u s d__end_en__"},
        {"vnd","việt nam đồng"},
        {"vnđ","việt nam đồng"},
        {"đ","đồng"},
        {"v n d","việt nam đồng"},
        {"v n đ","việt nam đồng"},
        {"€","__start_en__euro__end_en__"},
        {"euro","__start_en__euro__end_en__"},
        {"eur","__start_en__euro__end_en__"},
        {"¥","yên"},
        {"yên","yên"},
        {"jpy","yên"},
        {"%","phần trăm"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& acronyms_exceptions_vi() {
    static const std::unordered_map<std::string, std::string> m = {
        {"CĐV","cổ động viên"},       {"HĐND","hội đồng nhân dân"},
        {"HĐQT","hội đồng quản trị"}, {"TAND","toàn án nhân dân"},
        {"BHXH","bảo hiểm xã hội"},   {"BHTN","bảo hiểm thất nghiệp"},
        {"TP.HCM","thành phố hồ chí minh"}, {"VN","việt nam"},
        {"UBND","uỷ ban nhân dân"},    {"TP","thành phố"},
        {"HCM","hồ chí minh"},         {"HN","hà nội"},
        {"BTC","ban tổ chức"},         {"CLB","câu lạc bộ"},
        {"HTX","hợp tác xã"},          {"NXB","nhà xuất bản"},
        {"TW","trung ương"},           {"CSGT","cảnh sát giao thông"},
        {"LHQ","liên hợp quốc"},       {"THCS","trung học cơ sở"},
        {"THPT","trung học phổ thông"}, {"ĐH","đại học"},
        {"HLV","huấn luyện viên"},     {"GS","giáo sư"},
        {"TS","tiến sĩ"},              {"TNHH","trách nhiệm hữu hạn"},
        {"VĐV","vận động viên"},       {"TPHCM","thành phố hồ chí minh"},
        {"PGS","phó giáo sư"},         {"SP500","ét pê năm trăm"},
        {"PGS.TS","phó giáo sư tiến sĩ"}, {"GS.TS","giáo sư tiến sĩ"},
        {"ThS","thạc sĩ"},             {"BS","bác sĩ"},
        {"UAE","u a e"},               {"CUDA","cu đa"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& technical_terms() {
    static const std::unordered_map<std::string, std::string> m = {
        {"JSON","__start_en__j son__end_en__"},
        {"VRAM","__start_en__v ram__end_en__"},
        {"NVIDIA","__start_en__n v d a__end_en__"},
        {"VN-Index","__start_en__v n__end_en__ index"},
        {"MS DOS","__start_en__m s dos__end_en__"},
        {"MS-DOS","__start_en__m s dos__end_en__"},
        {"B2B","__start_en__b two b__end_en__"},
        {"MI5","__start_en__m i five__end_en__"},
        {"MI6","__start_en__m i six__end_en__"},
        {"2FA","__start_en__two f a__end_en__"},
        {"TX-0","__start_en__t x zero__end_en__"},
        {"IPv6","__start_en__i p v__end_en__ sáu"},
        {"IPv4","__start_en__i p v__end_en__ bốn"},
        {"Washington D.C","__start_en__washington d c__end_en__"},
        {"Washington DC","__start_en__washington d c__end_en__"},
        {"HCN","hát xê nờ"},
        {"HF","hát ép"},
        {"KI","ca i"},
        {"KOH","ca ô hát"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& combined_exceptions() {
    static const std::unordered_map<std::string, std::string> m = []() {
        std::unordered_map<std::string, std::string> tmp;
        for (const auto& [k, v] : acronyms_exceptions_vi()) tmp[k] = v;
        for (const auto& [k, v] : technical_terms())        tmp[k] = v;
        return tmp;
    }();
    return m;
}

const std::unordered_map<std::string, std::string>& domain_suffix_map() {
    static const std::unordered_map<std::string, std::string> m = {
        {"com","com"},
        {"vn","__start_en__v n__end_en__"},
        {"net","nét"},
        {"org","o rờ gờ"},
        {"edu","__start_en__edu__end_en__"},
        {"gov","gờ o vê"},
        {"io","__start_en__i o__end_en__"},
        {"biz","biz"},
        {"info","info"},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& currency_symbol_map() {
    static const std::unordered_map<std::string, std::string> m = {
        {"$","__start_en__u s d__end_en__"},
        {"€","__start_en__euro__end_en__"},
        {"¥","yên"},
        {"£","__start_en__pound__end_en__"},
        {"₩","won"},
    };
    return m;
}

const std::unordered_map<char, int>& roman_numerals() {
    static const std::unordered_map<char, int> m = {
        {'I',1}, {'V',5}, {'X',10}, {'L',50},
        {'C',100}, {'D',500}, {'M',1000},
    };
    return m;
}

const std::unordered_map<std::string, std::string>& abbrs() {
    static const std::unordered_map<std::string, std::string> m = {
        {"v.v"," vân vân"},
        {"v/v"," về việc"},
        {"đ/c","địa chỉ"},
    };
    return m;
}

const std::unordered_map<uint32_t, std::string>& symbols_map() {
    static const std::unordered_map<uint32_t, std::string> m = {
        {'&'," và "},       {'+'," cộng "},      {'='," bằng "},
        {'#'," thăng "},    {'>'," lớn hơn "},   {'<'," nhỏ hơn "},
        {0x2265," lớn hơn hoặc bằng "}, {0x2264," nhỏ hơn hoặc bằng "},
        {0x00B1," cộng trừ "}, {0x2248," xấp xỉ "}, {'/'," trên "},
        {0x2192," đến "},   {0x00F7," chia "},   {'*'," sao "},
        {0x00D7," nhân "},  {'^'," mũ "},         {'~'," khoảng "},
        {'%'," phần trăm "}, {'$'," đô la "},   {0x20AC," ê rô "},
        {0x00A3," bảng "},  {0x00A5," yên "},    {0x20A9," won "},
        {0x20AD," kíp "},   {0x20B1," bê xô "},  {0x0E3F," bạc "},
        {0x03A9," ôm "},    {'@'," a còng "},     {0x2260," khác "},
        {0x2200," với mọi "}, {0x220F," tích "}, {0x2208," thuộc "},
        {0x2211," tổng "},  {0x2229," giao "},   {0x222A," hội "},
        {0x00AC," phủ định "}, {0x221E," vô cùng "},
        {0x03B1," an pha "}, {0x03B2," bê ta "}, {0x03B3," ga ma "},
        {0x03B4," đen ta "}, {0x03B5," ép si lon "}, {0x03F5," thuộc "},
        {0x03B6," de ta "}, {0x03B7," ê ta "},   {0x03B8," thê ta "},
        {0x03B9," i ô ta "}, {0x03BA," cáp ba "}, {0x03BB," lam đa "},
        {0x1D27," và "},    {0x03BC," muy "},     {0x0394," đen ta "},
        {0x03BD," nu "},    {0x03BE," xi xi "},   {0x03BF," o mi ron "},
        {0x03C0," pi "},    {0x03C1," ro "},      {0x03C3," xích ma "},
        {0x03C4," tao "},   {0x03C5," úp si lon "}, {0x03C6," phi "},
        {0x03C7," chi "},   {0x03C8," si "},      {0x03C9," ô me ga "},
        {0x00A9," bản quyền "},
        {0x00BD," một phần hai "}, {0x00BC," một phần tư "}, {0x00BE," ba phần tư "},
        {0x2153," một phần ba "}, {0x2154," hai phần ba "},
        {0x2155," một phần năm "}, {0x2156," hai phần năm "}, {0x2157," ba phần năm "},
        {0x2158," bốn phần năm "}, {0x215A," năm phần sáu "},
    };
    return m;
}

const std::unordered_map<uint32_t, std::string>& superscripts_map() {
    static const std::unordered_map<uint32_t, std::string> m = {
        {0x2070," không "}, {0x00B9," một "}, {0x00B2," bình phương "},
        {0x00B3," lập phương "}, {0x2074," bốn "}, {0x2075," năm "},
        {0x2076," sáu "},   {0x2077," bảy "}, {0x2078," tám "}, {0x2079," chín "},
    };
    return m;
}

const std::unordered_map<uint32_t, std::string>& subscripts_map() {
    static const std::unordered_map<uint32_t, std::string> m = {
        {0x2080," không "}, {0x2081," một "}, {0x2082," hai "}, {0x2083," ba "},
        {0x2084," bốn "},   {0x2085," năm "}, {0x2086," sáu "}, {0x2087," bảy "},
        {0x2088," tám "},   {0x2089," chín "},
    };
    return m;
}

const std::unordered_set<std::string>& word_like_acronyms() {
    static const std::unordered_set<std::string> s = {
        "UNESCO","NASA","NATO","ASEAN","OPEC","SARS","FIFA","UNIC",
        "RAM","VRAM","COVID","IELTS","STEM","SWAT","SEAL","WASP",
        "COBOL","BASIC","OLED","COVAX","BRICS","APEC","VUCA","PERMA",
        "DINK","MENA","EPIC","OASIS","BASE","DART","IDEA","CHAOS",
        "SMART","FANG","BLEU","REST","ERROR","SELECT","FROM","WHERE",
        "ORDER","BY","LIMIT","OFFSET","GROUP","HAVING","JOIN","LEFT",
        "RIGHT","INNER","OUTER","ON","AS","AND","OR","NOT","IN",
        "BETWEEN","LIKE","IS","NULL","TRUE","FALSE","CASE","WHEN",
        "THEN","ELSE","END","UNION","INTERSECT","EXCEPT","DESC",
    };
    return s;
}

const std::unordered_map<std::string, std::string>& common_email_domains() {
    static const std::unordered_map<std::string, std::string> m = {
        {"gmail.com",   "__start_en__gmail__end_en__ chấm com"},
        {"yahoo.com",   "__start_en__yahoo__end_en__ chấm com"},
        {"yahoo.com.vn","__start_en__yahoo__end_en__ chấm com chấm __start_en__v n__end_en__"},
        {"outlook.com", "__start_en__outlook__end_en__ chấm com"},
        {"hotmail.com", "__start_en__hotmail__end_en__ chấm com"},
        {"icloud.com",  "__start_en__icloud__end_en__ chấm com"},
        {"fpt.vn",      "__start_en__f p t__end_en__ chấm __start_en__v n__end_en__"},
        {"fpt.com.vn",  "__start_en__f p t__end_en__ chấm com chấm __start_en__v n__end_en__"},
    };
    return m;
}

const std::unordered_set<std::string>& date_keywords() {
    static const std::unordered_set<std::string> s = {
        "vào","ngày","hôm","hôm nay","hôm qua","hôm kia","mai",
        "ngày mai","ngày kia","sinh","sinh nhật","kỷ niệm","lễ","tết",
        "diễn ra","tổ chức","thứ","tuần","tháng","năm",
    };
    return s;
}

const std::unordered_set<std::string>& math_keywords() {
    static const std::unordered_set<std::string> s = {
        "cộng","trừ","nhân","chia","bằng","sin","cos","tan","log",
        "sqrt","xác suất","tỷ lệ","tỉ lệ",
    };
    return s;
}

}  // namespace sea_g2p
