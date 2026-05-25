cmake_minimum_required(VERSION 3.14)
project(http-server VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler warnings ────────────────────────────────────────────────────────
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Sources ──────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(http-server ${SOURCES})
target_include_directories(http-server PRIVATE include)

# ── Threading ────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)
target_link_libraries(http-server PRIVATE Threads::Threads)

# ── Optional: Release optimisations ─────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(http-server PRIVATE -O2)
endif()

# ── Install ──────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS http-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
