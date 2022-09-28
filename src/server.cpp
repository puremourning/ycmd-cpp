#pragma once

#include "ycmd.h"

#include "completers/general/identifier_completer.cpp"
#include "completers/general/filename_completer.cpp"
#include "completers/general/ultisnips_completer.cpp"
#include "completers/cpp/clangd_completer.cpp"

namespace ycmd::server
{
  completers::general::IdentifierCompleter identifier_completer;
  completers::general::FilenameCompleter filename_compelter;
  completers::general::UltiSnipsCompleter ultisnips_compelter;
  completers::cpp::ClangdCompleter clangd_completer;
}
