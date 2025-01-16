
cmake_minimum_required(VERSION 3.23)

find_package(Catch2 3 REQUIRED)

find_package(PkgConfig)
pkg_check_modules(GMP REQUIRED gmpxx)
pkg_check_modules(CLN REQUIRED cln)

find_package(benchmark REQUIRED)

add_subdirectory("./rapidcheck")

set(CMAKE_CXX_FLAGS_DEBUG "-g --std=c++23 -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "-g --std=c++23 -O3 -DNDEBUG -fno-omit-frame-pointer")

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=")

add_executable(algorithms algorithms.cpp)
target_link_libraries(algorithms PRIVATE rapidcheck)

target_include_directories(algorithms PRIVATE ${GMP_INCLUDE_DIRS})
target_link_directories(algorithms PRIVATE ${GMP_LIBRARY_DIRS})
target_link_libraries(algorithms PRIVATE ${GMP_LIBRARIES})
target_compile_options(algorithms PRIVATE ${GMP_CFLAGS_OTHER})
target_link_options(algorithms PRIVATE ${GMP_LDFLAGS_OTHER})

target_compile_options(algorithms PRIVATE -Wall -Wextra -Wpedantic)

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
  target_compile_options(algorithms PRIVATE -fsanitize=address)
  target_link_options(algorithms PRIVATE -fsanitize=address)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

add_executable(wrapper wrapper.cpp)
target_link_libraries(wrapper PRIVATE rapidcheck)

target_include_directories(wrapper PRIVATE ${GMP_INCLUDE_DIRS})
target_link_directories(wrapper PRIVATE ${GMP_LIBRARY_DIRS})
target_link_libraries(wrapper PRIVATE ${GMP_LIBRARIES})
target_compile_options(wrapper PRIVATE ${GMP_CFLAGS_OTHER})
target_link_options(wrapper PRIVATE ${GMP_LDFLAGS_OTHER})

target_compile_options(wrapper PRIVATE -Wall -Wextra -Wpedantic)

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
  target_compile_options(wrapper PRIVATE -fsanitize=address)
  target_link_options(wrapper PRIVATE -fsanitize=address)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

add_executable(rational rational.cpp)
target_link_libraries(rational PRIVATE rapidcheck)

target_include_directories(rational PRIVATE ${GMP_INCLUDE_DIRS})
target_link_directories(rational PRIVATE ${GMP_LIBRARY_DIRS})
target_link_libraries(rational PRIVATE ${GMP_LIBRARIES})
target_compile_options(rational PRIVATE ${GMP_CFLAGS_OTHER})
target_link_options(rational PRIVATE ${GMP_LDFLAGS_OTHER})

target_compile_options(rational PRIVATE -Wall -Wextra -Wpedantic)

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
  target_compile_options(rational PRIVATE -fsanitize=address)
  target_link_options(rational PRIVATE -fsanitize=address)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

add_executable(int_container int_container.cpp)
target_link_libraries(int_container PRIVATE rapidcheck)

target_compile_options(int_container PRIVATE -Wall -Wextra -Wpedantic)

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
  target_compile_options(int_container PRIVATE -fsanitize=address)
  target_link_options(int_container PRIVATE -fsanitize=address)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

add_executable(int_container2 int_container2.cpp)
target_link_libraries(int_container2 PRIVATE rapidcheck)

target_compile_options(int_container2 PRIVATE -Wall -Wextra -Wpedantic)

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
  target_compile_options(int_container2 PRIVATE -fsanitize=address)
  target_link_options(int_container2 PRIVATE -fsanitize=address)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

add_executable(int_container3 int_container3.cpp)
target_link_libraries(int_container3 PRIVATE rapidcheck)

target_compile_options(int_container3 PRIVATE -Wall -Wextra -Wpedantic)

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
  target_compile_options(int_container3 PRIVATE -fsanitize=address)
  target_link_options(int_container3 PRIVATE -fsanitize=address)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

add_executable(benchmarks benchmarks.cpp)
target_link_libraries(benchmarks PRIVATE rapidcheck)

target_include_directories(benchmarks PRIVATE ${GMP_INCLUDE_DIRS})
target_link_directories(benchmarks PRIVATE ${GMP_LIBRARY_DIRS})
target_link_libraries(benchmarks PRIVATE ${GMP_LIBRARIES})
target_compile_options(benchmarks PRIVATE ${GMP_CFLAGS_OTHER})
target_link_options(benchmarks PRIVATE ${GMP_LDFLAGS_OTHER})

target_include_directories(benchmarks PRIVATE ${CLN_INCLUDE_DIRS})
target_link_directories(benchmarks PRIVATE ${CLN_LIBRARY_DIRS})
target_link_libraries(benchmarks PRIVATE ${CLN_LIBRARIES})
target_compile_options(benchmarks PRIVATE ${CLN_CFLAGS_OTHER})
target_link_options(benchmarks PRIVATE ${CLN_LDFLAGS_OTHER})

target_link_libraries(benchmarks PRIVATE benchmark::benchmark)

