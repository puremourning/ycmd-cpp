# Copyright (C) 2011-2021 ycmd contributors
#
# This file is part of YouCompleteMe.
#
# YouCompleteMe is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# YouCompleteMe is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with YouCompleteMe.  If not, see <http://www.gnu.org/licenses/>.

# CYGWIN needs handholding...
if( CYGWIN )
  set( NEEDS_EXTENSIONS ON )
else()
  set( NEEDS_EXTENSIONS OFF )
endif()


# Check if we need to add -lstdc++fs or -lc++fs or nothing
file( WRITE ${CMAKE_CURRENT_BINARY_DIR}/main.cpp "#include <filesystem>\nint main( int argc, char ** argv ) {\n  std::filesystem::path p( argv[ 0 ] );\n  return p.string().length();\n}" )
try_compile( STD_FS_NO_LIB_NEEDED ${CMAKE_CURRENT_BINARY_DIR}
             SOURCES ${CMAKE_CURRENT_BINARY_DIR}/main.cpp
             CXX_STANDARD 17
             CXX_STANDARD_REQUIRED TRUE
             CXX_EXTENSIONS ${NEEDS_EXTENSIONS} )
try_compile( STD_FS_NEEDS_STDCXXFS ${CMAKE_CURRENT_BINARY_DIR}
             SOURCES ${CMAKE_CURRENT_BINARY_DIR}/main.cpp
             CXX_STANDARD 17
             CXX_STANDARD_REQUIRED TRUE
             CXX_EXTENSIONS ${NEEDS_EXTENSIONS}
             LINK_LIBRARIES stdc++fs )
try_compile( STD_FS_NEEDS_CXXFS ${CMAKE_CURRENT_BINARY_DIR}
             SOURCES ${CMAKE_CURRENT_BINARY_DIR}/main.cpp
             CXX_STANDARD 17
             CXX_STANDARD_REQUIRED TRUE
             CXX_EXTENSIONS ${NEEDS_EXTENSIONS}
             LINK_LIBRARIES c++fs )
file( REMOVE ${CMAKE_CURRENT_BINARY_DIR}/main.cpp )

if( ${STD_FS_NEEDS_STDCXXFS} )
  set( STD_FS_LIB stdc++fs )
elseif( ${STD_FS_NEEDS_CXXFS} )
  set( STD_FS_LIB c++fs )
elseif( ${STD_FS_NO_LIB_NEEDED} )
  set( STD_FS_LIB "" )
else()
  message( FATAL_ERROR "Unknown compiler - C++17 filesystem library missing" )
endif()

find_package( absl REQUIRED
  COMPONENTS
    absl::flat_hash_map
    absl::flat_hash_set
)

#############################################################################

file( GLOB CORE_SOURCES *.h *.cpp )
add_library( YcmCore )

target_sources( YcmCore
  PRIVATE
    ${CORE_SOURCES}
)
target_compile_definitions( YcmCore
  PUBLIC
    YCM_ABSEIL_SUPPORTED
    YCM_EXPORT=
)

if ( MSVC )
  target_compile_options( YcmCore
    PRIVATE
      /W4 /WX
  )
else()
  target_compile_options( YcmCore
    PRIVATE
      -Wall -Wextra -Werror -fvisibility=hidden
  )
endif()

target_link_libraries( YcmCore
  PUBLIC
    absl::flat_hash_map
    absl::flat_hash_set
)

