#include "ycmd.h"
#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/functional/hash.hpp>
#include <boost/core/ignore_unused.hpp>

#include <boost/beast/http/verb.hpp>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace ycmd::handlers {
  namespace http = boost::beast::http;

  Response handle_healthy( const Request& req );
  Response handle_ready( const Request& req );
  Response handle_get_completions( const Request& req );

  std::unordered_map<Target,Handler,boost::hash<Target>> HANDLERS = {
    { { http::verb::get,  "/healthy" },          handle_healthy },
    { { http::verb::get,  "/ready" },            handle_ready },

    { { http::verb::post, "/get_completions" }, handle_get_completions },
  };

  /**
   * Return a HTTP OK with the supplied JSON payload
   */
  Response json_response( const json& j )
  {
    Response rep;
    rep.result(http::status::ok);
    rep.body() = j.dump();
    rep.prepare_payload();
    return rep;
  }

  /**
   * Parse a HTTP request into a struct.
   *
   * @param TRequest type to parse into
   */
  template<typename TRequest>
  TRequest json_requst( const Request& req )
  {
    return json::parse( req.body() ).get<TRequest>();
  }

  Response handle_healthy( const Request& req )
  {
    boost::ignore_unused( req );
    return json_response( true );
  }

  Response handle_ready( const Request& req )
  {
    boost::ignore_unused( req );
    return json_response( true );
  }

  Response handle_get_completions( const Request& req )
  {
    Response rep;
    auto req_body = json::parse(req.body());
    auto rep_body = json::object();

    if ( req_body.contains( "test" ) )
    {
      rep_body[ "foo" ] = "bar";
      rep.result(http::status::ok);
      rep.body() = rep_body.dump();
      rep.prepare_payload();
    }
    else
    {
      rep.result(http::status::bad_request);
      rep.reason("Missing 'test'");
    }

    return rep;
  }
}
