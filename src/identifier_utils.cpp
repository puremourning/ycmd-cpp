#pragma once

#include <algorithm>
#include <boost/core/ignore_unused.hpp>
#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>
#include <boost/regex/v5/icu.hpp>
#include <boost/regex/v5/match_flags.hpp>
#include <boost/regex/v5/match_results.hpp>
#include <boost/regex/v5/regex.hpp>
#include <boost/regex/v5/regex_fwd.hpp>
#include <boost/regex/v5/regex_iterator.hpp>
#include <boost/regex/v5/regex_match.hpp>
#include <boost/regex/v5/regex_search.hpp>

#include <c++/v1/concepts>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ycmd.hpp"
#include "api.hpp"
#include "ztd/text.hpp"

namespace ycmd {
  const boost::u32regex DEFAULT_IDENTIFIER_REGEX =
    boost::make_u32regex( R"([^\W\d]\w*)" );

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

    using svmatch = boost::match_results<std::string_view::const_iterator>;

    using u32svregex_iterator =
      boost::u32regex_iterator<std::string_view::const_iterator>;
    using u32svregex_token_iterator =
      boost::u32regex_token_iterator<std::string_view::const_iterator>;

    using u32sregex_iterator =
      boost::u32regex_iterator<std::string::const_iterator>;
    using u32sregex_token_iterator =
      boost::u32regex_token_iterator<std::string::const_iterator>;
  }

  const std::unordered_map< std::string,
                            boost::u32regex,
                            detail::string_hash,
                            std::equal_to<> > FILETYPE_TO_IDENTIFIER_REGEX {
    // TODO: There's a lot to port here from identifier_utils.py
  };

  const boost::u32regex& IdentifierRegexForFiletype( std::string_view filetype ) {
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

    detail::u32sregex_iterator b( file.contents.begin(),
                                  file.contents.end(),
                                  identifier_regex );
    detail::u32sregex_iterator e;

    // FIXME: sigh... more sad copying
    std::vector<std::string> candidates;
    candidates.reserve( std::distance( b, e ) );
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

  boost::u32regex SPLIT_LINES = boost::make_u32regex( "\n" );

  std::vector<std::string_view> SplitLines( std::string_view contents )
  {
      std::vector<std::string_view> file_lines;
      detail::u32svregex_token_iterator p{ contents.begin(),
                                           contents.end(),
                                           SPLIT_LINES,
                                           -1 };
      detail::u32svregex_token_iterator end{};

      file_lines.reserve( std::distance( p, end ) );
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
    detail::u32svregex_iterator b( text.begin(), text.end(), identifier_regex );
    detail::u32svregex_iterator e;

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

  // TODO/FIXME: THe following should work on RequestWrap, but currently there's
  // a circular include dependency (as request_wrap.cpp includes this file)
  // split out a request_wrap header. or maybe convert the whole project to
  // modules, just for fun/learning.

  std::string IdentifierUnderCursor( const api::SimpleRequest& request_data )
  {
    const auto& file = request_data.file_data.at( request_data.filepath );
    // auto contents = StripCommentsIfRequired( file );
    const auto& lines = SplitLines( file.contents );
    const auto& line = lines[ request_data.line_num - 1 ];

    return IdentifierAtIndex( line,
                              request_data.column_num -1,
                              file.filetypes[ 0 ],
                              std::greater<>() );
  }

  std::string IdentifierBeforeCursor( const api::SimpleRequest& request_data )
  {
    const auto& file = request_data.file_data.at( request_data.filepath );
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

  template< typename CharType >
  bool IsIdentifier( const boost::u32regex& identifier_regex,
                     std::basic_string_view<CharType> str ) {
    return boost::u32regex_match( str.begin(), str.end(), identifier_regex );
  }

  size_t StartOfLongestIdentifierEndingAt(
    size_t column_codepoint,
    const boost::u32regex& identifier_regex,
    std::u32string_view line_value )
  {
    auto end = column_codepoint - 1; // 0-based index into line_value()
                                       // but points 1-past-the-end

    if ( end <= 0 )
      return 1; // we return a 1-based offset into line_value()

    // We are working here in u32 space. Therefore it's safe to use simple
    // integer offsetting, as we're working in codepoints.
    for ( size_t start = end - 1; ; --start )
    {
      if ( !IsIdentifier( identifier_regex,
                          std::u32string_view{
                            line_value.data() + start,
                            end - start
                          } ) ) {
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
  }
}
