#pragma once

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
#include <string_view>
#include <unordered_set>
#include <vector>
#include <function2/function2.hpp>
#include <boost/process.hpp>

#include "api.hpp"
#include "lsp/lsp_types.hpp"
#include "request_wrap.cpp"
#include "lsp/lsp.hpp"
#include "ycmd.hpp"

namespace ycmd::completers::cpp {
  namespace process = boost::process;

  struct ClangdCompleter
  {
    process::child clangd;
    process::async_pipe server_stdout;
    process::async_pipe server_stdin;

    size_t next_id = 0;

    bool initialised = false;

    struct PendingRequest
    {
      lsp::ID id;
      fu2::unique_function<void(json)> handler;
    };

    std::deque<PendingRequest> pending_requests;

    ClangdCompleter( asio::io_context& ctx )
      : server_stdout( ctx )
      , server_stdin( ctx )
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
      if ( server::user_options.contains( "clangd_binary_path" ) )
      {
        // TODO: There must be a better way to do this without all the copying
        // Check filesystem and json docs for good ways to work with these
        // things
        server::user_options[ "clangd_binary_path" ].get_to( clangd_path );
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

      // TODO: I really really want to specify the payload type here. The
      // challenge is that the PendingRequest would need to type-erase the
      // template argument to the stored callback even further. Maybe that's
      // possible, though i don't fully know.
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
              .id = (double)next_id++,
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

    std::unordered_set<std::string> opened_files;

    Async<void> sync_files( const ycmd::RequestWrap& request_wrap )
    {

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

      co_return Candidates{};
    }
  };
}
