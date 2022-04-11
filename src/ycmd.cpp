#include "ycmd.h"
#include "handlers.cpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/co_spawn.hpp>

#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core/error.hpp>

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/log/trivial.hpp>

#include <boost/system/detail/errc.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>
#include <system_error>

namespace ycmd::server
{
  void server_abort( boost::system::error_code ec )
  {
    LOG(fatal) << ec.message();
    abort();
  }

  template< typename... Ts >
  void handle_unexpected_exception( std::exception_ptr ep, Ts&&... )
  {
    if ( ep )
    {
      try
      {
        std::rethrow_exception( ep );
      }
      catch ( const std::exception& e )
      {
        LOG(fatal) << "Unhandled exception! " << e.what();
        abort();
      }
    }
    // otherwise, ignore the result Ts...
  }

  asio::awaitable<void> handle_session( tcp::acceptor& acceptor,
                                        tcp::socket socket )
  {
    auto stream = beast::tcp_stream( std::move( socket ) );
    beast::flat_buffer buffer;

    Request req;
    try
    {
      co_await http::async_read( stream,
                                 buffer,
                                 req,
                                 asio::use_awaitable );

      auto handler = handlers::HANDLERS.find(
        { req.method(), { req.target().data(), req.target().length() } } );

      Response response;
      bool do_shutdown = false;
      if ( handler == handlers::HANDLERS.end() )
      {
        LOG(info) << "No handler for "
                  << req.method()
                  << " "
                  << req.target();

        response.result(http::status::not_found);
      }
      else
      {
        LOG(info) << "Handling request "
                  << req.method()
                  << " "
                  << req.target();

        try {
          response = co_await handler->second( req );
        } catch ( const ShutdownResult& s ) {
          response = std::move( s.response );
          do_shutdown = true;
        }
      }

      co_await http::async_write( stream,
                                  response,
                                  asio::use_awaitable );

      if ( do_shutdown ) {
        acceptor.cancel();
      }
    }
    catch( const boost::system::system_error &e )
    {
      LOG(info) << "Got an error: " << e.code() << " = " << e.what();
    }
  }

  asio::awaitable<void> listen( tcp::acceptor& acceptor )
  {
    for (;;)
    {
      try {
        asio::co_spawn(
          acceptor.get_executor(),
          handle_session(
            acceptor,
            co_await acceptor.async_accept( asio::use_awaitable ) ),
          handle_unexpected_exception<> );
      } catch ( boost::system::system_error& ec ) {
        if ( ec.code() == boost::system::errc::operation_canceled ) {
          LOG(info) << "Closing socket on request";
          break;
        }
      }
    }
  }
}

int main()
{
  asio::io_context ctx;
  tcp::acceptor acceptor( ctx, { tcp::v4(), 1337 } );
  asio::co_spawn( ctx,
                  ycmd::server::listen( acceptor ),
                  ycmd::server::handle_unexpected_exception<> );

  // TODO: spin up a handful of threads to handle stuff too
  ctx.run();
}
