#include "duplicate_finder.h"
#include <fstream>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/uuid/detail/md5.hpp>


void DuplicateFinder::scan_directories() {
    for (const boost::filesystem::path& base_dir : directories_) {
        if (!boost::filesystem::exists(base_dir) || !boost::filesystem::is_directory(base_dir)) continue;
        boost::filesystem::recursive_directory_iterator it(base_dir), end;
        while (it != end) {
            boost::filesystem::path current = it->path();
            // Исключение каталогов из рекурсии
            if (boost::filesystem::is_directory(current)) {
                for (const boost::filesystem::path& excl : exclude_dirs_) {
                    if (current == excl) {
                        it.disable_recursion_pending();  // не углубляемся в эту директорию
                        goto next_entry;
                    }
                    // Проверка, является ли current подпутём excl
                    if (is_sub_path(current, excl)) {
                        it.disable_recursion_pending();
                        goto next_entry;
                    }
                }
            }
            // Проверка глубины рекурсии
            // Исключение каталогов из рекурсии
            if (boost::filesystem::is_directory(current)) {
                for (const boost::filesystem::path& excl : exclude_dirs_) {
                    if (current == excl || is_sub_path(current, excl)) {
                        it.disable_recursion_pending();
                        goto next_entry;
                    }
                }
            }
            // Обработка файлов
            if (boost::filesystem::is_regular_file(current)) {
                uintmax_t file_size = boost::filesystem::file_size(current);
                if (file_size >= min_size_ && match_mask(current.filename().string())) {
                    FileInfo* info = new FileInfo;
                    info->path = current;
                    info->size = file_size;
                    info->block_count = (file_size + block_size_ - 1) / block_size_;
                    all_files_.push_back(info);
                }
            }
        next_entry:
            ++it;
        }
    }
}


// Проверка, что current является подпутём exclude (для исключения)
bool DuplicateFinder::is_sub_path(const boost::filesystem::path& current, const boost::filesystem::path& exclude) {
    boost::filesystem::path cur_abs, excl_abs;
    try { cur_abs = boost::filesystem::canonical(current); }
    catch (...) { return false; }
    try { excl_abs = boost::filesystem::canonical(exclude); }
    catch (...) { return false; }
    auto cur_it = cur_abs.begin();
    auto excl_it = excl_abs.begin();
    while (cur_it != cur_abs.end() && excl_it != excl_abs.end() && *cur_it == *excl_it) {
        ++cur_it; ++excl_it;
    }
    return excl_it == excl_abs.end();
}


// Проверка имени файла по маске (несколько масок)
bool DuplicateFinder::match_mask(const std::string& name) {
    if (masks_.empty()) return true;
    for (const std::string& mask : masks_) {
        std::string regex_pattern;
        for (char c : mask) {
            if (c == '*') regex_pattern += ".*";
            else if (c == '?') regex_pattern += ".";
            else if (std::string(".^$|()[]{}+\\/").find(c) != std::string::npos) {
                regex_pattern += '\\';
                regex_pattern += c;
            } else {
                regex_pattern += c;
            }
        }
        boost::regex pattern(regex_pattern, boost::regex::icase);
        if (boost::regex_match(name, pattern)) {
            return true;
        }
    }
    return false;
}

// Группировка файлов по размеру и рекурсивное сравнение по блокам
void DuplicateFinder::find_duplicates() {
    std::unordered_map<uintmax_t, std::vector<FileInfo*>> by_size;
    for (FileInfo* f : all_files_) {
        by_size[f->size].push_back(f);
    }
    for (auto& kv : by_size) {
        std::vector<FileInfo*>& group = kv.second;
        if (group.size() > 1) {
            find_groups_recursive(group, 0);
        }
    }
}


// Рекурсивное разбиение группы по хешам блоков
void DuplicateFinder::find_groups_recursive(std::vector<FileInfo*>& group, size_t block_index) {
    if (group.size() <= 1) return;
    bool any_ended = false, all_ended = true;
    for (FileInfo* f : group) {
        if (block_index >= f->block_count) any_ended = true;
        else all_ended = false;
    }
    // Если достигнут конец у части файлов
    if (any_ended) {
        if (all_ended) {
            // Все файлы закончились одновременно — это группа дубликатов
            std::vector<boost::filesystem::path> dup_group;
            for (FileInfo* f : group) {
                dup_group.push_back(f->path);
            }
            duplicate_groups_.push_back(dup_group);
        }
        return;
    }
    // Вычисляем хеш текущего блока и группируем по значению
    std::unordered_map<std::string, std::vector<FileInfo*>> by_hash;
    for (FileInfo* f : group) {
        std::string hash;
        if (block_index < f->block_hashes.size()) {
            hash = f->block_hashes[block_index];
        } else {
            hash = compute_block_hash(f->path, block_index);
            f->block_hashes.push_back(hash);
        }
        by_hash[hash].push_back(f);
    }
    for (auto& kv : by_hash) {
        find_groups_recursive(kv.second, block_index + 1);
    }
}


// Чтение блока файла и вычисление MD5
std::string DuplicateFinder::compute_block_hash(const boost::filesystem::path& file_path, size_t block_index) {
    std::ifstream ifs(file_path.string(), std::ios::binary);
    std::vector<char> buffer(block_size_, 0);
    if (!ifs) return "";
    ifs.seekg(static_cast<std::streamoff>(block_index) * static_cast<std::streamoff>(block_size_));
    ifs.read(buffer.data(), buffer.size());
    // Если прочитано меньше, хвост остаётся нулями
    boost::uuids::detail::md5 md5;
    md5.process_bytes(buffer.data(), buffer.size());
    boost::uuids::detail::md5::digest_type digest;
    md5.get_digest(digest);
    const uint32_t* int_digest = reinterpret_cast<const uint32_t*>(&digest);
    std::string hex_str;
    boost::algorithm::hex(int_digest, int_digest + sizeof(boost::uuids::detail::md5::digest_type)/sizeof(uint32_t), std::back_inserter(hex_str));
    return hex_str;
}

// Вывод найденных групп дубликатов
void DuplicateFinder::print_results() {
    for (const auto& group : duplicate_groups_) {
        for (const boost::filesystem::path& p : group) {
            std::cout << p.string() << std::endl;
        }
        std::cout << std::endl;
    }
}