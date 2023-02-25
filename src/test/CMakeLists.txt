add_executable( test_request_wrap test_request_wrap.cpp )

if ( MSVC )
  target_compile_options( test_request_wrap
    PRIVATE
      /W4 /WX
  )
else()
  target_compile_options( test_request_wrap
    PRIVATE
      -Wall -Wextra -Werror -fvisibility=hidden
  )
  if ( ${ENABLE_ASAN} )
    target_compile_options( test_request_wrap
      PRIVATE
       -fsanitize=address
      )
    target_link_options( test_request_wrap
      PRIVATE
        -fsanitize=address
    )
  endif()
endif()

find_package( absl REQUIRED
  COMPONENTS
    absl::flags
    absl::flags_parse
)

find_package( gtest REQUIRED )

target_link_libraries( test_request_wrap
  PRIVATE
    YcmCore
    Boost::boost
    Boost::log
    Boost::regex
    Boost::stacktrace
    absl::flags
    absl::flags_parse
    nlohmann_json::nlohmann_json
    gtest::gtest
    ztd::text
)

add_test(
  NAME test_request_wrap
  COMMAND test_request_wrap
)

