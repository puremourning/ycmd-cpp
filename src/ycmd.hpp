#pragma once

#include <pybind11/pybind11.h>

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

using tcp = asio::ip::tcp;

#define LOG BOOST_LOG_TRIVIAL

#include "json/json_serialisation.hpp"

namespace ycmd
{
  namespace server {
    struct server;
  }

  using RequestParser = http::request_parser<http::string_body>;
  using Request = http::request<http::string_body>;
  using Response = http::response<http::string_body>;
  using Result = asio::awaitable<Response>;
  using Target = std::pair<http::verb,std::string_view>;
  using Handler = std::function<Result( server::server&, const Request& )>;

  template<typename T>
  using Async = asio::awaitable<T>;

  struct ShutdownResult : std::exception {
    ShutdownResult( Response response_ )
      : response( std::move(response_) )
    {
    }

    Response response;
  };

  struct Shutdown: std::exception {};

  namespace server {
    inline json user_options;
  }
  using namespace std::literals;

  namespace py = pybind11;
}
