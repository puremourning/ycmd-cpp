#include <boost/asio/awaitable.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/system_context.hpp>
#include <boost/asio/this_coro.hpp>
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
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

#include "core/Candidate.h"
#include "core/IdentifierCompleter.h"
#include "core/Repository.h"
#include "core/Result.h"

#include "ycmd.hpp"
#include "api.hpp"
#include "request_wrap.cpp"
#include "server.cpp"

namespace ycmd::handlers {
  namespace http = boost::beast::http;
  using namespace boost::asio::experimental::awaitable_operators;

#define HANDLER_LIST \
  HANDLER( get,  healthy ) \
  HANDLER( get,  ready ) \
  HANDLER( post, shutdown ) \
  HANDLER( post, completions ) \
  HANDLER( post, event_notification ) \
  HANDLER( post, run_completer_command ) \
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
  Result handle_##handler_name( server::server& server, const Request& req );
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

  Result handle_healthy( server::server& server, const Request& req )
  {
    boost::ignore_unused( req );
    co_return api::json_response( true );
  }

  Result handle_ready( server::server& server, const Request& req )
  {
    boost::ignore_unused( req );
    co_return api::json_response( true );
  }

  Result handle_shutdown( server::server& server, const Request& req )
  {
    boost::ignore_unused( req );
    throw ShutdownResult( api::json_response( true ) );
  }

  Result handle_filter_and_sort_candidates( server::server& server, const Request& req )
  {
    auto [ request_data, _ ] =
      api::json_request<requests::FilterAndSortCandidatesRequest>( req );

    std::vector<std::string> strings;
    strings.reserve( request_data.candidates.size() );
    for( const auto& c : request_data.candidates )
    {
      strings.push_back( c[ request_data.sort_property ] );
    }

    namespace ycm = YouCompleteMe;
    auto repository_candidates =
      ycm::Repository<ycm::Candidate>::Instance().GetElements(
        std::move( strings ) );

    std::vector< ycm::ResultAnd< size_t > > result_and_objects;
    ycm::Word query_object( std::move( request_data.query ) );

    for ( size_t i = 0; i < repository_candidates.size(); ++i )
    {
      const ycm::Candidate *candidate = repository_candidates[ i ];

      if ( candidate->IsEmpty() || !candidate->ContainsBytes( query_object ) )
      {
        continue;
      }

      ycm::Result result = candidate->QueryMatchResult( query_object );

      if ( result.IsSubsequence() )
      {
        result_and_objects.emplace_back( result, i );
      }
    }
    constexpr auto MAX_CANDIDATES = 0;
    ycm::PartialSort( result_and_objects, MAX_CANDIDATES );

    std::vector<json> filtered_candidates;
    filtered_candidates.reserve( result_and_objects.size() );
    for ( const auto& r : result_and_objects )
    {
      filtered_candidates.push_back(
        request_data.candidates[ r.extra_object_ ] );
    }

    co_return api::json_response( filtered_candidates );
  }

  Result handle_event_notification( server::server& server, const Request& req )
  {
    auto request_wrap = make_request_wrap<requests::EventNotification>(req);

    LOG(debug) << "Event name: " << request_wrap.raw_req.at( "event_name" );

    if ( request_wrap.first_filetype() == "cpp" )
    {
      if ( !server.clangd_completer.has_value() )
      {
        server.clangd_completer.emplace( server.user_options, server.ctx );
        co_await server.clangd_completer->init( request_wrap );
      }
    }

    // Do these "concurrently"
    co_await (
      server.identifier_completer.handle_event_notification( request_wrap ) &&
      server.filename_completer.handle_event_notification( request_wrap.req ) &&
      server.clangd_completer->handle_event_notification( request_wrap )
    );

    co_return api::json_response( json::object() );
  }

  Result handle_completions( server::server& server, const Request& req )
  {
    auto request_wrap = ycmd::make_request_wrap( req );
    if ( request_wrap.query().length() <
          server.user_options[ "min_num_of_chars_for_completion" ] )
    {
      co_return api::json_response( json::array() );
    }

    const auto& ft = request_wrap.first_filetype();

    std::vector<api::Candidate> candidates;

    if ( ft == "cpp" && server.clangd_completer.has_value() )
    {
      candidates = co_await server.clangd_completer->compute_candiatdes(
        request_wrap );
    }
    else
    {
      candidates = co_await server.identifier_completer.compute_candiatdes(
        request_wrap );
    }


    responses::CompletionsResponse response {
      .completions = std::move( candidates ),
      .completion_start_column = (int)request_wrap.start_column()
    };
    co_return api::json_response( response );
  }

  Result handle_run_completer_command( server::server& server, const Request& req )
  {
    co_return api::json_response( nullptr );
  }

  Result handle_semantic_completion_available( server::server& server, const Request& req )
  {
    co_return api::json_response( false );
  }

  Result handle_signature_help_available( server::server& server, const Request& req )
  {
    co_return api::json_response( false );
  }

  Result handle_defined_subcommands( server::server& server, const Request& req )
  {
    co_return api::json_response( json::array() );
  }

  Result handle_detailed_diagnostic( server::server& server, const Request& req )
  {
    co_return api::json_response( "" );
  }

  Result handle_debug_info( server::server& server, const Request& req )
  {
    auto request_wrap = ycmd::make_request_wrap( req );

    py::module_ sys = py::module_::import( "sys" );

    responses::DebugInfoResponse response{
      .python{
        .executable = py::str( sys.attr( "executable" ) ),
        .version{ py::str( sys.attr( "version" ) ) }
      }
    };

    co_return api::json_response( response );
  }

  Result handle_receive_messages( server::server& server, const Request& req )
  {
    // co_wait get_messages_for_compelter
    co_return api::json_response( false );
  }

  Result handle_semantic_tokens( server::server& server, const Request& req )
  {
    co_return api::json_response( responses::SemanticTokensResponse{} );
  }

  Result handle_inlay_hints( server::server& server, const Request& req )
  {
    co_return api::json_response( responses::InlayHintsResponse{} );
  }
}
