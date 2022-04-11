#include "core/Candidate.h"
#include "core/IdentifierCompleter.h"
#include "core/Repository.h"

#include "core/Result.h"
#include "ycmd.h"
#include "api.h"
#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/functional/hash.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/log/trivial.hpp>
#include <boost/regex.hpp>

#include <boost/beast/http/verb.hpp>
#include <boost/regex/v5/match_flags.hpp>
#include <boost/regex/v5/regex_fwd.hpp>
#include <boost/regex/v5/regex_iterator.hpp>
#include <boost/regex/v5/regex_search.hpp>
#include <functional>
#include <iterator>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

namespace ycmd::handlers {
  namespace http = boost::beast::http;

  Result handle_healthy( const Request& req );
  Result handle_ready( const Request& req );
  Result handle_shutdown( const Request& req );
  Result handle_completions( const Request& req );
  Result handle_event_notification( const Request& req );
  Result handle_filter_and_sort_candidates( const Request& req );

  std::unordered_map<Target,Handler,boost::hash<Target>> HANDLERS = {
    { { http::verb::get,  "/healthy" },         handle_healthy },
    { { http::verb::get,  "/ready" },           handle_ready },
    { { http::verb::post, "/shutdown" },        handle_shutdown },

    { { http::verb::post, "/event_notification" }, handle_event_notification },
    { { http::verb::post, "/completions" },     handle_completions },
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

  Result handle_healthy( const Request& req )
  {
    boost::ignore_unused( req );
    co_return json_response( true );
  }

  Result handle_ready( const Request& req )
  {
    boost::ignore_unused( req );
    co_return json_response( true );
  }

  Result handle_shutdown( const Request& req )
  {
    boost::ignore_unused( req );
    throw ShutdownResult( json_response( true ) );
  }

  Result handle_filter_and_sort_candidates( const Request& req )
  {
    // FIXME: Unfortuantely, this is really inefficent. It does _tons_ of
    // copies. It might actually be better to use the json _directly_ rather
    // than try and parse it into a struct with a variant. Or alternatively,
    // just store a json ref in the request struct.
    auto [ request_data, _ ] =
      json_request<requests::FilterAndSortCandidatesRequest>( req );

    auto GetResults = [&, &request_data=request_data](
      const auto& candidates ) {

      using T = std::remove_cvref_t<decltype(candidates)>;

      std::vector<std::string> strings;
      strings.reserve( candidates.size() );
      for( const auto& c : candidates ) {
        if constexpr ( std::is_same_v< typename T::value_type, std::string > ) {
          strings.push_back( c );
        } else if constexpr ( std::is_same_v< typename T::value_type, json > ) {
          strings.push_back( c.at( request_data.sort_property.value_or(
                "insertion_text" ) ) );
        } else {
          strings.push_back( c.insertion_text );
        }
      }

      using namespace YouCompleteMe;
      auto repository_candidates =
        Repository<Candidate>::Instance().GetElements( std::move(strings) );

      std::vector< ResultAnd< size_t > > result_and_objects;
      Word query_object( std::move( request_data.query ) );

      for ( size_t i = 0; i < candidates.size(); ++i ) {
        const Candidate *candidate = repository_candidates[ i ];

        if ( candidate->IsEmpty() ||
             !candidate->ContainsBytes( query_object ) ) {
          continue;
        }

        YouCompleteMe::Result result = candidate->QueryMatchResult(
          query_object );

        if ( result.IsSubsequence() ) {
          result_and_objects.emplace_back( result, i );
        }
      }
      constexpr auto MAX_CANDIDATES = 0;
      PartialSort( result_and_objects, MAX_CANDIDATES );

      // Now we have a sorted/filtered list of YCM-land Candidates paired with
      // their index in the input. We return a vector of the original input
      // indices matching.
      std::remove_cvref_t<decltype(candidates)> filtered_candidates;

      filtered_candidates.reserve( result_and_objects.size() );
      for ( const auto& r : result_and_objects ) {
        filtered_candidates.push_back(
          candidates[ r.extra_object_ ] );
      }

      return json_response( filtered_candidates );
    };

    switch( request_data.candidate_type )
    {
    case requests::FilterAndSortCandidatesRequest::CandidateType::CANDIDATES:
      co_return GetResults( std::get<std::vector<api::Candidate>>(
          request_data.candidates ) );
    case requests::FilterAndSortCandidatesRequest::CandidateType::STRINGS:
      co_return GetResults( std::get<std::vector<std::string>>(
          request_data.candidates ) );
    case requests::FilterAndSortCandidatesRequest::CandidateType::UNKNOWN:
      co_return GetResults( std::get<std::vector<json>>(
          request_data.candidates ) );
    }

    co_return json_response(false);
  }

  // TODO: Move
  YouCompleteMe::IdentifierCompleter c;

  boost::regex DEFUALT_IDENTIFIER_REGEX{ R"([^\W\d]\w*)" };

  std::unordered_map<std::string, boost::regex> FILETYPE_TO_IDENTIFIER_REGEX {
  };

  Result handle_event_notification( const Request& req ) 
  {
    auto [ request_data, j ] = json_request<requests::EventNotification>( req );
    LOG(debug) << "Event name: " << j.at( "event_name" );

    const auto& file = request_data.file_data[ request_data.file_path ];

    using enum requests::EventNotification::Event;
    switch ( request_data.event_name )
    {
      case FileReadyToParse:
      {
        boost::smatch results;
        boost::sregex_iterator b( file.contents.begin(),
                                  file.contents.end(),
                                  DEFUALT_IDENTIFIER_REGEX );
        boost::sregex_iterator e;

        // FIXME: sigh... more sad copying
        std::vector<std::string> candidates;
        std::for_each( b, e, [&]( const auto& match ) {
          candidates.push_back( match.str() );
        } );

        // TODO: extract the identifiers using regex
        c.ClearForFileAndAddIdentifiersToDatabase(
          candidates,
          file.filetypes[ 0 ],
          request_data.file_path.string() );
        break;
      }
    }

    co_return json_response(nullptr);
  }

  Result handle_completions( const Request& req )
  {
    auto [ request_data, _ ] = json_request<api::SimpleRequest>( req );
    co_return Response();
  }
}
