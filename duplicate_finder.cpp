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


void DuplicateFinder::grouping_files_by_size() {
    std::unordered_map<uintmax_t, std::vector<FileInfo*>> by_size;

    for (FileInfo* f : all_files_) {
        by_size[f->size].push_back(f);
    }

    for (auto& vect : by_size) {
        auto& group = vect.second;
        if (group.size() > 1) compare_group(group);
    }
}


void DuplicateFinder::compare_group(std::vector<FileInfo*>& group) {
    // Открываем файлы
    for (auto* f : group) {
        if (!f->stream.is_open()) {
            f->stream.open(f->path.string(), std::ios::binary);
        }
    }

    int block{};

    while (true) {
        bool all_end = true;
        bool any_end = false;

        for (auto* f : group) {
            if (block < f->block_count) all_end = false;
            else any_end = true;
        }

        if (any_end) {
            if (all_end) {
                std::vector<boost::filesystem::path> dup;
                for (auto* f : group) dup.push_back(f->path);
                duplicate_groups_.push_back(dup);
            }
            break;
        }

        // разбиение файлов по хэшу текущего блока
        std::unordered_map<std::string, std::vector<FileInfo*>> buckets;
        for (auto* f : group) {
            std::string h = get_block_hash(f, block);
            buckets[h].push_back(f);
        }

        std::vector<std::vector<FileInfo*>> next_groups;
        for (auto& map : buckets) {
            if (map.second.size() > 1)
                next_groups.push_back(map.second);
        }

        // нет подгрупп, где файлов больше одного
        if (next_groups.empty())
            break;

        if (next_groups.size() == 1) // есть только одна подгруппа файлов, которые совпали по текущему блоку
            group = next_groups[0];
        else {
            for (auto& g : next_groups)
                compare_group(g);
            return;
        }

        block++;
    }
}


std::string DuplicateFinder::get_block_hash(FileInfo* f, int block_index) {
    if (block_index < f->block_hashes.size())
        return f->block_hashes[block_index];

    std::vector<char> buffer(block_size_, 0);

    // перемещаем указатель чтения в файле на нужный блок
    f->stream.seekg(static_cast<std::streamoff>(block_index) * block_size_);
    f->stream.read(buffer.data(), buffer.size());

    boost::uuids::detail::md5 md5;
    md5.process_bytes(buffer.data(), buffer.size());
    boost::uuids::detail::md5::digest_type digest;
    md5.get_digest(digest);

    std::string hex;
    boost::algorithm::hex(
        reinterpret_cast<const uint8_t*>(&digest),
        reinterpret_cast<const uint8_t*>(&digest) + sizeof(digest),
        std::back_inserter(hex)
    );

    f->block_hashes.push_back(hex);
    return hex;
}


void DuplicateFinder::print_results() {
    for (const auto& group : duplicate_groups_) {
        for (const boost::filesystem::path& p : group) {
            std::cout << p.string() << std::endl;
        }
        std::cout << std::endl;
    }
}