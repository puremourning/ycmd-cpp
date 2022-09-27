#include "../request_wrap.cpp"

#include <gtest/gtest.h>
#include <memory>


namespace
{
  using namespace ycmd;

  struct Fixture : testing::Test
  {
    // The copy-assignment operator on this RequestWrap thingy doesn't work, so
    // we have to new/delete fresh ones
    std::unique_ptr<RequestWrap> wrap;

    // TODO: Overload with a proper json version and use the parser
    void BuildRequest( int line_num,
                       int column_num,
                       std::string filetype,
                       std::string filename,
                       std::string contents )
    {
      // std::cout << "Req "
      //           << filename << "@" << line_num << ":" << column_num
      //           << "\n{" << contents << "}\n";
      wrap.reset(new RequestWrap());
      wrap->req.line_num = line_num;
      wrap->req.column_num = column_num;
      wrap->req.filepath = filename;
      wrap->req.file_data.emplace(
        filename,
        api::SimpleRequest::FileData{
          .filetypes = { filetype },
          .contents = contents,
        } );
    }
  };
}

TEST_F(Fixture, lines_multiple)
{
  BuildRequest( 1,1, "tst", "test_file", "One\nTwo\nThree\nFour" );
  ASSERT_EQ( wrap->lines().size(), 4 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One" );
  EXPECT_EQ( wrap->lines()[ 1 ], "Two" );
  EXPECT_EQ( wrap->lines()[ 2 ], "Three" );
  EXPECT_EQ( wrap->lines()[ 3 ], "Four" );

  // newline at end
  BuildRequest( 1,1, "tst", "test_file", "One\nTwo\nThree\nFour\n" );
  ASSERT_EQ( wrap->lines().size(), 4 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One" );
  EXPECT_EQ( wrap->lines()[ 1 ], "Two" );
  EXPECT_EQ( wrap->lines()[ 2 ], "Three" );
  EXPECT_EQ( wrap->lines()[ 3 ], "Four" );

  BuildRequest( 1,1, "tst", "test_file", "One\n" );
  ASSERT_EQ( wrap->lines().size(), 1 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One" );
  
  // just newlines
  BuildRequest( 1,1, "tst", "test_file", "\n\n\n\n\n" );
  ASSERT_EQ( wrap->lines().size(), 5 );
  EXPECT_EQ( wrap->lines()[ 0 ], "" );
  EXPECT_EQ( wrap->lines()[ 1 ], "" );
  EXPECT_EQ( wrap->lines()[ 2 ], "" );
  EXPECT_EQ( wrap->lines()[ 3 ], "" );
  EXPECT_EQ( wrap->lines()[ 4 ], "" );
}

TEST_F(Fixture, lines_single)
{
  BuildRequest( 1,1, "tst", "test_file", "One");
  EXPECT_EQ( wrap->lines().size(), 1 );
  ASSERT_EQ( wrap->lines()[ 0 ], "One" );

  BuildRequest( 1,1, "tst", "test_file", "One\r");
  ASSERT_EQ( wrap->lines().size(), 1 );
  EXPECT_EQ( wrap->lines()[ 0 ], "One\r" );

  BuildRequest( 1,1, "tst", "test_file", "\n");
  ASSERT_EQ( wrap->lines().size(), 1 );
  EXPECT_EQ( wrap->lines()[ 0 ], "" );
}

TEST_F(Fixture, lines_none)
{
  BuildRequest( 1,1, "tst", "test_file", "");
  ASSERT_EQ( wrap->lines().size(), 0 );
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
