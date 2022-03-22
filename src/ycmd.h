#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using json = nlohmann::json;
using tcp = asio::ip::tcp;

#define LOG BOOST_LOG_TRIVIAL

namespace ycmd
{
  using Request = http::request<http::string_body>;
  using Response = http::response<http::string_body>;
  using Target = std::pair<http::verb,std::string_view>;
  using Handler = std::function<Response( const Request& )>;
}
