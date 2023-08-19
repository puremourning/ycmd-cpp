#pragma once

#include "ycmd.hpp"

#include "completers/general/identifier_completer.cpp"
#include "completers/general/filename_completer.cpp"
#include "completers/general/ultisnips_completer.cpp"
#include "completers/cpp/clangd_completer.cpp"
#include <boost/asio/io_context.hpp>

namespace ycmd::server
{
  struct server {
    asio::io_context* globbal_ctx{nullptr};
    json user_options;

    completers::general::IdentifierCompleter identifier_completer{user_options};
    completers::general::FilenameCompleter filename_completer;
    completers::general::UltiSnipsCompleter ultisnips_completer;
    std::optional<completers::cpp::ClangdCompleter> clangd_completer;
  };
}
