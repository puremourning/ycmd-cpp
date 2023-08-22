#include "../identifier_utils.cpp"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>

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
        { "/foo", api::SimpleRequest::FileData{
          .filetypes = { "foo" },
          .contents = std::string{contents}
        } }
      }
    };
  }
}

namespace
{
  using PosResult = std::tuple< size_t /*pos*/, std::string_view /* result */>;
  struct IdentifierBeforeCursorSingleFileTest : testing::TestWithParam<std::tuple<
    std::string_view, // text
    PosResult
  >> {};

  TEST_P( IdentifierBeforeCursorSingleFileTest, MatchesAtPos )
  {
    auto [ text, pos_result ] = GetParam();
    auto [ pos, result ] = pos_result;
    EXPECT_EQ( IdentifierBeforeCursor( make_simple_request( 1, pos, text ) ),
               result );
  }
}

#define INSTANTIATE_POS_STRING_TEST( fixture, name, text, ... ) \
  INSTANTIATE_TEST_SUITE_P( \
    name, \
    fixture,\
    testing::Combine(\
      testing::Values( text ), \
      testing::ValuesIn( std::initializer_list<PosResult> \
        __VA_ARGS__ \
      ) \
    ), \
    []( const auto& info ) { \
      /* for some reason i can't use a structured binding here.. */ \
      auto pos = std::get<size_t>( std::get<PosResult>( info.param ) ); \
      return std::to_string( pos ); \
    });

INSTANTIATE_POS_STRING_TEST(
  IdentifierBeforeCursorSingleFileTest,
  SimpleWord,
  " foo ",
  {
    { 1, "" },
    { 2, "" },
    { 3, "" },
    { 4, "" },
    { 5, "foo" },
    { 6, "foo" },
    { 7, "" },
  }
)

INSTANTIATE_POS_STRING_TEST(
  IdentifierBeforeCursorSingleFileTest,
  Sentence,
  "a walk in the park",
  {
    { 1, "" },
    { 2, "a" },
    { 3, "a" },
    { 4, "a" },
    { 5, "a" },
    { 6, "a" },
    { 7, "walk" },
    { 8, "walk" },
    { 9, "walk" },
    { 10, "in" },
    { 11, "in" },
    { 12, "in" },
    { 13, "in" },
    { 14, "the" },
    { 15, "the" },
    { 16, "the" },
  }
)

namespace
{
  struct IdentifierUnderCursorSingleFileTest : testing::TestWithParam<std::tuple<
    std::string_view, // text
    PosResult
  >> {};

  TEST_P( IdentifierUnderCursorSingleFileTest, MatchesAtPos )
  {
    auto [ text, pos_result ] = GetParam();
    auto [ pos, result ] = pos_result;
    EXPECT_EQ( IdentifierUnderCursor( make_simple_request( 1, pos, text ) ),
               result );
  }
}

INSTANTIATE_POS_STRING_TEST(
  IdentifierUnderCursorSingleFileTest,
  SimpleWord,
  " foo ",
  {
    { 1, "foo" },
    { 2, "foo" },
    { 3, "foo" },
    { 4, "foo" },
    { 5, "" },
    { 6, "" },
    { 7, "" },
  }
)

INSTANTIATE_POS_STRING_TEST(
  IdentifierUnderCursorSingleFileTest,
  Sentence,
  "a walk in the park",
  {
    { 1, "a" },
    { 2, "walk" },
    { 3, "walk" },
    { 4, "walk" },
    { 5, "walk" },
    { 6, "walk" },
    { 7, "in" },
    { 8, "in" },
    { 9, "in" },
    { 10, "the" },
    { 11, "the" },
    { 12, "the" },
    { 13, "the" },
    { 14, "park" },
    { 15, "park" },
    { 16, "park" },
  }
)

#undef INSTANTIATE_POS_STRING_TEST

int main( int argc, char** argv )
{
  testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
