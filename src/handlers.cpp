#include "core/Candidate.h"
#include "core/IdentifierCompleter.h"
#include "core/Repository.h"
#include "core/Result.h"
#include "ycmd.h"
#include "identifier_utils.h"
#include "api.h"
#include "request_wrap.cpp"
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

#define HANDLER_LIST \
  HANDLER( get,  healthy ) \
  HANDLER( get,  ready ) \
  HANDLER( post, shutdown ) \
  HANDLER( post, completions ) \
  HANDLER( post, event_notification ) \
  HANDLER( post, filter_and_sort_candidates ) \
  HANDLER( post, semantic_completion_available ) \
  HANDLER( get,  signature_help_available ) \
  HANDLER( post, defined_subcommands ) \
  HANDLER( post, detailed_diagnostic ) \
  HANDLER( post, debug_info ) \
  HANDLER( post, receive_messages ) \
  HANDLER( post, semantic_tokens ) \
  HANDLER( post, inlay_hints )

#define HANDLER( handler_verb, handler_name )  \
  Result handle_##handler_name( const Request& req );
  HANDLER_LIST
#undef HANDLER

  std::unordered_map<Target,Handler,boost::hash<Target>> HANDLERS = {
#define HANDLER( handler_verb, handler_name ) \
    { { http::verb::handler_verb, \
        "/" # handler_name }, \
        handle_ ## handler_name },
    HANDLER_LIST
#undef HANDLER
  };

  Result handle_healthy( const Request& req )
  {
    boost::ignore_unused( req );
    co_return api::json_response( true );
  }

  Result handle_ready( const Request& req )
  {
    boost::ignore_unused( req );
    co_return api::json_response( true );
  }

  Result handle_shutdown( const Request& req )
  {
    boost::ignore_unused( req );
    throw ShutdownResult( api::json_response( true ) );
  }

  Result handle_filter_and_sort_candidates( const Request& req )
  {
    // FIXME: Unfortuantely, this is really inefficent. It does _tons_ of
    // copies. It might actually be better to use the json _directly_ rather
    // than try and parse it into a struct with a variant. Or alternatively,
    // just store a json ref in the request struct.
    auto [ request_data, _ ] =
      api::json_request<requests::FilterAndSortCandidatesRequest>( req );

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

      return api::json_response( filtered_candidates );
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

    co_return api::json_response(false);
  }

  // TODO: Move
  YouCompleteMe::IdentifierCompleter identifier_completer;

  Result handle_event_notification( const Request& req )
  {
    auto [ request_data, j ] = api::json_request<requests::EventNotification>(
      req );
    LOG(debug) << "Event name: " << j.at( "event_name" );

    const auto& file = request_data.file_data[ request_data.filepath ];

    using enum requests::EventNotification::Event;
    switch ( request_data.event_name )
    {
      case FileReadyToParse:
      {
        identifier_completer.ClearForFileAndAddIdentifiersToDatabase(
          IdentifiersFromBuffer( file ),
          file.filetypes[ 0 ],
          request_data.filepath.string() );
        break;
      }
    }

    co_return api::json_response( json::object() );
  }

  Result handle_completions( const Request& req )
  {
    auto request_wrap = ycmd::make_request_wrap( req );
    // TODO settings[ min_num_of_chars_for_completion ]
    if ( request_wrap.query().length() <
          server::user_options[ "min_num_of_chars_for_completion" ] )
    {
      co_return api::json_response( json::array() );
    }

    auto completions = identifier_completer.CandidatesForQueryAndType(
          request_wrap.query(),
          request_wrap.first_filetype() );

    std::vector<api::Candidate> candidates;
    for ( auto& completion_sring : completions ) {
      // TODO settings[ min_num_identifier_candidate_chars ]
      if ( completion_sring.length() >
            server::user_options[ "min_num_identifier_candidate_chars" ] )
      {
        candidates.push_back( api::Candidate{
          .insertion_text = completion_sring,
          .extra_menu_info = "[ID]",
        } );
      }
    }
    responses::CompletionsResponse response {
      .completions = std::move( candidates ),
        // TODO: switch to byte offset
      .completion_start_column = (int)request_wrap.start_codepoint()
    };
    co_return api::json_response( response );
  }

  Result handle_semantic_completion_available( const Request& req )
  {
    co_return api::json_response( false );
  }

  Result handle_signature_help_available( const Request& req )
  {
    co_return api::json_response( false );
  }

  Result handle_defined_subcommands( const Request& req )
  {
    co_return api::json_response( json::array() );
  }

  Result handle_detailed_diagnostic( const Request& req )
  {
    co_return api::json_response( "" );
  }

  Result handle_debug_info( const Request& req )
  {
    co_return api::json_response( json::object() );
  }

  Result handle_receive_messages( const Request& req )
  {
    co_return api::json_response( false );
  }

  Result handle_semantic_tokens( const Request& req )
  {
    co_return api::json_response( responses::SemanticTokensResponse{} );
  }

  Result handle_inlay_hints( const Request& req )
  {
    co_return api::json_response( responses::InlayHintsResponse{} );
  }
}
