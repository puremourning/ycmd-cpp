add_subdirectory( core )
add_subdirectory( lsp )
add_executable( ycmd )

set( SOURCES
  ycmd.hpp
  api.hpp
  identifier_utils.cpp
  handlers.cpp
  request_wrap.cpp
  server.cpp

  meowhash.h

  completers/general/identifier_completer.cpp
  completers/general/filename_completer.cpp
  completers/general/ultisnips_completer.cpp
  completers/cpp/clangd_completer.cpp
)

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
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories( ycmd SYSTEM
  PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/meowhash/include
)

ycmd_target_setup( ycmd )

install( TARGETS ycmd DESTINATION bin )

add_subdirectory( test )
