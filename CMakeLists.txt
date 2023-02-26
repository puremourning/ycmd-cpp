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
  DESCRIPTION "A code comprehension server"
  LANGUAGES CXX
)

include( CTest )
include( FetchContent )

function(ycmd_target_compiler_setup target)
  if ( MSVC )
    target_compile_options( ${target}
      PRIVATE
        /W4 /WX
    )
  else()
    target_compile_options( ${target}
      PRIVATE
        -Wall -Wextra -Werror -fvisibility=hidden
    )
    if ( ${ENABLE_ASAN} )
      target_compile_options( ${target}
        PRIVATE
         -fsanitize=address
        )
      target_link_options( ${target}
        PRIVATE
          -fsanitize=address
      )
    endif()
  endif()
endfunction()

function(ycmd_target_setup target)
  ycmd_target_compiler_setup( ${target} )
  target_link_libraries( ${target}
    PRIVATE
      YcmCore
      Boost::boost
      Boost::log
      Boost::regex
      Boost::stacktrace
      absl::flags
      absl::flags_parse
      nlohmann_json::nlohmann_json
      ztd::text
      pybind11::pybind11
      pybind11::embed
  )
endfunction()

FetchContent_Declare(ztd.text
  GIT_REPOSITORY https://github.com/soasis/text.git
  GIT_SHALLOW    ON
  GIT_TAG        main)
FetchContent_MakeAvailable( ztd.text )

# All of this project uses c++17 and we certainly don't want random extensions
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

option( ENABLE_ASAN "Build with address sanitizer" ON )

# Always generate compilation database
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

find_package(Boost REQUIRED
  COMPONENTS
    beast
    filesystem
    process
    regex
)
find_package( absl REQUIRED
  COMPONENTS
    absl::flags
    absl::flags_parse
)
find_package(nlohmann_json REQUIRED)
find_package(pybind11 REQUIRED
  COMPONENTS
    pybind11::pybind11
    pybind11::embed
)

# add the executable
add_subdirectory(src)
