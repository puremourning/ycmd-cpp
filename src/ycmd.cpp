// third party

#include <absl/flags/internal/flag.h>
#include <algorithm>
#include <boost/any.hpp>
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

#include "boost/url/url.hpp"
#include "boost/url/url_view.hpp"

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

#include <boost/core/ignore_unused.hpp>
#include <boost/log/trivial.hpp>

#include <boost/stacktrace.hpp>

#include <boost/stacktrace/stacktrace_fwd.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <pybind11/eval.h>
#include <string>
#include <sys/signal.h>
#include <system_error>
#include <sys/ptrace.h>

#include <absl/flags/usage.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/str_split.h>
#include <unistd.h>

#include <boost/url.hpp>

#include <pybind11/embed.h>

// first party only below here. Note the following sequence should be used
// everywhere

#include "ycmd.hpp"
#include "api.hpp"
#include "identifier_utils.cpp"
#include "request_wrap.cpp"

#include "server.cpp"
#include "handlers.cpp"

// tcp server depends on handlers
namespace ycmd::server
{
  bool check_hmac( Request& req )
  {
    boost::ignore_unused( req );
    return true;
  }

  void write_hmac( Response& resp )
  {
    boost::ignore_unused( resp );
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
        LOG(fatal) << "Unhandled exception! "
                   << e.what()
                   << boost::stacktrace::stacktrace();
        abort();
      }
    }
    // otherwise, ignore the result Ts...
  }

  asio::awaitable<void> handle_session( ycmd::server::server& server,
                                        tcp::acceptor& acceptor,
                                        tcp::socket socket )
  {
    auto stream = beast::tcp_stream( std::move( socket ) );
    beast::flat_buffer buffer;

    RequestParser parser;
    parser.body_limit( std::numeric_limits< uint64_t >::max() );
    try
    {
      co_await http::async_read( stream,
                                 buffer,
                                 parser,
                                 asio::use_awaitable );

      Request &req = parser.get();

      if ( !check_hmac(req) )
      {
        co_return;
      }

      auto url = boost::urls::url_view( req.target() );
      auto handler = handlers::HANDLERS.find( { req.method(), url.path() } );

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
          response = co_await handler->second( server, req );
        } catch ( const ShutdownResult& s ) {
          response = std::move( s.response );
          do_shutdown = true;
        } catch ( const boost::system::system_error& ec ) {
          response.result(http::status::internal_server_error);
          response.body() = json( responses::Error{
            .exception = ec.what(),
            .message = ec.code().message(),
          }.set_traceback( boost::stacktrace::stacktrace() ) );
          response.prepare_payload();
        } catch ( const std::exception& e ) {
          // unexpected exception!
          response.result(http::status::internal_server_error);
          response.body() = json( responses::Error{
            .exception = typeid(e).name(),
            .message = e.what(),
          }.set_traceback( boost::stacktrace::stacktrace() ) );
          response.prepare_payload();
        }
      }

      write_hmac(response);

      LOG(info) << "Result: " << response;

      co_await http::async_write( stream,
                                  response,
                                  asio::use_awaitable );

      if ( do_shutdown )
      {
        acceptor.cancel();
      }
    }
    catch( const boost::system::system_error &e )
    {
      LOG(info) << "Got an error: " << e.code() << " = " << e.what();
    }
  }

  asio::awaitable<void> listen( ycmd::server::server& server,
                                tcp::acceptor& acceptor )
  {
    for (;;)
    {
      try {
        asio::co_spawn(
          acceptor.get_executor(),
          handle_session(
            server,
            acceptor,
            co_await acceptor.async_accept( asio::use_awaitable ) ),
          handle_unexpected_exception<> );
      } catch ( boost::system::system_error& ec ) {
        if ( ec.code() == boost::system::errc::operation_canceled )
        {
          LOG(info) << "Closing socket on request";
          break;
        }
      }
    }
  }

  std::optional<json> read_options( std::string_view options_file_name )
  {
    // TODO: What if this faile? Thros and exception?
    std::ifstream infile( std::string{options_file_name} );
    std::optional<json> user_options;
    try {
      user_options.emplace( json::parse( infile ) );
    } catch (const json::exception &e) {
      LOG(warning) << "Failed to parse " << options_file_name << ": " << e.what();
      return std::nullopt;
    }

    LOG(info) << "Loaded options from file " << user_options;

    return user_options;
  }
}

