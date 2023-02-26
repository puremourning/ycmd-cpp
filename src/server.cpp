#pragma once

#include "ycmd.hpp"

#include "completers/general/identifier_completer.cpp"
#include "completers/general/filename_completer.cpp"
#include "completers/general/ultisnips_completer.cpp"
#include "completers/cpp/clangd_completer.cpp"

namespace ycmd::server
{
  completers::general::IdentifierCompleter identifier_completer;
  completers::general::FilenameCompleter filename_completer;
  completers::general::UltiSnipsCompleter ultisnips_completer;
  completers::cpp::ClangdCompleter clangd_completer;
}
