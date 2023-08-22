find_package( GTest REQUIRED )

list( APPEND YCMD_TESTS
  test_request_wrap
  test_identifier_utils
  test_json_serialisation
)

function( add_ycmd_test test_name )
  add_executable( ${test_name} ${test_name}.cpp )
  ycmd_target_setup( ${test_name} )
  target_link_libraries( ${test_name}
    PRIVATE
      gtest::gtest
  )
  add_test(
    NAME ${test_name}
    COMMAND ${test_name}
  )
endfunction()

foreach( test_name IN LISTS YCMD_TESTS )
  add_ycmd_test( ${test_name} )
endforeach()

# This is a manual, special case
add_executable( test_regex test_regex.cpp )
target_link_libraries( test_regex
  PRIVATE
    Boost::regex
)
