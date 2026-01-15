#pragma once
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <fstream>

// Структура для хранения информации о файле
struct FileInfo {
    boost::filesystem::path path;
    uintmax_t size;
    int block_count;
    std::vector<std::string> block_hashes;  // Хеши прочитанных блоков
    std::ifstream stream; // держим открытым!
};