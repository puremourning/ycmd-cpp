#include "../request_wrap.cpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

#include <fmt/format.h>
#include <ranges>

namespace
{
  using namespace ycmd;

  struct Fixture : testing::Test
  {
    // The copy-assignment operator on this RequestWrap thingy doesn't work, so
    // we have to new/delete fresh ones
    std::unique_ptr<RequestWrap> wrap;

    void BuildRequestForTypes( int line_num,
                               int column_num,
                               std::vector<std::string> filetypes,
                               std::string filename,
                               std::string contents )
    {
      constexpr auto tmplt =  R"(
      {{
        "filepath": "{filename}",
        "line_num": {line_num},
        "column_num": {column_num},
        "file_data": {{
          "{filename}": {{
            "filetypes": [ {filetypes} ],
            "contents": "{contents}"
          }}
        }}
      }}
      )"sv;

      auto ftv = filetypes | std::ranges::views::transform(
        []( const auto& s ) {
          std::string result = "\"";
          result += s;
          result += '"';
          return result;
        } );

      using namespace fmt::literals;
      auto json_string = fmt::format(
        tmplt,
        "filename"_a = filename,
        "line_num"_a = line_num,
        "column_num"_a = column_num,
        "filetypes"_a = fmt::join( ftv, ","sv ),
        "contents"_a = contents );
      auto j = json::parse( json_string );
      auto r = j.get< api::SimpleRequest >();
      wrap.reset( new RequestWrap{
        .req = r,
        .raw_req = j
      } );
    }

    // TODO: Overload with a proper json version and use the parser
    void BuildRequest( int line_num,
                       int column_num,
                       std::string filetype,
                       std::string filename,
                       std::string contents )
    {
      return BuildRequestForTypes( line_num,
                                   column_num,
                                   { filetype },
                                   filename,
                                   contents );
    }
  };
}

