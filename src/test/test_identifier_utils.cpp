#include "../identifier_utils.cpp"
#include <gtest/gtest.h>

using namespace ycmd;

namespace
{
  api::SimpleRequest make_simple_request( int line_num,
                                          int column_num,
                                          std::string_view contents )
  {
    return api::SimpleRequest{
      .line_num = line_num,
      .column_num = column_num,
      .filepath = "/foo",
      .file_data {
        { "/foo", {
          .filetypes{ "foo" },
          .contents{ contents }
        } }
      }
    };
  }
}

TEST( IdentifierUtils, IdentifierBeforeCursor_SingleFile )
{
  EXPECT_EQ( IdentifierBeforeCursor( make_simple_request( 1, 5, " foo " ) ),
             "foo" ) ;
  EXPECT_EQ( IdentifierBeforeCursor( make_simple_request( 1, 4, " foo " ) ),
             "" ) ;

  // TODO: This doesn't work. The IdentifierAtIndex doesn't work for
  // "less_equal" because it returns the first matching one. i.e. it always
  // returns the first identifier in the line!
  EXPECT_EQ( IdentifierBeforeCursor( make_simple_request( 1, 2, "a walk in" ) ),
             "a" ) ;
  EXPECT_EQ( IdentifierBeforeCursor( make_simple_request( 1, 7, "a walk in" ) ),
             "walk" ) ;
  EXPECT_EQ( IdentifierBeforeCursor( make_simple_request( 1, 10, "a walk in" ) ),
             "in" ) ;
}

TEST( IdentifierUtils, IdentifierUnderCursor_SingleFile )
{
  EXPECT_EQ( IdentifierUnderCursor( make_simple_request( 1, 5, " foo " ) ),
             "" ) ;
  EXPECT_EQ( IdentifierUnderCursor( make_simple_request( 1, 4, " foo " ) ),
             "foo" ) ;
}

int main( int argc, char** argv )
{
  testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
