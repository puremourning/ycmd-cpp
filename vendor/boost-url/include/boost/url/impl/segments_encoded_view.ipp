//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/CPPAlliance/url
//

#ifndef BOOST_URL_IMPL_SEGMENTS_ENCODED_VIEW_IPP
#define BOOST_URL_IMPL_SEGMENTS_ENCODED_VIEW_IPP

#include <boost/url/segments_view.hpp>
#include <boost/url/error.hpp>
#include <boost/url/detail/except.hpp>
#include <boost/url/detail/path.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/rfc/query_rule.hpp>
#include <boost/url/rfc/detail/path_rules.hpp>
#include <ostream>

namespace boost {
namespace urls {

//------------------------------------------------

segments_encoded_view::
segments_encoded_view(
    string_view s,
    std::size_t nseg) noexcept
    : s_(s)
    , n_(nseg)
{
}

//------------------------------------------------
//
// Iterators
//
//------------------------------------------------

auto
segments_encoded_view::
begin() const noexcept ->
    iterator
{
    return iterator(s_, n_);
}

auto
segments_encoded_view::
end() const noexcept ->
    iterator
{
    return iterator(s_, n_, 0);
}

//------------------------------------------------
//
// Friends
//
//------------------------------------------------

result<segments_encoded_view>
parse_path(string_view s) noexcept
{
    if(s.empty())
        return segments_encoded_view();
    if(s[0] == '/')
    {
        auto rv = grammar::parse(
            s, detail::path_abempty_rule);
        if(! rv)
            return rv.error();
        return segments_encoded_view(
            rv->string(),
            detail::path_segments(
                rv->string(),
                rv->size()));
    }
    {
        auto rv = grammar::parse(
            s, detail::path_rootless_rule);
        if(! rv)
            return rv.error();
        return segments_encoded_view(
            rv->string(),
            detail::path_segments(
                rv->string(),
                rv->size()));
    }
}

} // urls
} // boost

#endif
