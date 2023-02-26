add_executable( test_request_wrap test_request_wrap.cpp )

ycmd_target_setup( test_request_wrap )

find_package( gtest REQUIRED )

target_link_libraries( test_request_wrap
  PRIVATE
    gtest::gtest
)

add_test(
  NAME test_request_wrap
  COMMAND test_request_wrap
)

add_executable( test_regex test_regex.cpp )
target_link_libraries( test_regex
  PRIVATE
    Boost::regex
)
