#pragma once

#include "api.h"
#include <algorithm>
#include <boost/core/ignore_unused.hpp>
#include <boost/regex.hpp>
#include <boost/regex/v5/match_flags.hpp>
#include <boost/regex/v5/match_results.hpp>
#include <boost/regex/v5/regex_fwd.hpp>
#include <boost/regex/v5/regex_iterator.hpp>
#include <boost/regex/v5/regex_match.hpp>
#include <boost/regex/v5/regex_search.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

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
  }

  const std::unordered_map< std::string,
                      boost::regex,
                      detail::string_hash,
                      std::equal_to<> > FILETYPE_TO_IDENTIFIER_REGEX {
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

  bool IsIdentifier( const boost::regex& identifier_regex,
                     std::string_view str ) {
    return boost::regex_match( str.begin(), str.end(), identifier_regex );
  }

  bool IsIdentifier( std::string_view str, std::string_view filetype ) {
    auto identifier_regex = IdentifierRegexForFiletype( filetype );
    return IsIdentifier( identifier_regex, str );
  }

}
