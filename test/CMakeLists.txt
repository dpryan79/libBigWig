set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LOCAL_TEST_TARGETS "exampleWrite;testBigBed;testIterator;testLocal;testWrite")

set(REMOTE_TEST_TARGETS "testRemote;testRemoteManyContigs")

if (WITH_CURL)
  set(TEST_TARGETS "${LOCAL_TEST_TARGETS};${REMOTE_TEST_TARGETS}")
else()
  set(TEST_TARGETS "${LOCAL_TEST_TARGETS}")
endif()

set(LIBBIGWIG_COMPILER_WARNINGS -Wall -Wsign-compare)

foreach(TEST_TARGET ${TEST_TARGETS})
  add_executable(${TEST_TARGET} ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_TARGET}.c)
  target_link_libraries(${TEST_TARGET} PUBLIC ${CONAN_LIBS}
                                              libBigWig::libbigwig)
  target_compile_features(${TEST_TARGET} PRIVATE c_std_${CMAKE_C_STANDARD})
  target_compile_options(${TEST_TARGET} PRIVATE ${LIBBIGWIG_COMPILER_WARNINGS})
endforeach()
