cmake_minimum_required(VERSION 3.10)
project(laba7)

# Ищем Boost
find_package(Boost REQUIRED COMPONENTS filesystem)

# Настройка стандарта C++
set(CMAKE_CXX_STANDARD 17)

# Создаем исполняемый файл
add_executable(laba7
    main.cpp
    duplicate_finder.cpp
)

# Подключаем Boost
target_include_directories(laba7 PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(laba7 PRIVATE ${Boost_LIBRARIES}) 