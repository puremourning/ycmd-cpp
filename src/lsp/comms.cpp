#include <boost/asio/awaitable.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/process/async_pipe.hpp>
#include <charconv>
#include <iterator>
#include <nlohmann/json.hpp>
#include <boost/log/core.hpp>

#include "lsp.hpp"

#define LOG BOOST_LOG_TRIVIAL

namespace lsp
{
  asio::experimental::coro<json> read_message(
    boost::process::async_pipe& pipe )
  {
    asio::streambuf buf;
    buf.prepare( 4096 );

    for(;;)
    {
      // Read the content-length header (as it is the only one that matters)
      size_t content_length = 0;
      for( ;; )
      {
        LOG(debug) << "Reading header line... "
                   << std::endl;

        // NOTE(Ben): This may make 0 read calls if the get buffer already
        // contains the delimiter.
        auto bytes_read = co_await asio::async_read_until(
          pipe,
          buf,
          '\n',
          asio::experimental::use_coro );

        LOG(debug) << "Read "
                   << bytes_read
                   << " bytes from the stream"
                   << std::endl;

        // -1 because we don't care about the \n
        std::string line{ asio::buffers_begin( buf.data() ),
                          asio::buffers_begin( buf.data() ) + bytes_read - 1 };

        LOG(debug) << "Header line..."
                   << line
                   << std::endl;

        buf.consume( bytes_read );

        // string any \r. The spec says all lines are terminated with \r\n but it
        // makes sense to just handle \n as well.
        while ( line.length() > 0 && line[ line.length() - 1 ] == '\r' )
          line.resize( line.length() - 1 );

        if ( line.empty() )
        {
          // We reached the end of headers
          LOG(debug) << "End of headers!" << std::endl;
          break;
        }

        auto colon = line.find( ':' );
        if ( colon != std::string::npos )
        {
          std::string header( line.data(), colon );
          std::transform( header.begin(),
                          header.end(),
                          header.begin(),
                          []( auto c ){ return std::tolower(c); } );

          LOG(debug) << "Header..."
                     << header
                     << std::endl;

          if ( header != "content-length" )
          {
            // this header is not interesting
            continue;
          }

          // skip whitespace
          colon++;
          while( colon < line.length() && std::isspace( line[ colon ] ) )
            ++colon;

          if ( colon >= line.length() )
            break;

          std::string_view value{ line.data() + colon, line.length() - colon };

          LOG(debug) << "Value..."
                     << value
                     << std::endl;

          {
            auto [ _, ec ] = std::from_chars( value.data(),
                                              value.data() + value.length(),
                                              content_length );

            if ( ec != std::errc() )
            {
              break;
            }

            LOG(debug) << "ContentLength..."
                       << content_length
                       << std::endl;

          }
        }
      }

      LOG(debug) << "About to read "
                 << content_length
                 << " bytes of message data with buffer size "
                 << buf.size()
                 << std::endl;

      if ( content_length == 0 )
      {
        // probably an error, try and listen for the next recognisable message.
        // This also helps from testing because we always get a \n after a full
        // message
        continue;
      }

      buf.prepare( content_length );
      while (buf.size() < content_length )
      {
        co_await asio::async_read(
          pipe,
          buf,
          asio::transfer_exactly(content_length - buf.size()),
          asio::experimental::use_coro );
      }

      std::string message{
        asio::buffers_begin( buf.data() ),
        asio::buffers_begin( buf.data() ) + content_length };

      LOG(debug) << "Read a message (buffer size="
                 << buf.size()
                 << ", actual msg length="
                 << message.length()
                 << "): "
                 << message
                 << std::endl;

      buf.consume( content_length );

      co_yield json::parse( message );
    }
  }

  asio::awaitable<void> send_message(
    Pipe& out,
    json message )
  {
    static asio::streambuf buf;

    std::string data = message.dump();
    buf.prepare( data.length() + 50 );

    std::ostream os(&buf);
    os << "Content-Length: "
       << data.length()
       << "\r\n\r\n"
       << data;

    LOG(debug) << "TX: Content-Length: "
               << data.length()
               << "\r\n\r\n"
               << data;

    co_await asio::async_write( out, buf, asio::use_awaitable );
  }
}
