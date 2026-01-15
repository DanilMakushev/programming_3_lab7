#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "file_info.h"

// Класс утилиты поиска дубликатов
class DuplicateFinder {
public:
    DuplicateFinder(const std::vector<boost::filesystem::path>& dirs,
                    const std::vector<boost::filesystem::path>& exclude_dirs,
                    int max_depth, uintmax_t min_size,
                    const std::vector<std::string>& masks,
                    int block_size)
        : directories_(dirs),
          exclude_dirs_(exclude_dirs),
          max_depth_(max_depth),
          min_size_(min_size),
          masks_(masks),
          block_size_(block_size) {}
    
    // Запуск поиска дубликатов
    void run() {
        scan_directories();
        grouping_files_by_size();
        print_results();
    }

private:
    std::vector<boost::filesystem::path> directories_;
    std::vector<boost::filesystem::path> exclude_dirs_;
    int max_depth_;
    uintmax_t min_size_;
    std::vector<std::string> masks_;
    int block_size_;
    std::vector<FileInfo*> all_files_;
    std::vector<std::vector<boost::filesystem::path>> duplicate_groups_;

    // Сканирование директорий с учётом фильтров для получения списка проверяемых файлов
    void scan_directories();

    // Проверка, что current является подпутём exclude (для исключения)
    bool is_sub_path(const boost::filesystem::path& current, const boost::filesystem::path& exclude);

    // Проверка имени файла по маске (несколько масок)
    bool match_mask(const std::string& name);

    // Группирует файлы по размеру
    void grouping_files_by_size();

    // Возвращает хэш блока
    std::string get_block_hash(FileInfo* f, int block_index);

    // Сравнивает группу файлов, имеющих одинаковый размер, блок за блоком.
    void compare_group(std::vector<FileInfo*>& group);

    // Вывод найдненых групп дубликатов
    void print_results();
};