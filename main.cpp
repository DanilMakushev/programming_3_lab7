#include "duplicate_finder.h"
#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<boost::filesystem::path> dirs;
    std::vector<boost::filesystem::path> exclude_dirs;
    std::vector<std::string> masks;
    int max_depth = -1;
    uintmax_t min_size = 1;
    unsigned int block_size = 0;
    std::string algo = "md5";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d" && i+1 < argc) {
            dirs.emplace_back(argv[++i]);
        } else if (arg == "-e" && i+1 < argc) {
            exclude_dirs.emplace_back(argv[++i]);
        } else if (arg == "-l" && i+1 < argc) {
            max_depth = std::stoi(argv[++i]);
        } else if (arg == "-m" && i+1 < argc) {
            min_size = std::stoull(argv[++i]);
        } else if (arg == "-p" && i+1 < argc) {
            std::string mask_str = argv[++i];
            int pos = 0;
            while ((pos = mask_str.find(';')) != std::string::npos) {
                masks.push_back(mask_str.substr(0, pos));
                mask_str.erase(0, pos+1);
            }
            masks.push_back(mask_str);
        } else if (arg == "-s" && i+1 < argc) {
            block_size = std::stoi(argv[++i]);
        } else {
            std::cerr << "Неизвестный или неполный аргумент: " << arg << std::endl;
            return 1;
        }
    }
    if (dirs.empty()) {
        std::cerr << "Нет директории для сканирования. используйте -d <directory>" << std::endl;
        return 1;
    }
    if (block_size == 0) {
        std::cerr << "укажите размер блока -s" << std::endl;
        return 1;
    }
    DuplicateFinder finder(dirs, exclude_dirs, max_depth, min_size, masks, block_size);
    finder.run();
    
    return 0;
}