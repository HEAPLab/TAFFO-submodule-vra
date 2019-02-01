cmake_minimum_required(VERSION 3.4.3) # like llvm

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS}
    ../MDUtils)
link_directories(../MDUtils)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_subdirectory(TaffoVRA)
