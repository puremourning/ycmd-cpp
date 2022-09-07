add_subdirectory( core )
add_executable( ycmd )

set( SOURCES ycmd.h api.h identifier_utils.cpp handlers.cpp request_wrap.cpp )

target_sources( ycmd
  PRIVATE
    ycmd.cpp
    ${SOURCES}
)

# This enables unity build (ish), by pretending that these source files are
# header files
set_source_files_properties(
  ${SOURCES}
  PROPERTIES
    HEADER_FILE_ONLY ON
)

target_include_directories( ycmd
  PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/boost-url/include
)

if ( MSVC )
  target_compile_options( ycmd
    PRIVATE
      /W4 /WX
  )
else()
  target_compile_options( ycmd
    PRIVATE
      -Wall -Wextra -Werror -fvisibility=hidden
  )
  if ( ${ENABLE_ASAN} )
    target_compile_options( ycmd
      PRIVATE
       -fsanitize=address
      )
    target_link_options( ycmd
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

target_link_libraries( ycmd
  PRIVATE
    YcmCore
    Boost::boost
    Boost::log
    Boost::regex
    Boost::stacktrace
    absl::flags
    absl::flags_parse
    nlohmann_json::nlohmann_json
)

install( TARGETS ycmd DESTINATION bin )

add_subdirectory( test )

