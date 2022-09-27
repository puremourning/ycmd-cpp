#pragma once

#include "identifier_utils.cpp"
#include "ycmd.h"
#include "api.h"
#include <boost/regex/v5/regex_iterator.hpp>
#include <boost/regex/v5/regex_token_iterator.hpp>
#include <functional>
#include <vector>

namespace ycmd
{
  template<typename T>
  struct Lazy
  {
    Lazy( std::function<T()>&& builder_ )
      : builder( builder_ )
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

    Lazy<std::vector<std::string_view>> lines{ [this]() {
      const auto& contents = req.file_data[ req.filepath ].contents;
      return SplitLines( contents );
    } };

    Lazy<std::string_view> line_value{ [this]() -> std::string_view {
      const auto &file_lines = lines();
      if ( req.line_num >= 1 && (size_t)req.line_num <= file_lines.size() )
      {
        return file_lines[ req.line_num -1 ];
      }
      return "";
    } };

    Lazy<std::string> first_filetype{ [this]() {
      return req.file_data[ req.filepath ].filetypes[ 0 ];
    } };

    Lazy<size_t> start_codepoint{ [this]() -> size_t {
      // TODO: This is .. wrong and kinda messy
      // Also line_value() is not impplementd
      auto end = column_codepoint() - 1; // 0-based index into line_value()
                                         // but points 1-past-the-end

      if ( end <= 0 )
        return 1; // we return a 1-based offset into line_value()

      const auto& q = line_value();
      // StartOfLongestIdentifierEndingAt column_codepoint()
      const auto& identifier_regex = IdentifierRegexForFiletype(
        first_filetype() );

      for ( size_t start = end - 1; ; --start )
      {
        if ( !IsIdentifier( identifier_regex,
                            { q.data() + start, end - start } ) ) {
          return start + 2; // this is the first non-identifier character. we
                            // start at the next character + 1 for returning
                            // 1-based value
        }

        if ( start == 0 )
        {
          return 1;
        }
      }
      // impossible codepath
      abort();
      return 1;
    } };

    Lazy<size_t> column_codepoint{ [this]() {
      return req.column_num; // TODO assumes ascii!
    } };

    Lazy<std::string_view> query{ [this]() {
      return line_value().substr( start_codepoint() - 1,
                                  column_codepoint() - start_codepoint() );
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
