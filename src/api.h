#pragma once

#include "ycmd.h"
#include <filesystem>
#include <nlohmann/detail/macro_scope.hpp>
#include <string>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include <variant>

// These are in latest nlohmann::json but they are really useful, so
// copypaste...
#define NLOHMANN_JSON_FROM_WITH_DEFAULT(v1) nlohmann_json_t.v1 = nlohmann_json_j.value(#v1, nlohmann_json_default_obj.v1);
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Type, ...)  \
     friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
     friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { Type nlohmann_json_default_obj; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, ...)  \
     inline void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
     inline void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { Type nlohmann_json_default_obj; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }


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
    std::string menu_text;
    std::string extra_menu_info;
    std::string detailed_info;
    std::string kind;
    std::string extra_data;
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
}
