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
#include <boost/beast/core.hpp>
#include <boost/beast/core/error.hpp>

#include <boost/log/trivial.hpp>

#include <boost/system/detail/error_code.hpp>
#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

#define LOG BOOST_LOG_TRIVIAL

namespace ycmd::server
{
  void server_abort( boost::system::error_code ec )
  {
    LOG(fatal) << ec.message();
    abort();
  }

  asio::awaitable<void> listen( tcp::endpoint endpoint )
  {
    beast::error_code ec;
    tcp::acceptor acceptor( co_await asio::this_coro::executor );
    if ( acceptor.open( endpoint.protocol(), ec ) ||
         acceptor.set_option( asio::socket_base::reuse_address( true ), ec ) ||
         acceptor.listen( asio::socket_base::max_listen_connections, ec ) )
    {
      co_return server_abort( ec );
    }

    for( ;; )
    {
      tcp::socket socket( co_await asio::this_coro::executor );
      
    }
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
      }
    }
  }
}

int main()
{
  asio::io_context ctx;
  tcp::endpoint listen( asio::ip::address_v4(), 1337 );

  asio::co_spawn( ctx,
                  ycmd::server::listen( std::move( listen ) ),
                  ycmd::server::handle_unexpected_exception<> );

}