ABSL_FLAG( uint16_t, port, 1337, "Port to listen on" );
ABSL_FLAG( std::optional<std::string>, out, std::nullopt, "Output log file" );
ABSL_FLAG( std::optional<std::string>, err, std::nullopt, "Error log file" );
ABSL_FLAG( bool, wait_for_debugger, false, "Wait in a loop until attach" );
ABSL_FLAG( std::optional<std::string>,
           options_file,
           std::nullopt, "Default options" );

static void crash_handler(int signum)
{
  signal(signum, SIG_DFL);
  // FIXME: This is not async signal safe!
  auto signame = strsignal( signum );
  std::cerr << "Received deadly signal ";
  if ( signame )
  {
    std::cerr << signame;
  }
  else
  {
    std::cerr << signum;
  }
  std::cerr << "\nTraceback:\n"
            << boost::stacktrace::stacktrace();
  raise(signum);
}

static void interupt_handler(int signum)
{
  signal(signum, SIG_DFL);
  std::cerr << "Received signal " << strsignal(signum) << "\n";
  raise(signum);
}

int main( int argc, char **argv )
{
  signal( SIGABRT, &crash_handler );
  signal( SIGSEGV, &crash_handler );
  signal( SIGBUS, &crash_handler );
  signal( SIGINT, &interupt_handler );

  absl::SetProgramUsageMessage("A code comprehension server");
  absl::ParseCommandLine(argc, argv);

  if ( const auto& flag = absl::GetFlag( FLAGS_out ); flag.has_value() )
  {
    std::freopen( flag.value().c_str(), "w", stdout );
    // TODO: check the result
  }
  if ( const auto& flag = absl::GetFlag( FLAGS_err ); flag.has_value() )
  {
    std::freopen( flag.value().c_str(), "w", stderr );
    // TODO: check the result
  }

  std::optional<json> user_options;
  if ( const auto& flag = absl::GetFlag( FLAGS_options_file );
       !flag.has_value() )
  {
    // TODO: change this ? do we really need this to be mandatory
    std::cerr << "Must supply --options_file" << std::endl;
    return 1;
  }
  else if ( user_options = ycmd::server::read_options( flag.value() );
            !user_options.has_value() )
  {
    return 2;
  }

  if ( absl::GetFlag( FLAGS_wait_for_debugger ) ||
       getenv( "YCMD_WAIT_FOR_DEBUGGER" ) )
  {
    std::cout << "Waiting for debugger (set should_continue=true)..."
              << std::endl;
    volatile bool should_continue = false;
    while ( !should_continue )
    {
      sleep( 100 );
    }
  }

  LOG(info) << "ycmd starting...";

  {
    ycmd::py::scoped_interpreter interp{};

    // TODO: This doesn't print to stdout reopened above. Indeed if specifying
    // --out foo, it just doesn't get printed. Which is kind of odd.
    ycmd::py::exec( R"(
      import sys
      print( "YCMD PYTHON VERSION: " + sys.version )
    )");

    auto& server = ycmd::server::server::get();
    server.initialize( std::move( user_options.value() ) );

    tcp::acceptor acceptor( server.ctx, { tcp::v4(), absl::GetFlag( FLAGS_port ) } );
    asio::co_spawn( server.ctx,
                    ycmd::server::listen( server, acceptor ),
                    ycmd::server::handle_unexpected_exception<> );

    // TODO: spin up a handful of threads to handle stuff too
    server.ctx.run();
  }
}
