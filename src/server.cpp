#pragma once

#include "ycmd.hpp"

#include "completers/general/identifier_completer.cpp"
#include "completers/general/filename_completer.cpp"
#include "completers/general/ultisnips_completer.cpp"
#include "completers/cpp/clangd_completer.cpp"
#include <boost/asio/io_context.hpp>
#include <exception>
#include <stdexcept>

namespace ycmd::server
{
  struct server {
    asio::io_context ctx;
    json user_options;

    completers::general::IdentifierCompleter identifier_completer{user_options};
    completers::general::FilenameCompleter filename_completer;
    completers::general::UltiSnipsCompleter ultisnips_completer;
    std::optional<completers::cpp::ClangdCompleter> clangd_completer;

    server( const json& user_options )
      : user_options{user_options}
    {
      if ( the_instance )
      {
        throw std::runtime_error( "ycmd server already initialized" );
      }
      the_instance = this;
    }

    ~server()
    {
      the_instance = nullptr;
    }

    static server& instance()
    {
      if ( !the_instance )
      {
        throw std::runtime_error( "ycmd server not initialized" );
      }
      return *the_instance;
    }

  private:
    inline static server* the_instance;
  };
}
