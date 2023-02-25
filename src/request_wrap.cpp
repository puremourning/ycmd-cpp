#pragma once

#include <ztd/text.hpp>
#include "ycmd.h"
#include "api.h"
#include "identifier_utils.cpp"
#include "ztd/text/count_as_encoded.hpp"
#include "ztd/text/count_as_transcoded.hpp"
#include <string>
#include <string_view>

namespace ycmd
{
  template<typename T>
  struct Lazy
  {
    Lazy( std::function<T()>&& builder_ )
      : builder( std::move( builder_ ) )
    {
    }

    const T& operator()() const
    {
      if ( !value )
      {
        value.emplace( builder() );
      }
      return *value;
    }

    mutable std::optional<T> value;
    std::function<T()> builder;
  };

  struct RequestWrap
  {
    api::SimpleRequest req;
    json raw_req;

    std::u32string unicode_line_value;

    Lazy<std::vector<std::string_view>> lines{ [this]() {
      const auto& contents = req.file_data[ req.filepath ].contents;
      return SplitLines( contents );
    } };

    Lazy<std::string_view> line_bytes{ [this]() -> std::string_view {
      const auto &file_lines = lines();
      if ( req.line_num >= 1 && (size_t)req.line_num <= file_lines.size() )
      {
        return file_lines[ req.line_num -1 ];
      }
      return "";
    } };

    Lazy<std::u32string_view> line_value{ [this]() -> std::u32string_view {
      auto bytes = this->line_bytes();
      this->unicode_line_value = ztd::text::decode(
        bytes,
        ztd::text::utf8 );

      return this->unicode_line_value;
    } };

    Lazy<std::string> first_filetype{ [this]() {
      return req.file_data[ req.filepath ].filetypes[ 0 ];
    } };

    size_t column_num() const { return req.column_num; }

    Lazy<size_t> start_column{ [this]() -> size_t {
      // use the unicode-safe calculation, then convert to bytes
      // the key pointis that we can do simple integer math on u32string/_view,
      // which is required by the faffy StartOfLongestIdentifierEndingAt
      // calculation
      std::u32string_view unicode_prefix = {
        line_value().data(),
        start_codepoint()
      };
      return ztd::text::count_as_encoded( unicode_prefix,
                                          ztd::text::utf8 ).count;
    } };

    Lazy<size_t> start_codepoint{ [this]() -> size_t {
      const auto& identifier_regex = IdentifierRegexForFiletype(
        first_filetype() );
      return StartOfLongestIdentifierEndingAt( column_codepoint(),
                                               identifier_regex,
                                               line_value() );
    } };

    Lazy<size_t> column_codepoint{ [this]() {
      // column num is a 1-based byte offset. We're using it here as a length in
      // bytes.
      std::string_view prefix_bytes = { line_bytes().data(),
                                        (size_t)req.column_num };
      return ztd::text::count_as_decoded( prefix_bytes,
                                          ztd::text::utf8 ).count;
    } };

    Lazy<std::u32string_view> query{ [this]() {
      return line_value().substr(
        start_codepoint() - 1,
        column_codepoint() - start_codepoint() );
    } };

    Lazy<std::string_view> query_bytes{ [this]() {
      return line_bytes().substr(
        start_column() - 1,
        column_num() - start_column() );
    } };
  };

  RequestWrap make_request_wrap( const Request& req )
  {
    auto [ r, j ] = api::json_request< api::SimpleRequest >( req );
    return {
      .req = r,
      .raw_req = j,
    };
  }
}
