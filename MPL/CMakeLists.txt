
cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 23)

project("mpl")

add_subdirectory("test")

add_link_options("-L/usr/local/opt/llvm/lib/c++ -Wl,-rpath,/usr/local/opt/llvm/lib/c++")

