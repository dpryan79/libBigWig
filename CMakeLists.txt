cmake_minimum_required(VERSION 3.8)

project(libBigWig LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(WITH_CURL "Enable CURL support" ON)
option(WITH_ZLIBNG "Link to zlib-ng instead of zlib" OFF)
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(ENABLE_TESTING "Build tests" OFF)

if(WITH_ZLIBNG)
  find_package(zlib-ng REQUIRED)
else()
  find_package(ZLIB REQUIRED)
endif()

if(WITH_CURL)
  find_package(CURL REQUIRED)
endif()

add_library(BigWig)
add_library(libBigWig::libbigwig ALIAS BigWig)

target_sources(
  BigWig
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bwRead.c
          ${CMAKE_CURRENT_SOURCE_DIR}/bwStats.c
          ${CMAKE_CURRENT_SOURCE_DIR}/bwValues.c
          ${CMAKE_CURRENT_SOURCE_DIR}/bwWrite.c
          ${CMAKE_CURRENT_SOURCE_DIR}/io.c)

target_include_directories(BigWig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT WITH_CURL)
  target_compile_definitions(BigWig PUBLIC NOCURL)
endif()

target_link_libraries(
  BigWig PUBLIC $<IF:$<BOOL:${WITH_ZLIBNG}>,zlib-ng::zlib-ng,ZLIB::ZLIB>
                $<$<BOOL:${WITH_CURL}>:CURL::libcurl> m)

target_compile_features(BigWig PRIVATE c_std_${CMAKE_C_STANDARD})

set(LIBBIGWIG_COMPILER_WARNINGS -Wall -Wsign-compare)
target_compile_options(BigWig PRIVATE ${LIBBIGWIG_COMPILER_WARNINGS})

set(LIBBIGWIG_PUBLIC_HEADERS "bigWig.h;bigWigIO.h;bwCommon.h;bwValues.h")

set_target_properties(BigWig PROPERTIES PUBLIC_HEADER
                                        "${LIBBIGWIG_PUBLIC_HEADERS}")

if(ENABLE_TESTING)
  add_subdirectory(test)
endif()

include(GNUInstallDirs)
install(
  TARGETS BigWig
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libbigwig)
