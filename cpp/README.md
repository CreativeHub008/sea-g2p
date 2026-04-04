# sea-g2p C++ API

High-performance C++ implementation of the SEA-G2P engine for Vietnamese.

## 🚀 Quick Start (CMake FetchContent)

The easiest way to use `sea-g2p` in your project is via CMake's `FetchContent`. It will automatically download the source, satisfy the `PCRE2` dependency, and link it to your target.

```cmake
include(FetchContent)
FetchContent_Declare(
    sea_g2p
    GIT_REPOSITORY https://github.com/CreativeHub008/sea-g2p.git
    GIT_TAG        main # or a specific version/commit
    SOURCE_SUBDIR  cpp
)
FetchContent_MakeAvailable(sea_g2p)

# Link to your executable/library
target_link_libraries(my_app PRIVATE sea_g2p::sea_g2p)
```

## 🛠️ Usage

### Zero-Config Initialization
If you build and run your application, the engine will automatically look for `sea_g2p.bin` in:
1. The same directory as your executable.
2. The current working directory.
3. The environment variable `SEA_G2P_DICT_PATH`.

```cpp
#include <sea_g2p/vi_normalizer.hpp>
#include <sea_g2p/g2p.hpp>
#include <iostream>

int main() {
    try {
        // Automatic dictionary discovery
        sea_g2p::Normalizer normalizer("vi");
        sea_g2p::G2PEngine g2p; 

        std::string text = "Giá SP500 hôm nay là 4.200,5 điểm.";
        
        // 1. Normalize (expand numbers, dates, units)
        std::string normalized = normalizer.normalize(text);
        
        // 2. Phonemize
        std::string phonemes = g2p.phonemize(normalized);

        std::cout << "Normalized: " << normalized << std::endl;
        std::cout << "Phonemes:   " << phonemes << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

## 🏗️ Building & Installing

If you want to build and install the library to your system:

```bash
cd cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cmake --install . --prefix /path/to/install
```

### Build Options
- `SEA_G2P_ENABLE_ICU`: (Default: ON) Use ICU for NFC normalization. Highly recommended for Vietnamese.
- `SEA_G2P_BUILD_SHARED`: (Default: OFF) Build as a shared library.
- `SEA_G2P_FETCH_PCRE2`: (Default: ON) Automatically download PCRE2 if not found.

## 📄 License
This C++ port is released under the same license as the main project.
