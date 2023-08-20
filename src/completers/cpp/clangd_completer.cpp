#pragma once

#include <algorithm>
#include <boost/asio/async_result.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/concept_check.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/process/detail/child_decl.hpp>
#include <boost/process/io.hpp>
#include <boost/process/search_path.hpp>
#include <deque>
#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <function2/function2.hpp>
#include <boost/process.hpp>

#include "api.hpp"
#include "lsp/lsp_types.hpp"
#include "request_wrap.cpp"
#include "lsp/lsp.hpp"
#include "server.cpp"
#include "util.hpp"
#include "ycmd.hpp"

#include <xxhash.h>

namespace ycmd::completers::cpp {
  namespace process = boost::process;

  std::string URI( std::string_view file_path )
  {
    return "file://" + std::string( file_path );
  }

  std::string URI( std::filesystem::path file_path )
  {
    return URI( std::string_view{ file_path.string() } );
  }

  struct ClangdCompleter
  {
    process::child clangd;
    process::async_pipe server_stdout;
    process::async_pipe server_stdin;

    lsp::number next_id = 0;

    bool initialised = false;

    struct PendingRequest
    {
      lsp::ID id;
      fu2::unique_function<void(json)> handler;
    };

    std::deque<PendingRequest> pending_requests;

    const json& user_options;

    ClangdCompleter( json& user_options, asio::io_context& ctx )
      : server_stdout( ctx )
      , server_stdin( ctx )
      , user_options( user_options )
    {
    }

    template< typename RequestType >
    Async<bool> init( const RequestWrapper<RequestType>& req )
    {
      boost::ignore_unused( req );

      // Launch clangd
      // Start the LSP connection on its channels
      // co_await initialize response

      boost::filesystem::path clangd_path;
      if ( user_options.contains( "clangd_binary_path" ) )
      {
        // TODO: There must be a better way to do this without all the copying
        // Check filesystem and json docs for good ways to work with these
        // things
        user_options[ "clangd_binary_path" ].get_to( clangd_path );
      }
      else
      {
        clangd_path = process::search_path( "clangd" );
      }

      if ( clangd_path.size() == 0 )
      {
        LOG(warning) << "Unable to find clangd" << std::endl;
        co_return 0;
      }

      LOG(debug) << "Found clangd at: " << clangd_path;
      clangd = process::child( clangd_path,
                               "-log=verbose",
                               "-j=4",
                               "--query-driver=/**/*",
                               "-header-insertion-decorators=0",
                               "-limit-results=500",
                               process::std_out > server_stdout,
                               process::std_in < server_stdin,
                               process::std_err > stderr );


      asio::co_spawn( co_await asio::this_coro::executor,
                      message_pump(),
                      asio::detached );

      lsp::ResponseMessage<lsp::InitializeResult> response =
        co_await get_response<lsp::InitializeResult>(
          "initialize",
          lsp::InitializeParams{
            .processId { getpid() },
            .rootUri{"/foo"},
            .capabilities{
              .workspace{
                .applyEdit = false,
              },
              .textDocument{
                .completion{
                  .snippetSupport = false,
                },
              },
            }
          } );
      LOG(debug) << "Got a freaking response to init: " << response;
      if (response.error.has_value())
      {
        LOG(warning) << "clangd initialize failed: "
                     << *response.error;
      }
      else if (!response.result.has_value())
      {
        LOG(warning) << "clangd initialize invalid: "
                     << response;
      }
      else
      {
        LOG(trace) << "clangd got capabilities! "
                   << response.result->capabilities;
        initialised = true;
      }

      co_return initialised;
    }

    Async<void> handle_event_notification(
      const RequestWrapper<requests::EventNotification>& request_data )
    {
      using enum requests::EventNotification::Event;
      switch ( request_data.req.event_name )
      {
        case FileReadyToParse:
        {
          if ( initialised )
          {
            co_await sync_files( request_data );
          }
          break;
        }
        case FileSave:
          break;
        case BufferVisit:
          break;
        case BufferUnload:
          break;
        case InsertLeave:
          break;
        case CurrentIdentifierFinished:
          break;
      }

      co_return;
    }
    template<typename ResultType, typename Payload>
    Async<lsp::ResponseMessage<ResultType>> get_response(
      std::string_view method,
      Payload&& payload )
    {
      json responseMessage =
        co_await asio::async_initiate< decltype(asio::use_awaitable),
                                       void(json) >(
          [
            this,
            method,
            payload=std::move(payload)
          ]( auto&& handler, const auto& executor )
          {
            auto& server_stdin = this->server_stdin;
            auto& entry = pending_requests.emplace_back( PendingRequest{
              .id = next_id++,
              .handler = std::move(handler)
            });
            asio::co_spawn( executor,
                            lsp::send_request( server_stdin,
                                               entry.id,
                                               method,
                                               std::move(payload) ),
                            asio::detached );
          },
          asio::use_awaitable,
          co_await asio::this_coro::executor
        );

      co_return
        responseMessage.template get<lsp::ResponseMessage<ResultType>>();
    }

    Async<void> message_pump()
    {
      LOG(debug) << "Starting LSP message pump..." << std::endl;

      auto generator = lsp::read_message( server_stdout );
      while( true ) // TODO: Something something cancellation is hard.
      {
        auto message = co_await generator.async_resume( asio::use_awaitable );
        if (!message.has_value())
        {
          LOG(trace) << "Got no message, bailing: ";
          break;
        }
        LOG(debug) << "Got a message: " << *message;
        if (message->contains("method"))
        {
          // notification or request
          if (message->contains("id"))
          {
            // reverse-request
            // TODO
          }
          else
          {
            // notification
            // TODO
          }
        }
        else
        {
          // response
          auto id = message->at("id");
          auto pos = std::find_if( pending_requests.begin(),
                                   pending_requests.end(),
                                   [&]( const auto& r ) {
                                     return r.id == id;
                                   } );
          if ( pos == pending_requests.end() )
          {
            LOG(debug) << "Unexpected response to non-message " << *message;
            continue;
          }

          auto handler = std::move(pos->handler);
          pending_requests.erase(pos);

          handler( std::move(*message) );
        }
      }
    }

