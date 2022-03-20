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

#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/log/trivial.hpp>

#include <boost/system/detail/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <iostream>
#include <system_error>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = asio::ip::tcp;

#define LOG BOOST_LOG_TRIVIAL

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

  asio::awaitable<void> handle_session( tcp::socket socket )
  {
    auto stream = beast::tcp_stream( std::move( socket ) );
    beast::flat_buffer buffer;

    for ( ;; )
    {
      http::request<http::string_body> req;
      try
      {
        co_await http::async_read( stream,
                                   buffer,
                                   req,
                                   asio::use_awaitable );

        LOG(info) << "Get a request:\n"
                  << req.method()
                  << " "
                  << req.target();

      }
      catch( const boost::system::system_error &e )
      {
        LOG(info) << "Got an error: " << e.code() << " = " << e.what();
        break;
      }
    }
  }

  asio::awaitable<void> listen( tcp::acceptor& acceptor )
  {
    for( ;; )
    {
      asio::co_spawn( acceptor.get_executor(),
                      handle_session(
                        co_await acceptor.async_accept( asio::use_awaitable ) ),
                      handle_unexpected_exception<> );
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

  ctx.run();
}
