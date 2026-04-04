#include "sea_g2p/g2p.hpp"
#include "sea_g2p/vi_normalizer.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <stdexcept>

void test_normalization() {
    std::cout << "--- Testing Normalization ---" << std::endl;
    try {
        sea_g2p::Normalizer normalizer("vi");
        
        struct TestCase {
            std::string input;
            std::string expected;
        };

        std::vector<TestCase> test_cases = {
            {"Giá SP500 hôm nay là 4.200,5 điểm", "giá ét pê năm trăm hôm nay là bốn nghìn hai trăm phẩy năm điểm"},
            {"Ngày 15/03/2024", "ngày mười lăm tháng ba năm hai nghìn không trăm hai mươi tư"},
            {"Số 0912345678", "số không chín một hai ba bốn năm sáu bảy tám"},
            {"Nhiệt độ 38.5°C", "nhiệt độ ba mươi tám chấm năm độ xê"},
            {"support@example.com", "<en>support</en> a còng <en>example</en> chấm com"}
        };

        for (const auto& tc : test_cases) {
            std::string result = normalizer.normalize(tc.input);
            if (result != tc.expected) {
                std::cerr << "FAIL Normalize: [" << tc.input << "] -> [" << result << "] != [" << tc.expected << "]" << std::endl;
            } else {
                std::cout << "PASS Normalize: [" << tc.input << "]" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "CRASH in test_normalization: " << e.what() << std::endl;
        throw;
    }
}

void test_g2p(const std::string& dict_path) {
    std::cout << "--- Testing G2P ---" << std::endl;
    try {
        sea_g2p::G2PEngine g2p(dict_path);
        
        struct TestCase {
            std::string input;
        };

        std::vector<TestCase> test_cases = {
            {"xin chào"},
            {"anh yêu em"}
        };

        for (const auto& tc : test_cases) {
            std::string result = g2p.phonemize(tc.input);
            std::cout << "G2P result for [" << tc.input << "]: " << result << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "CRASH in test_g2p: " << e.what() << std::endl;
        throw;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <dict_path>" << std::endl;
        return 1;
    }

    std::string dict_path = argv[1];

    try {
        std::cout << "Running tests..." << std::endl;
        test_normalization();
        test_g2p(dict_path);
        std::cout << "Tests finished successfully." << std::endl;
    } catch (...) {
        std::cerr << "Tests failed with unknown exception." << std::endl;
        return 1;
    }

    return 0;
}
