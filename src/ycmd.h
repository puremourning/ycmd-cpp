#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/parser.hpp>
#include <exception>
#include <nlohmann/json.hpp>

#include <boost/log/trivial.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using json = nlohmann::json;
using tcp = asio::ip::tcp;

// Support for optional<T> in json
namespace nlohmann
{
  template<typename T>
  struct adl_serializer< std::optional<T> >
  {
    static void to_json( json& j, const std::optional<T>& o )
    {
      if ( o.has_value() )
      {
        j = *o;
      }
    }

    static void from_json( const json& j, std::optional<T>& o )
    {
      // In our definition optional<T> means may or may not be supplied. If
      // supplied, must be one of the valid types (null is only valid where
      // specified). Point is, that if we're going _from_ a json value, then
      // the json value _must exist_ therefore its conversion is trivial
      o = j.get<T>();
    }
  };
}

#define LOG BOOST_LOG_TRIVIAL

namespace ycmd
{
  using RequestParser = http::request_parser<http::string_body>;
  using Request = http::request<http::string_body>;
  using Response = http::response<http::string_body>;
  using Result = asio::awaitable<Response>;
  using Target = std::pair<http::verb,std::string_view>;
  using Handler = std::function<Result( const Request& )>;

  struct ShutdownResult : std::exception {
    ShutdownResult( Response response_ )
      : response( std::move(response_) )
    {
    }

    Response response;
  };

  namespace server {
    json user_options;
  }

  struct Shutdown: std::exception {};

  using namespace std::literals;
}
