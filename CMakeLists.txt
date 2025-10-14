cmake_minimum_required(VERSION 3.12)
project(lmcvm LANGUAGES C VERSION 0.0.0)

add_library(lmcvm_interface INTERFACE)
target_compile_features(lmcvm_interface INTERFACE c_std_11)
target_include_directories(lmcvm_interface INTERFACE src)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")

add_subdirectory(src)