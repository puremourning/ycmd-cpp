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
#include <vector>
#include <function2/function2.hpp>
#include <boost/process.hpp>

#include "api.hpp"
#include "lsp/lsp_types.hpp"
#include "request_wrap.cpp"
#include "lsp/lsp.hpp"

namespace ycmd::completers::cpp {
  namespace bp = boost::process;

  struct ClangdCompleter
  {
    bp::child clangd;
    bp::async_pipe server_stdout;
    bp::async_pipe server_stdin;

    size_t next_id = 0;

    bool initialised = false;

    struct PendingRequest
    {
      std::string id;
      fu2::unique_function<void(lsp::ResponseMessage<>)> handler;
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
      //auto clangd_path = bp::search_path( "clangd" );
      auto clangd_path = "/opt/homebrew/opt/llvm/bin/clangd"s;
      LOG(debug) << "Found clangd at: " << clangd_path;
      clangd = bp::child( clangd_path,
                          "-log=verbose",
                          "-j=4",
                          "--query-driver=/**/*",
                          "-header-insertion-decorators=0",
                          "-limit-results=500",
                          bp::std_out > server_stdout,
                          bp::std_in < server_stdin,
                          bp::std_err > stderr );


      asio::co_spawn( co_await asio::this_coro::executor,
                      message_pump(),
                      asio::detached );

      auto response = co_await get_response( "initialize",
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

      initialised = clangd.running();
      co_return clangd.running();
    }

    template<typename Payload>
    Async<lsp::ResponseMessage<>> get_response( std::string_view method,
                                                Payload&& payload )
    {
      co_return co_await asio::async_initiate<decltype(asio::use_awaitable),
                                                    void(lsp::ResponseMessage<>)>(
        [
          this,
          method,
          payload=std::move(payload)
        ]( auto&& handler, const auto& executor )
        {
          auto& server_stdin = this->server_stdin;
          auto id = std::to_string(next_id++);
          pending_requests.emplace_back( PendingRequest{
            .id = id,
            .handler = std::move(handler)
          });
          asio::co_spawn( executor,
                          [
                            &server_stdin,
                            method,
                            payload=std::move(payload),
                            id=std::move(id)
                          ]() -> Async<void>
                          {
                            LOG(debug) << "Sending a message " << id;
                            co_await lsp::send_request( server_stdin,
                                                        std::move(id),
                                                        method,
                                                        std::move(payload) );
                          },
                          asio::detached);
        },
        asio::use_awaitable,
        co_await asio::this_coro::executor
       );

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
          }
          else
          {
            // notification
          }
        }
        else
        {
          // response
          auto response = message->get<lsp::ResponseMessage<>>();
          auto pos = std::find_if( pending_requests.begin(),
                                   pending_requests.end(),
                                   [&]( const auto& r ) {
                                     return r.id == response.id;
                                   } );
          if ( pos == pending_requests.end() )
          {
            LOG(debug) << "Unexpected response to non-message " << response;
            continue;
          }

          auto handler = std::move(pos->handler);
          pending_requests.erase(pos);

          handler( std::move(response) );
        }
      }
    }

    using Candidates = std::vector<ycmd::api::Candidate>;
    Async<Candidates> compute_candiatdes(
      const ycmd::RequestWrap& request_wrap )
    {
      Candidates ret;
      co_return ret;
    }
  };
}
