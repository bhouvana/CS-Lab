// CLI: lz77 compress|decompress <in> <out>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "lz77.hpp"

namespace {
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void write_file(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    out.write((const char*)data.data(), (std::streamsize)data.size());
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage:\n  " << argv[0] << " compress   <in> <out>\n  " << argv[0]
                  << " decompress <in> <out>\n";
        return 2;
    }
    std::string mode = argv[1], in_path = argv[2], out_path = argv[3];

    try {
        if (mode == "compress") {
            auto data = read_file(in_path);
            auto tokens = lz77_compress(data);
            auto bytes = serialize_tokens(tokens);
            write_file(out_path, bytes);

            double ratio = data.empty() ? 0.0 : 100.0 * (double)bytes.size() / (double)data.size();
            std::cout << "Original:   " << data.size() << " bytes\n";
            std::cout << "Tokens:     " << tokens.size() << "\n";
            std::cout << "Compressed: " << bytes.size() << " bytes\n";
            std::cout << std::fixed << std::setprecision(2) << "Ratio:      " << ratio << "%\n";
        } else if (mode == "decompress") {
            auto bytes = read_file(in_path);
            auto tokens = deserialize_tokens(bytes);
            auto data = lz77_decompress(tokens);
            write_file(out_path, data);
        } else {
            std::cerr << "unknown mode '" << mode << "'\n";
            return 2;
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
