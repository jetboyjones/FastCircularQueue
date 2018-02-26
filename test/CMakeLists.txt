include_directories("${PROJECT_SOURCE_DIR}")

# First Simple Google Test
add_executable(FunctionTest FunctionTest.cpp)
add_executable(UnitTest UnitTest.cpp)

target_link_libraries(UnitTest gtest gmock_main)

# GTest Unit Tests
add_test(NAME unit_test COMMAND UnitTest)

# Non-GTest functional test
add_test(NAME functional_test COMMAND FunctionTest)
