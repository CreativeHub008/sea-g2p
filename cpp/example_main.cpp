#include "src/win_utf8.hpp"
#include "sea_g2p/g2p.hpp"
#include "sea_g2p/vi_normalizer.hpp"
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

    if (args.size() < 2) {
        std::cerr << "Usage (Auto-discovery): " << args[0] << " <text>" << std::endl;
        std::cerr << "Usage (Manual path):    " << args[0] << " <dict_path> <text>" << std::endl;
        return 1;
    }

    std::string dict_path;
    std::string text;

    if (args.size() == 2) {
        // Auto-discovery mode
        text = args[1];
    } else {
        // Explicit path mode
        dict_path = args[1];
        text = args[2];
    }

    try {
        sea_g2p::Normalizer normalizer("vi");
        std::unique_ptr<sea_g2p::G2PEngine> g2p;
        
        if (dict_path.empty()) {
            g2p = std::make_unique<sea_g2p::G2PEngine>();
        } else {
            g2p = std::make_unique<sea_g2p::G2PEngine>(dict_path);
        }

        std::cout << "Input: " << text << std::endl;

        std::string normalized = normalizer.normalize(text);
        std::cout << "Normalized: " << normalized << std::endl;

        std::string phonemes = g2p->phonemize(normalized);
        std::cout << "Phonemes: " << phonemes << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
