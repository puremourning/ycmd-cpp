# This is boilerplate to use the latest policies available
# Info:
# https://cliutils.gitlab.io/modern-cmake/chapters/basics.html#minimum-version
cmake_minimum_required(VERSION 3.7...3.19)
if ( ${CMAKE_VERSION} VERSION_LESS 3.12 )
    cmake_policy( VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} )
endif()
# End.

project( ycmd
  VERSION 2.0.0
  DESCRIPTION "A code comrehension server"
  LANGUAGES CXX
)

# All of this project uses c++17 and we certainly don't want random extensions
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED True )

# Always generate compilation database
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

find_package(Boost REQUIRED
  COMPONENTS
    beast
    filesystem
    process
)

# Controlling the flags for debug/release is per-config, not per-target (and
# necessarily per-compiler)
if ( NOT MSVC )
  set( CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer" )
  set( CMAKE_CXX_FLAGS_RELEASE "-O2" )
  set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2" )
endif()

# add the executable
add_subdirectory(src)
