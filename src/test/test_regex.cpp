#include <algorithm>
#include <boost/regex/icu.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <string>

int main()
{
  boost::u32regex r = boost::make_u32regex( R"([^\W\d]\w*)" );

  for(std::string l; std::getline( std::cin, l ) ; )
  {
    boost::u32regex_iterator<std::string::const_iterator> a{ l.begin(),
                                                             l.end(),
                                                             r };
    boost::u32regex_iterator<std::string::const_iterator> b{};

    int i = 0;
    std::for_each( a, b, [&i]( const auto& match ) {
      std::cout << ++i << ": " << match.str() << "\n";
    } );
  }
}
