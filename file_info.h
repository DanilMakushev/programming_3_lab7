#pragma once
#include <string>
#include <vector>
#include <boost/filesystem.hpp>

// Структура для хранения информации о файле
struct FileInfo {
    boost::filesystem::path path;
    uintmax_t size;
    size_t block_count;
    std::vector<std::string> block_hashes;  // Хеши прочитанных блоков
};