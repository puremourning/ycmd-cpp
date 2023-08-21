#pragma once

#include "ycmd.hpp"

#include "completers/general/identifier_completer.cpp"
#include "completers/general/filename_completer.cpp"
#include "completers/general/ultisnips_completer.cpp"
#include "completers/cpp/clangd_completer.cpp"
#include <boost/asio/io_context.hpp>
#include <exception>
#include <optional>
#include <stdexcept>

namespace ycmd::server
{
  struct server {
    asio::io_context ctx;
    json user_options;

    enum class SemanticCompleterKind
    {
      CLANGD,
      JEDI,
      NONE
    };

    template< typename Request >
    SemanticCompleterKind completer_for_request(
      const RequestWrapper<Request> &req )
    {
      if ( req.first_filetype() == "cpp" )
      {
        return SemanticCompleterKind::CLANGD;
      }

      if ( req.first_filetype() == "python" )
      {
        return SemanticCompleterKind::JEDI;
      }

      return SemanticCompleterKind::NONE;
    }

    completers::general::IdentifierCompleter identifier_completer{user_options};
    completers::general::FilenameCompleter filename_completer;
    completers::general::UltiSnipsCompleter ultisnips_completer;
    std::optional<completers::cpp::ClangdCompleter> clangd_completer;

    static server& get()
    {
      static server instance;
      return instance;
    }

    void initialize( json user_options )
    {
      this->user_options = std::move( user_options );
    }

  private:
    server() = default;
    server( const server& ) = delete;
    server( server&& ) = delete;
    server& operator=( const server& ) = delete;
    server&& operator=( server&& ) = delete;
  };
}
