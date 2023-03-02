add_library( YcmLsp )
target_sources( YcmLsp
  PRIVATE
    lsp.hpp
    lsp_types.hpp
    comms.cpp
)
ycmd_target_compiler_setup( YcmLsp )

target_include_directories( YcmLsp
  PRIVATE
    ../
)

target_link_libraries( YcmLsp
  PRIVATE
    Boost::boost
    Boost::log
    nlohmann_json::nlohmann_json
)
