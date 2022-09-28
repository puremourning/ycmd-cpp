#pragma once

#include <algorithm>
#include <boost/core/ignore_unused.hpp>
#include <boost/regex.hpp>
#include <boost/regex/v5/match_flags.hpp>
#include <boost/regex/v5/match_results.hpp>
#include <boost/regex/v5/regex_fwd.hpp>
#include <boost/regex/v5/regex_iterator.hpp>
#include <boost/regex/v5/regex_match.hpp>
#include <boost/regex/v5/regex_search.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ycmd.h"
#include "api.h"

namespace ycmd {
  const boost::regex DEFAULT_IDENTIFIER_REGEX{ R"([^\W\d]\w*)" };

  namespace detail {
    // this ugly boilerplate is required to make heterogenous lookup work for
    // unordered containers
    struct string_hash {
      using is_transparent = void;

      auto operator() (const std::string& s) const {
        return std::hash<std::string>{}(s);
      }
      auto operator() (std::string_view s) const {
        return std::hash<std::string_view>{}(s);
      }
      auto operator() (const char * s) const {
        return std::hash<std::string_view>{}(s);
      }
    };

    using svregex_iterator =
      boost::regex_iterator<std::string_view::const_iterator>;
    using svmatch = boost::match_results<std::string_view::const_iterator>;

    using svregex_token_iterator =
      boost::regex_token_iterator<std::string_view::const_iterator>;
  }

  const std::unordered_map< std::string,
                      boost::regex,
                      detail::string_hash,
                      std::equal_to<> > FILETYPE_TO_IDENTIFIER_REGEX {
    // TODO: There's a lot to port here from identifier_utils.py
  };

  const boost::regex& IdentifierRegexForFiletype( std::string_view filetype ) {
    if ( auto pos = FILETYPE_TO_IDENTIFIER_REGEX.find( filetype );
         pos != FILETYPE_TO_IDENTIFIER_REGEX.end() ) {
      return pos->second;
    }

    return DEFAULT_IDENTIFIER_REGEX;
  }

  std::vector<std::string> IdentifiersFromBuffer(
    const api::SimpleRequest::FileData& file )
  {
    auto identifier_regex = IdentifierRegexForFiletype( file.filetypes[ 0 ] );

    boost::smatch results;
    boost::sregex_iterator b( file.contents.begin(),
                              file.contents.end(),
                              identifier_regex );
    boost::sregex_iterator e;

    // FIXME: sigh... more sad copying
    std::vector<std::string> candidates;
    std::for_each( b, e, [&]( const auto& match ) {
      candidates.push_back( match.str() );
    } );

    return candidates;
  }

#if 0
  const std::unordered_map< std::string,
                      boost::regex,
                      detail::string_hash,
                      std::equal_to<> > FILETYPE_TO_COMMENT_AND_STRING_REGEX {

    // TODO: There's a lot to port here from identifier_utils.py
  };

  std::string StripCommentsIfRequired(
    const api::SimpleRequest::FileData& file )
  {
    if ( !server::user_options[
          "collect_identifiers_from_comments_and_strings" ] )
    {
      assert( false && "Not implemented yet" );
    }
    return file.contents;
  }
#endif

  boost::regex SPLIT_LINES{ "\n" };

  std::vector<std::string_view> SplitLines( std::string_view contents )
  {
      std::vector<std::string_view> file_lines;
      detail::svregex_token_iterator p{ contents.begin(),
                                        contents.end(),
                                        SPLIT_LINES,
                                        -1 };
      detail::svregex_token_iterator end{};
      while ( p != end )
      {
        const auto& match = *p;
        file_lines.push_back( { match.first, (size_t)match.length() } );
        ++p;
      }
      return file_lines;
  }

  template< typename TPosOperator >
    requires std::relation< TPosOperator, size_t, size_t >
  std::string IdentifierAtIndex( std::string_view text,
                                 size_t index,
                                 std::string_view filetype,
                                 TPosOperator op )
  {
    if ( index > text.length() )
    {
      return "";
    }

    auto identifier_regex = IdentifierRegexForFiletype( filetype );
    detail::svmatch results;
    detail::svregex_iterator b( text.begin(), text.end(), identifier_regex );
    detail::svregex_iterator e;

    for( auto it = b; it != e; ++it )
    {
      const auto& match = *it;
      size_t end = match.position() + match.length();
      if ( op( end, index ) )
      {
        return match.str();
      }
    }

    return "";
  }

  std::string IdentifierUnderCursor( api::SimpleRequest& request_data )
  {
    const auto& file = request_data.file_data[ request_data.filepath ];
    // auto contents = StripCommentsIfRequired( file );
    const auto& lines = SplitLines( file.contents );
    const auto& line = lines[ request_data.line_num - 1 ];

    return IdentifierAtIndex( line,
                              request_data.column_num -1,
                              file.filetypes[ 0 ],
                              std::greater<>() );
  }

  std::string IdentifierBeforeCursor( api::SimpleRequest& request_data )
  {
    const auto& file = request_data.file_data[ request_data.filepath ];
    // auto contents = StripCommentsIfRequired( file );
    const auto& lines = SplitLines( file.contents );
    const auto& line = lines[ request_data.line_num - 1 ];

    // TODO: The real code in ycmd handles where the identifier is on the
    // previous line
    return IdentifierAtIndex( line,
                              request_data.column_num -1,
                              file.filetypes[ 0 ],
                              std::less_equal<>() );
  }

  bool IsIdentifier( const boost::regex& identifier_regex,
                     std::string_view str ) {
    return boost::regex_match( str.begin(), str.end(), identifier_regex );
  }

  bool IsIdentifier( std::string_view str, std::string_view filetype ) {
    auto identifier_regex = IdentifierRegexForFiletype( filetype );
    return IsIdentifier( identifier_regex, str );
  }

}