TEST_F(Fixture, lines_multiple)
{
  BuildRequest( 4,1, "tst", "test_file", "One\\nTwo\\nThree\\nFour" );
  ASSERT_EQ( wrap->lines().size(), 4 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One" );
  EXPECT_EQ( wrap->lines()[ 1 ], "Two" );
  EXPECT_EQ( wrap->lines()[ 2 ], "Three" );
  EXPECT_EQ( wrap->lines()[ 3 ], "Four" );
  EXPECT_EQ( wrap->line_bytes(), "Four" );
  EXPECT_EQ( wrap->line_value(), U"Four" );

  // newline at end
  BuildRequest( 3,1, "tst", "test_file", "One\\nTwo\\nThree\\nFour\\n" );
  ASSERT_EQ( wrap->lines().size(), 4 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One" );
  EXPECT_EQ( wrap->lines()[ 1 ], "Two" );
  EXPECT_EQ( wrap->lines()[ 2 ], "Three" );
  EXPECT_EQ( wrap->lines()[ 3 ], "Four" );
  EXPECT_EQ( wrap->line_bytes(), "Three" );
  EXPECT_EQ( wrap->line_value(), U"Three" );

  BuildRequest( 3,1, "tst", "test_file", "óne\\nTwó\\nThręe\\nFóur\\n" );
  ASSERT_EQ( wrap->lines().size(), 4 );
  EXPECT_EQ( wrap->lines()[ 0 ], "óne" );
  EXPECT_EQ( wrap->lines()[ 1 ], "Twó" );
  EXPECT_EQ( wrap->lines()[ 2 ], "Thręe" );
  EXPECT_EQ( wrap->lines()[ 3 ], "Fóur" );
  EXPECT_EQ( wrap->line_bytes(), "Thręe" );
  EXPECT_EQ( wrap->line_value(), U"Thręe" );

  BuildRequest( 1,1, "tst", "test_file", "One\\n" );
  ASSERT_EQ( wrap->lines().size(), 1 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One" );
  EXPECT_EQ( wrap->line_bytes(), "One" );
  EXPECT_EQ( wrap->line_value(), U"One" );

  // just newlines
  BuildRequest( 3,1, "tst", "test_file", "\\n\\n\\n\\n\\n" );
  ASSERT_EQ( wrap->lines().size(), 5 );
  EXPECT_EQ( wrap->lines()[ 0 ], "" );
  EXPECT_EQ( wrap->lines()[ 1 ], "" );
  EXPECT_EQ( wrap->lines()[ 2 ], "" );
  EXPECT_EQ( wrap->lines()[ 3 ], "" );
  EXPECT_EQ( wrap->lines()[ 4 ], "" );
  EXPECT_EQ( wrap->line_bytes(), "" );
  EXPECT_EQ( wrap->line_value(), U"" );
}

TEST_F(Fixture, lines_single)
{
  BuildRequest( 1,1, "tst", "test_file", "One");
  EXPECT_EQ( wrap->lines().size(), 1 );
  ASSERT_EQ( wrap->lines()[ 0 ], "One" );
  EXPECT_EQ( wrap->line_bytes(), "One" );
  EXPECT_EQ( wrap->line_value(), U"One" );

  BuildRequest( 2,1, "tst", "test_file", "One");
  EXPECT_EQ( wrap->line_bytes(), "" );
  EXPECT_EQ( wrap->line_value(), U"" );

  BuildRequest( 1,1, "tst", "test_file", "One\\r");
  ASSERT_EQ( wrap->lines().size(), 1 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One\r" );
  EXPECT_EQ( wrap->line_bytes(), "One\r" );
  EXPECT_EQ( wrap->line_value(), U"One\r" );

  BuildRequest( 1,1, "tst", "test_file", "\\n");
  ASSERT_EQ( wrap->lines().size(), 1 );
  EXPECT_EQ( wrap->lines()[ 0 ], "" );
  EXPECT_EQ( wrap->line_bytes(), "" );
  EXPECT_EQ( wrap->line_value(), U"" );

  BuildRequest( 1, 5, "tst", "test_file", "fóóbar" );
  ASSERT_EQ( wrap->lines().size(), 1 );
  EXPECT_EQ( wrap->lines()[ 0 ], "fóóbar" );
  EXPECT_EQ( wrap->line_bytes(), "fóóbar" );
  EXPECT_EQ( wrap->line_value(), U"fóóbar" );
}

TEST_F(Fixture, lines_none)
{
  BuildRequest( 1,1, "tst", "test_file", "");
  ASSERT_EQ( wrap->lines().size(), 0 );
  EXPECT_EQ( wrap->line_bytes(), "" );
  EXPECT_EQ( wrap->line_value(), U"" );
}

TEST_F(Fixture, query)
{
  BuildRequest( 1, 1, "tst", "test_file", "One\\nTwo\\nThree\\nFour\\n" );
  EXPECT_EQ( wrap->start_codepoint(), 1 );
  EXPECT_EQ( wrap->column_codepoint(), 1 );
  EXPECT_EQ( wrap->start_column(), 1 );
  EXPECT_EQ( wrap->column_num(), 1 );
  EXPECT_EQ( wrap->query_bytes(), "" );
  EXPECT_EQ( wrap->query(), U"" );

  BuildRequest( 1, 4, "tst", "test_file", "One\\nTwo\\nThree\\nFour\\n" );
  EXPECT_EQ( wrap->start_codepoint(), 1 );
  EXPECT_EQ( wrap->column_codepoint(), 4 );
  EXPECT_EQ( wrap->start_column(), 1 );
  EXPECT_EQ( wrap->column_num(), 4 );
  EXPECT_EQ( wrap->query_bytes(), "One" );
  EXPECT_EQ( wrap->query(), U"One" );

  BuildRequest( 1, 8, "tst", "test_file", "One[TwoThree]Four AndFive" );
  EXPECT_EQ( wrap->start_codepoint(), 5 );
  EXPECT_EQ( wrap->column_codepoint(), 8 );
  EXPECT_EQ( wrap->start_column(), 5 );
  EXPECT_EQ( wrap->column_num(), 8 );
  EXPECT_EQ( wrap->query_bytes(), "Two" );
  EXPECT_EQ( wrap->query(), U"Two" );
  BuildRequest( 1, 13, "tst", "test_file", "One[TwoThree]Four AndFive" );
  EXPECT_EQ( wrap->start_codepoint(), 5 );
  EXPECT_EQ( wrap->column_codepoint(), 13 );
  EXPECT_EQ( wrap->start_column(), 5 );
  EXPECT_EQ( wrap->column_num(), 13 );
  EXPECT_EQ( wrap->query_bytes(), "TwoThree" );
  EXPECT_EQ( wrap->query(), U"TwoThree" );
  BuildRequest( 1, 14, "tst", "test_file", "One[TwoThree]Four AndFive" );
  EXPECT_EQ( wrap->start_codepoint(), 14 );
  EXPECT_EQ( wrap->column_codepoint(), 14 );
  EXPECT_EQ( wrap->start_column(), 14 );
  EXPECT_EQ( wrap->column_num(), 14 );
  EXPECT_EQ( wrap->query_bytes(), "" );
  EXPECT_EQ( wrap->query(), U"" );
  BuildRequest( 1, 15, "tst", "test_file", "One[TwoThree]Four AndFive" );
  EXPECT_EQ( wrap->start_codepoint(), 14 );
  EXPECT_EQ( wrap->column_codepoint(), 15 );
  EXPECT_EQ( wrap->start_column(), 14 );
  EXPECT_EQ( wrap->column_num(), 15 );
  EXPECT_EQ( wrap->query_bytes(), "F" );
  EXPECT_EQ( wrap->query(), U"F" );

  BuildRequest( 1, 19, "tst", "test_file", "One[TwoThree]Four AndFive" );
  EXPECT_EQ( wrap->start_codepoint(), 19 );
  EXPECT_EQ( wrap->column_codepoint(), 19 );
  EXPECT_EQ( wrap->start_column(), 19 );
  EXPECT_EQ( wrap->column_num(), 19 );
  EXPECT_EQ( wrap->query_bytes(), "" );
  EXPECT_EQ( wrap->query(), U"" );
  BuildRequest( 1, 20, "tst", "test_file", "One[TwoThree]Four AndFive" );
  EXPECT_EQ( wrap->start_codepoint(), 19 );
  EXPECT_EQ( wrap->column_codepoint(), 20 );
  EXPECT_EQ( wrap->start_column(), 19 );
  EXPECT_EQ( wrap->column_num(), 20 );
  EXPECT_EQ( wrap->query_bytes(), "A" );
  EXPECT_EQ( wrap->query(), U"A" );
  BuildRequest( 1, 26, "tst", "test_file", "One[TwoThree]Four AndFive" );
  EXPECT_EQ( wrap->start_codepoint(), 19 );
  EXPECT_EQ( wrap->column_codepoint(), 26 );
  EXPECT_EQ( wrap->start_column(), 19 );
  EXPECT_EQ( wrap->column_num(), 26 );
  EXPECT_EQ( wrap->query_bytes(), "AndFive" );
  EXPECT_EQ( wrap->query(), U"AndFive" );

  BuildRequest( 1, 6, "tst", "test_file", "fóóbar" );
  EXPECT_EQ( wrap->start_codepoint(), 1 );
  EXPECT_EQ( wrap->column_codepoint(), 4 );
  EXPECT_EQ( wrap->start_column(), 1 );
  EXPECT_EQ( wrap->column_num(), 6 );
  EXPECT_EQ( wrap->query_bytes(), "fóó" );
  EXPECT_EQ( wrap->query(), U"fóó" );

  BuildRequest( 1, 14, "tst", "test_file", "fóóbar.båz" );
  EXPECT_EQ( wrap->start_codepoint(), 8 );
  EXPECT_EQ( wrap->column_codepoint(), 11 ); // TODO: is this.. right ?
  EXPECT_EQ( wrap->start_column(), 10 );
  EXPECT_EQ( wrap->column_num(), 14 );
  EXPECT_EQ( wrap->query_bytes(), "båz" );
  EXPECT_EQ( wrap->query(), U"båz" );
}

TEST_F( Fixture, copy_request_wrap )
{
  auto j = json::parse( R"(
  {
    "filepath": "/foo",
    "line_num": 1,
    "column_num": 1,
    "file_data": {
      "/foo": {
        "filetypes": [ "test" ],
        "contents": "føø bår"
      }
    }
  }
  )" );
  auto r = j.get< api::SimpleRequest >();
  RequestWrap wrap{
    .req = r,
    .raw_req = j
  };

  EXPECT_EQ( wrap.first_filetype(), "test" );
  EXPECT_EQ( wrap.line_value(), U"føø bår" );
  EXPECT_EQ( wrap.line_bytes(), "føø bår" );
  EXPECT_EQ( wrap.start_column(), 1 );
  EXPECT_EQ( wrap.start_codepoint(), 1 );
  EXPECT_EQ( wrap.column_num(), 1 );
  EXPECT_EQ( wrap.column_codepoint(), 1 );

  RequestWrap another_wrap( wrap );

  EXPECT_EQ( another_wrap.first_filetype(), "test" );
  EXPECT_EQ( another_wrap.line_value(), U"føø bår" );
  EXPECT_EQ( another_wrap.line_bytes(), "føø bår" );
  EXPECT_EQ( another_wrap.start_column(), 1 );
  EXPECT_EQ( another_wrap.start_codepoint(), 1 );
  EXPECT_EQ( another_wrap.column_num(), 1 );
  EXPECT_EQ( another_wrap.column_codepoint(), 1 );

  RequestWrap a_third_wrap;

  a_third_wrap = another_wrap;

  EXPECT_EQ( a_third_wrap.first_filetype(), "test" );
  EXPECT_EQ( a_third_wrap.line_value(), U"føø bår" );
  EXPECT_EQ( a_third_wrap.line_bytes(), "føø bår" );
  EXPECT_EQ( a_third_wrap.start_column(), 1 );
  EXPECT_EQ( a_third_wrap.start_codepoint(), 1 );
  EXPECT_EQ( a_third_wrap.column_num(), 1 );
  EXPECT_EQ( a_third_wrap.column_codepoint(), 1 );

  RequestWrap a_final_wrap = std::move( a_third_wrap );

  EXPECT_EQ( a_final_wrap.first_filetype(), "test" );
  EXPECT_EQ( a_final_wrap.line_value(), U"føø bår" );
  EXPECT_EQ( a_final_wrap.line_bytes(), "føø bår" );
  EXPECT_EQ( a_final_wrap.start_column(), 1 );
  EXPECT_EQ( a_final_wrap.start_codepoint(), 1 );
  EXPECT_EQ( a_final_wrap.column_num(), 1 );
  EXPECT_EQ( a_final_wrap.column_codepoint(), 1 );

}

TEST_F( Fixture, first_filetype )
{
  BuildRequestForTypes( 1, 26, { "tst", "toast" }, "test_file", "" );
  EXPECT_EQ( wrap->first_filetype(), "tst" );
  BuildRequestForTypes( 1, 26, { "tst" }, "test_file", "" );
  EXPECT_EQ( wrap->first_filetype(), "tst" );
  BuildRequestForTypes( 1, 26, { "toast", "c" }, "test_file", "" );
  EXPECT_EQ( wrap->first_filetype(), "toast" );
}

// TODO: test with unicode actual values

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
