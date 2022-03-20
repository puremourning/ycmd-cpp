add_executable( ycmd )

# set( SOURCES script.cpp source_location.cpp index.cpp db.cpp )

target_sources( ycmd
  PRIVATE
    ycmd.cpp
    # ${SOURCES}
)

# This enables unity build (ish), by pretending that these source files are
# header files
# set_source_files_properties(
#   ${SOURCES}
#   PROPERTIES
#     HEADER_FILE_ONLY ON
# )

if ( MSVC )
  target_compile_options( ycmd
    PRIVATE
      /W4 /WX
  )
else()
  target_compile_options( ycmd
    PRIVATE
      -Wall -Wextra -Werror
  )
endif()

target_link_libraries( ycmd
  PRIVATE
    Boost::boost
    Boost::log
)

