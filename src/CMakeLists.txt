add_subdirectory( core )
add_executable( ycmd )

set( SOURCES handlers.cpp api.h )

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
endif()

target_link_libraries( ycmd
  PRIVATE
    YcmCore
    Boost::boost
    Boost::log
    nlohmann_json::nlohmann_json
)

