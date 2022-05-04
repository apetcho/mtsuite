cmake_minimum_required(VERSION 3.17)
project(mstuite LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED 11)

add_library(mtsuite src/mtsuite.c src/mtsuite.h)
add_executable(mtsuitedemo src/mtsuitedemo.c)
target_link_libraries(mtsuitedemo mtsuite)

