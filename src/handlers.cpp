#include "core/Candidate.h"
#include "core/Repository.h"

#include "core/Result.h"
#include "ycmd.h"
#include "api.h"
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
#include <vector>
#include <ranges>
#include <algorithm>

namespace ycmd::handlers {
  namespace http = boost::beast::http;

  Response handle_healthy( const Request& req );
  Response handle_ready( const Request& req );
  Response handle_get_completions( const Request& req );
  Response handle_filter_and_sort_candidates( const Request& req );

  std::unordered_map<Target,Handler,boost::hash<Target>> HANDLERS = {
    { { http::verb::get,  "/healthy" },          handle_healthy },
    { { http::verb::get,  "/ready" },            handle_ready },

    { { http::verb::post, "/get_completions" },  handle_get_completions },
    { { http::verb::post, "/filter_and_sort_candidates" },
      handle_filter_and_sort_candidates },
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
  std::pair<TRequest, json> json_request( const Request& req )
  {
    auto j = json::parse( req.body() );
    return { j.get<TRequest>(), j };
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

  Response handle_filter_and_sort_candidates( const Request& req )
  {
    auto [ request_data, _ ] =
      json_request<requests::FilterAndSortCandidatesRequest>( req );

#if 0
    auto sort_property = request_data.sort_property.value_or(
      "insertion_text" );

    if ( sort_property == "insertion_text" ) {
    } else {
      return YouCompleteMe::Repository<YouCompleteMe::Candidate>::Instance().GetElements(
        request_data.candidates );
    }
#endif

    std::vector<std::string> strings;
    strings.reserve( request_data.candidates.size() );
    for( const auto& c : request_data.candidates ) {
      strings.push_back( c.insertion_text );
    }

    using namespace YouCompleteMe;
    auto repository_candidates = Repository<Candidate>::Instance().GetElements(
      std::move(strings) );

    std::vector< ResultAnd< size_t > > result_and_objects;
    Word query_object( std::move( request_data.query ) );

    for ( size_t i = 0; i < request_data.candidates.size(); ++i ) {
      const Candidate *candidate = repository_candidates[ i ];

      if ( candidate->IsEmpty() || !candidate->ContainsBytes( query_object ) ) {
        continue;
      }

      Result result = candidate->QueryMatchResult( query_object );

      if ( result.IsSubsequence() ) {
        result_and_objects.emplace_back( result, i );
      }
    }
    constexpr auto MAX_CANDIDATES = 0;
    PartialSort( result_and_objects, MAX_CANDIDATES );

    // Now we have a sorted/filtered list of YCM-land Candidates paired with
    // their index in the input. We return a vector of the original input
    // indices matching.
    std::vector<api::Candidate> filtered_candidates;
    filtered_candidates.reserve( result_and_objects.size() );
    for ( const auto& r : result_and_objects ) {
      filtered_candidates.push_back(
        request_data.candidates[ r.extra_object_ ] );
    }

    return json_response( filtered_candidates );
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
