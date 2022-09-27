#pragma once

#include <nlohmann/json.hpp>
#include "core/IdentifierCompleter.h"

#include "ycmd.h"

#include "completers/general/filename_completer.cpp"
#include "completers/general/ultisnips_completer.cpp"
#include "completers/cpp/clangd_completer.cpp"

namespace ycmd {
  namespace server {
    json user_options;
    YouCompleteMe::IdentifierCompleter identifier_completer;

    completers::general::FilenameCompleter filename_compelter;
    completers::general::UltiSnipsCompleter ultisnips_compelter;

    completers::cpp::ClangdCompleter clangd_completer;
  }
}

