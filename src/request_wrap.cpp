#pragma once

#include "identifier_utils.h"
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

  boost::regex SPLIT_LINES{ "\n" };

  struct RequestWrap
  {
    api::SimpleRequest req;
    json raw_req;

    Lazy<std::vector<std::string>> lines{ [this]() {
      std::vector<std::string> file_lines;
      const auto& contents = req.file_data[ req.file_path ].contents;
      boost::sregex_token_iterator p{ contents.begin(),
                                      contents.end(),
                                      SPLIT_LINES,
                                      -1 };
      boost::sregex_token_iterator end{};
      while ( p != end ) {
        file_lines.push_back( *p++ );
      }
      return file_lines;
    } };

    Lazy<std::string> line_value{ [this]() {
      const auto &file_lines = lines();
      if ( req.line_num >= 1 && (size_t)req.line_num <= file_lines.size() ) {
        return file_lines[ req.line_num -1 ];
      }
      return std::string( "" );
    } };

    Lazy<std::string> first_filetype{ [this]() {
      return req.file_data[ req.file_path ].filetypes[ 0 ];
    } };

    Lazy<size_t> start_codepoint{ [this]() {
      // TODO: This is .. wrong and kinda messy
      // Also line_value() is not impplementd
      auto end = column_codepoint() - 1; // 0-based index into line_value()

      if ( end == 0 )
        return end + 1; // we return a 1-based offset into line_value()

      const auto& q = line_value();
      // StartOfLongestIdentifierEndingAt column_codepoint()
      const auto& identifier_regex = IdentifierRegexForFiletype(
        first_filetype() );

      for ( size_t start = 0; start < end - 1; ++start ) {
        if ( !IsIdentifier( identifier_regex,
                            { q.data() + start, end - 1 - start } ) ) {
          return start + 1;
        }
      }
      return end + 1;
    } };

    Lazy<size_t> column_codepoint{ [this]() {
      return req.column_num; // TODO assumes ascii!
    } };

    Lazy<std::string> query{ [this]() {
      return line_value().substr( start_codepoint() - 1,
                                  column_codepoint() - 1 );
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
