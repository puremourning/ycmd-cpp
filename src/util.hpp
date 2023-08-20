#pragma once

#include <memory>

namespace ycmd::util
{
  template< typename... Fs >
  struct visitor : Fs...
  {
    visitor( Fs&&... fs ) : Fs( std::move( fs ) )... {}
    using Fs::operator()...;
  };
}


