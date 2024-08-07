#pragma once

#include "ycmd.hpp"
#include "json/json_serialisation.hpp"
#include <boost/stacktrace.hpp>
#include <boost/stacktrace/stacktrace_fwd.hpp>
#include <filesystem>
#include <nlohmann/detail/conversions/from_json.hpp>
#include <nlohmann/detail/conversions/to_json.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <sstream>
#include <string>
#include <optional>
#include <type_traits>
#include <vector>
#include <nlohmann/json.hpp>
#include <variant>

namespace ycmd::api {
  using LineNum = int;
  using ColumnNum = int;
  using FilePath = std::filesystem::path;

  struct Location {
    LineNum line_num;
    ColumnNum column_number;
    FilePath filepath;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      Location,
      line_num,
      column_number,
      filepath);
  };

  struct Range {
    Location start;
    Location end;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      Range,
      start,
      end);
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
    std::string working_dir;
    json::object_t extra_conf_data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      SimpleRequest,
      line_num,
      column_num,
      filepath,
      file_data,
      completer_target,
      working_dir,
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
    // This request actually accepts arbitrary objects (e.g. for sorting the
    // finder window), so we just store a vector of jsons
    std::vector<json> candidates;
    std::optional<std::string> sort_property;
    std::string query;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
      FilterAndSortCandidatesRequest,
      candidates,
      sort_property,
      query);
  };

  struct InitializeRequest {
    json user_options;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
      InitializeRequest,
      user_options);
  };

  struct EventNotification : SimpleRequest
  {
#define EVENTS \
    EVENT( FileReadyToParse ) \
    EVENT( FileSave ) \
    EVENT( BufferVisit ) \
    EVENT( BufferUnload ) \
    EVENT( InsertLeave ) \
    EVENT( CurrentIdentifierFinished )

#define EVENT( e ) e,
    enum class Event
    {
      EVENTS
    };
#undef EVENT

    Event event_name;

    // tag_files
    // syntax_keywords

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE(
      EventNotification,
      SimpleRequest,
      event_name);
  };

#define EVENT( e ) { EventNotification::Event::e, #e },
  NLOHMANN_JSON_SERIALIZE_ENUM( EventNotification::Event, { EVENTS } )
#undef EVENT
}

namespace ycmd::responses {
  using namespace api;

  template<typename T>
  using Nullable = nlohmann::Nullable<T>;

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
      errors );
  };

  struct InlayHintsResponse {
    struct InlayHint {
      std::string hint;
      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        InlayHint,
        hint );
    };
    std::vector<InlayHint> inlay_hints;
    std::vector<Error> errors;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
      InlayHintsResponse,
      inlay_hints,
      errors);
  };

  struct DebugInfoResponse {
    // Probably needed to run jedi...
    struct Python {
      std::string executable{};
      Nullable<std::string> version{std::nullopt};

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        Python,
        executable,
        version
      );
    } python;

    struct Clang {
      bool has_support{false};
      Nullable<std::string> version{std::nullopt};

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        Clang,
        has_support,
        version
      );
    } clang;

    struct ExtraConf {
      std::string path;
      bool is_loaded;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        ExtraConf,
        path,
        is_loaded
      );
    } extra_conf;

    json completer;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
      DebugInfoResponse,
      python,
      clang,
      extra_conf,
      completer
    );
  };
}