    struct File
    {
      lsp::integer version;
      uint64_t hash;
    };

    std::unordered_map<std::string,File> opened_files;

    template<typename SimpleRequest>
    Async<void> sync_files(
      const ycmd::RequestWrapper<SimpleRequest>& request_wrap )
    {
      for ( const auto& [ filename, filedata ] : request_wrap.req.file_data )
      {
        auto hash = XXH64( filedata.contents.data(),
                           filedata.contents.size(),
                           0 );

        auto pos = opened_files.find( filename );
        if ( pos == opened_files.end() )
        {
          opened_files.emplace( filename,
                                File{ .version = 1, .hash = hash } );

          // add file
          //
          co_await lsp::send_notification(
            server_stdin,
            "textDocument/didOpen",
            lsp::DidOpenTextDocumentParams {
              .textDocument{
                .uri = URI(filename),
                .languageId = filedata.filetypes[ 0 ],
                .version = 1,
                .text = filedata.contents,
              }
            } );

        }
        else if ( pos->second.hash != hash )
        {
          pos->second.hash = hash;
          ++pos->second.version;
          // update file
          co_await lsp::send_notification(
            server_stdin,
            "textDocument/didChange",
            lsp::DidChangeTextDocumentParams {
              .textDocument{
                { URI( filename ) },
                pos->second.version
              },
              .contentChanges{
                lsp::DidChangeTextDocumentParams::TextDocumentChangeEvent{
                  .text = filedata.contents
                }
              }
            } );
        }
      }

      for (auto iter = opened_files.begin(); iter != opened_files.end(); )
      {
        const auto& filename = iter->first;
        if ( !request_wrap.req.file_data.contains( filename ) )
        {
          // delete
          co_await lsp::send_notification(
            server_stdin,
            "textDocument/didClose",
            lsp::DidCloseTextDocumentParams{
              .textDocument{ filename }
            } );
          iter = opened_files.erase( iter );
        }
        else
        {
          ++iter;
        }
      }

      co_return;
    }

    using Candidates = std::vector<ycmd::api::Candidate>;
    Async<Candidates> compute_candiatdes(
      const ycmd::RequestWrap& request_wrap )
    {
      if (!initialised)
      {
        co_return Candidates{};
      }

      co_await sync_files( request_wrap );

      lsp::ResponseMessage<lsp::CompletionsResponse> response = co_await get_response<lsp::CompletionsResponse>(
        "textDocument/completion",
        lsp::CompletionParams{
          lsp::TextDocumentPositionParams{
            .textDocument{
              .uri = URI(request_wrap.req.filepath)
            },
            .position{
              .line = (uint64_t)request_wrap.req.line_num - 1,
              .character = request_wrap.column_codepoint() - 1,
            },
          },
          std::nullopt,
        } );

      if ( response.error.has_value() || !response.result.has_value() )
      {
        co_return Candidates{};
      }

      lsp::CompletionItems& items = std::visit(
        util::visitor{
          []( lsp::CompletionList& list ) -> lsp::CompletionItems& {
            return list.items;
          },
          []( lsp::CompletionItems& items ) -> lsp::CompletionItems& {
            return items;
          },
        },
        response.result.value() );

      Candidates candidates;
      for (const auto& item : items)
      {
        using enum lsp::CompletionItem::CompletionItemKind;
        auto candidate = ycmd::api::Candidate{
          .menu_text = item.label,
          .detailed_info = item.detail.value_or(""),
          .kind = item.kind == Text ? "text" :
            item.kind == Method ? "method" :
            item.kind == Function ? "function" :
            item.kind == Constructor ? "constructor" :
            item.kind == Field ? "field" :
            item.kind == Variable ? "variable" :
            item.kind == Class ? "class" :
            item.kind == Interface ? "interface" :
            item.kind == Module ? "module" :
            item.kind == Property ? "property" :
            item.kind == Unit ? "unit" :
            item.kind == Value ? "value" :
            item.kind == Enum ? "enum" :
            item.kind == Keyword ? "keyword" :
            item.kind == Snippet ? "snippet" :
            item.kind == Color ? "color" :
            item.kind == File ? "file" :
            item.kind == Reference ? "reference" :
            item.kind == Folder ? "folder" :
            item.kind == EnumMember ? "enum-member" :
            item.kind == Constant ? "constant" :
            item.kind == Struct ? "struct" :
            item.kind == Event ? "event" :
            item.kind == Operator ? "operator" :
            item.kind == TypeParameter ? "type-parameter" :
            "unknown"
        };
        if ( item.documentation.has_value() )
        {
          std::visit( util::visitor{
            [&]( const std::string& str ) {
              candidate.detailed_info = str;
            },
            [&]( const lsp::MarkupContent& markup ) {
              candidate.detailed_info = markup.value;
            },
          }, item.documentation.value() );
        }

        if ( item.textEdit.has_value() )
        {
          candidate.insertion_text = item.textEdit->newText;
        }
        else if ( item.insertText.has_value() )
        {
          candidate.insertion_text = item.insertText.value();
        }
        else
        {
          candidate.insertion_text = item.label;
        }

        candidates.push_back( std::move(candidate) );
      }
      co_return candidates;
    }
  };
}
