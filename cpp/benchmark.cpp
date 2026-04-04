#include "src/win_utf8.hpp"
#include "sea_g2p/g2p.hpp"
#include "sea_g2p/vi_normalizer.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    sea_g2p::setup_utf8_console();

#ifdef _WIN32
    auto args = sea_g2p::get_utf8_args();
#else
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
#endif

    std::string dict_path;
    if (args.size() >= 2) {
        dict_path = args[1];
    }

    try {
        sea_g2p::Normalizer normalizer("vi");
        std::unique_ptr<sea_g2p::G2PEngine> g2p;

        if (dict_path.empty()) {
            g2p = std::make_unique<sea_g2p::G2PEngine>();
        } else {
            g2p = std::make_unique<sea_g2p::G2PEngine>(dict_path);
        }

        std::vector<std::string> base_sentences = {
            "Giá SP500 hôm nay là 4.200,5 điểm",
            "Ngày 15/03/2024, thời tiết Hà Nội rất đẹp.",
            "Tôi đang học tại trường Đại học Bách Khoa TP.HCM.",
            "Số điện thoại của tôi là 0912-345-678.",
            "Nhiệt độ ngoài trời là 38.5°C.",
            "Công thức H2O là của nước.",
            "Chào bạn, bạn khỏe không?",
            "Mời bạn uống cốc café 20k.",
            "Tỉ giá USD/VND hiện nay là 24.500.",
            "Ông ấy sinh năm 1990."
        };

        std::vector<std::string> test_sentences;
        for (int i = 0; i < 1000; ++i) {
            for (const auto& s : base_sentences) {
                test_sentences.push_back(s);
            }
        }

        std::cout << "Benchmarking with " << test_sentences.size() << " sentences..." << std::endl;

        // Benchmark Normalization (Batch/Parallel)
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::string> normalized_texts = normalizer.normalize_batch(test_sentences);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << "Normalization (Batch) time: " << std::fixed << std::setprecision(4) 
                  << duration.count() << "s (" << (test_sentences.size() / duration.count()) 
                  << " sentences/sec)" << std::endl;

        // Benchmark G2P Batch
        start = std::chrono::high_resolution_clock::now();
        std::vector<std::string> phonemes = g2p->phonemize_batch(normalized_texts);
        end = std::chrono::high_resolution_clock::now();
        duration = end - start;
        std::cout << "G2P Batch time: " << std::fixed << std::setprecision(4) 
                  << duration.count() << "s (" << (test_sentences.size() / duration.count()) 
                  << " sentences/sec)" << std::endl;

        // Total
        static double total_time = duration.count(); // actually just sum of both
        // We need the normalization duration again
        // Actually, just calculate total
        // I'll just re-calculate it to be sure
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
