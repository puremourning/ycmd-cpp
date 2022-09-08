#pragma once

#include "ycmd.h"
#include <boost/stacktrace.hpp>
#include <boost/stacktrace/stacktrace_fwd.hpp>
#include <filesystem>
#include <nlohmann/detail/macro_scope.hpp>
#include <sstream>
#include <string>
#include <optional>
#include <type_traits>
#include <vector>
#include <nlohmann/json.hpp>
#include <variant>

#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE(Type, Base, ...)  \
     friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { nlohmann::to_json(nlohmann_json_j, static_cast<const Base&>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
     friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { nlohmann::from_json(nlohmann_json_j, static_cast<Base&>(nlohmann_json_t)); Type nlohmann_json_default_obj; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }

//#define NLOHMANN_JSON_TO(v1) nlohmann_json_j[#v1] = nlohmann_json_t.v1;

template<typename T, typename enable = void>
struct is_optional : std::false_type { };

template<typename T>
struct is_optional<std::optional<T>> : std::true_type { };

static_assert( is_optional< std::optional< std::string > >::value == true );
static_assert( is_optional< std::string >::value == false );

namespace ycmd::api {
  using LineNum = int;
  using ColumnNum = int;
  using FilePath = std::filesystem::path;

  void to_json_optional( json& j, const char* fieldName, auto&& v )
  {
    if constexpr ( is_optional< std::remove_cvref_t<decltype( v )> >::value ) {
      if ( v.has_value() ) {
        j[fieldName] = v;
      }
    } else {
      j[fieldName] = v;
    }
  }

#undef NLOHMANN_JSON_TO
#define NLOHMANN_JSON_TO(v1) \
    to_json_optional( nlohmann_json_j, #v1, nlohmann_json_t.v1 );

  struct Location {
    LineNum line_num;
    ColumnNum column_number;
    FilePath filepath;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      Location,
      line_num,
      column_number,
      filepath)
  };

  struct Range {
    Location start;
    Location end;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      Range,
      start,
      end)
  };

  struct FixIt {
    struct Chunk {
      std::string replacement_text;
      Range range;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        Chunk,
        replacement_text,
        range);
    };
    std::string text;
    Location location;
    std::optional<bool> resolve;
    std::string kind;
    std::vector<Chunk> chunks;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      FixIt,
      text,
      location,
      resolve,
      kind,
      chunks);
  };

  struct Candidate {
    std::string insertion_text;
    std::optional<std::string> menu_text;
    std::optional<std::string> extra_menu_info;
    std::optional<std::string> detailed_info;
    std::optional<std::string> kind;
    std::optional<std::string> extra_data;
    std::optional<std::string> doc_string;
    std::optional<std::vector<FixIt>> fixits;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      Candidate,
      insertion_text,
      extra_menu_info,
      detailed_info,
      kind,
      extra_data,
      doc_string,
      fixits);
  };

  struct SimpleRequest {
    LineNum line_num;
    ColumnNum column_num;
    FilePath filepath;

    struct FileData {
      std::vector<std::string> filetypes;
      std::string contents;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        FileData,
        filetypes,
        contents );
    };

    using FileDataMap = std::map<FilePath, FileData>;
    FileDataMap file_data;

    std::string completer_target;
    std::string working_directory;
    json::object_t extra_conf_data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      SimpleRequest,
      line_num,
      column_num,
      filepath,
      file_data,
      completer_target,
      working_directory,
      extra_conf_data);
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
    LOG(info) << "Request data: " << req.body();
    // TODO: What if this faile? Thros and exception?
    auto j = json::parse( req.body() );
    return { j.get<TRequest>(), j };
  }

}

namespace ycmd::requests {
  using namespace ycmd::api;

  struct FilterAndSortCandidatesRequest {
    enum class CandidateType { CANDIDATES, STRINGS, UNKNOWN } candidate_type;
    std::variant< std::vector<Candidate>,
                  std::vector<std::string>,
                  std::vector<json> > candidates;
    std::optional<std::string> sort_property;
    std::string query;
  };

  void from_json(const json& j, FilterAndSortCandidatesRequest& r) {
    r.sort_property = j.at( "sort_property" ).get<decltype(r.sort_property)>();
    r.query = j.at( "query" ).get<decltype(r.query)>();

    using enum FilterAndSortCandidatesRequest::CandidateType;

    const auto& candidates = j.at( "candidates" );

    if ( r.sort_property == "word" ) {
        r.candidates = candidates.get<std::vector<std::string>>();
        r.candidate_type = STRINGS;
    } else if ( r.sort_property == "insertion_text" ) {
        r.candidates = candidates.get<std::vector<Candidate>>();
        r.candidate_type = CANDIDATES;
    } else {
      r.candidates = candidates.get<std::vector<json>>();
      r.candidate_type = UNKNOWN;
    }
  }

  struct EventNotification : SimpleRequest
  {
    enum class Event
    {
      FileReadyToParse
    };
    Event event_name;

    // tag_files
    // syntax_keywords


    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE(
      EventNotification,
      SimpleRequest,
      event_name);
  };

  NLOHMANN_JSON_SERIALIZE_ENUM( EventNotification::Event, {
    {EventNotification::Event::FileReadyToParse, "FileReadyToParse"},
  });

}

namespace ycmd::responses {
  using namespace api;

  struct Error {
    std::string exception;
    std::string message;
    std::string traceback;

    Error& set_traceback( const boost::stacktrace::stacktrace& trace ) {
      std::ostringstream os;
      os << trace;
      traceback = os.str();
      return *this;
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      Error,
      exception,
      message,
      traceback );
  };

  struct CompletionsResponse {
    std::vector<Candidate> completions;
    ColumnNum completion_start_column;
    std::vector<Error> errors;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      CompletionsResponse,
      completions,
      completion_start_column,
      errors );
  };

  struct SemanticTokensResponse {
    struct Tokens {
      std::vector<int> tokens;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        SemanticTokensResponse::Tokens,
        tokens );
    };


    Tokens semantic_tokens;
    std::vector<Error> errors;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      SemanticTokensResponse,
      semantic_tokens,
      errors);
  };

  struct InlayHintsResponse {
    struct InlayHint {
      std::string hint;
      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        InlayHint,
        hint
      )
    };
    std::vector<InlayHint> inlay_hints;
    std::vector<Error> errors;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      InlayHintsResponse,
      inlay_hints,
      errors);
  };
}
