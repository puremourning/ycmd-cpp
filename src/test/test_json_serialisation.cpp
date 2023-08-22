#include "../json/json_serialisation.hpp"

#include <gtest/gtest.h>
#include <optional>

// Tests the std::optional and nlohmann::Nullable implementation, which is
// fiddly as hell and difficult to otherwise debug.

namespace thetest
{
  struct Basic
  {
    int i;
    char c;
    std::string s;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( Basic,
                                    i,
                                    c,
                                    s );
  };

  struct Derived : Basic
  {
    int id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE( Derived,
                                                          Basic,
                                                          id );
  };

  struct SimpleOptionalFields
  {
    std::optional<int> opt_int;
    std::optional<char> opt_chr;
    std::optional<std::string> opt_str;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( SimpleOptionalFields,
                                    opt_int,
                                    opt_chr,
                                    opt_str );
  };

  struct SimpleNullableFields
  {
    nlohmann::Nullable<int> opt_int;
    nlohmann::Nullable<char> opt_chr;
    nlohmann::Nullable<std::string> opt_str;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( SimpleNullableFields,
                                    opt_int,
                                    opt_chr,
                                    opt_str );
  };

  struct OptionalBase
  {
    std::optional<int> oi;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( OptionalBase,
                                    oi );
  };

  struct OptionalDerived : OptionalBase
  {
    std::optional<int> di;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE( OptionalDerived,
                                                          OptionalBase,
                                                          di );
  };

  struct NullableBase
  {
    nlohmann::Nullable<int> oi;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( NullableBase,
                                    oi );
  };

  struct NullableDerived : NullableBase
  {
    nlohmann::Nullable<int> di;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE( NullableDerived,
                                                          NullableBase,
                                                          di );
  };
}

namespace
{
  using json = nlohmann::json;

  struct Fixture : testing::Test
  {
    json j;
  };

}

TEST_F(Fixture, basic_types)
{
  auto json_str =  R"({"c":49,"i":1,"s":"one"})";
  j = thetest::Basic{ .i = 1, .c = '1', .s = "one" };
  EXPECT_EQ( j.dump(), json_str );

  auto b = json::parse( json_str ).get<thetest::Basic>();
  EXPECT_EQ( b.i, 1 );
  EXPECT_EQ( b.c, '1' );
  EXPECT_EQ( b.s, "one" );
}

TEST_F(Fixture, hierarchy)
{
  auto json_str =  R"({"c":49,"i":1,"id":50,"s":"one"})";
  j = thetest::Derived{
    {1, '1', "one"},
    50
  };
  EXPECT_EQ( j.dump(), json_str );

  {
    auto b = json::parse( json_str ).get<thetest::Derived>();
    EXPECT_EQ( b.i, 1 );
    EXPECT_EQ( b.c, '1' );
    EXPECT_EQ( b.s, "one" );
    EXPECT_EQ( b.id, 50 );
  }

  {
    auto b = json::parse( json_str ).get<thetest::Basic>();
    EXPECT_EQ( b.i, 1 );
    EXPECT_EQ( b.c, '1' );
    EXPECT_EQ( b.s, "one" );
  }
}

TEST_F(Fixture, optional_not_present_empty_object)
{
  auto json_str = R"({})";
  j = thetest::SimpleOptionalFields{};
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( json_str ).get<thetest::SimpleOptionalFields>();
  EXPECT_EQ( o.opt_chr, std::nullopt );
  EXPECT_EQ( o.opt_int, std::nullopt );
  EXPECT_EQ( o.opt_str, std::nullopt );
}

TEST_F(Fixture, optional_not_present_partial_object)
{
  auto json_str = R"({"opt_int":10})";
  j = thetest::SimpleOptionalFields{
    .opt_int{ 10 },
    .opt_chr{},
    .opt_str{}
  };
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( json_str ).get<thetest::SimpleOptionalFields>();
  EXPECT_EQ( o.opt_chr, std::nullopt );
  EXPECT_EQ( o.opt_int, 10 );
  EXPECT_EQ( o.opt_str, std::nullopt );
}

TEST_F(Fixture, nullable_not_set)
{
  auto json_str = R"({"opt_chr":null,"opt_int":null,"opt_str":null})";
  j = thetest::SimpleNullableFields{};
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( json_str ).get<thetest::SimpleNullableFields>();
  EXPECT_EQ( o.opt_chr, std::nullopt );
  EXPECT_EQ( o.opt_int, std::nullopt );
  EXPECT_EQ( o.opt_str, std::nullopt );
}

TEST_F(Fixture, nullable_not_supplied)
{
  auto json_str = R"({"opt_chr":null,"opt_int":null,"opt_str":null})";
  j = thetest::SimpleNullableFields{};
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( "{}" ).get<thetest::SimpleNullableFields>();
  EXPECT_EQ( o.opt_chr, std::nullopt );
  EXPECT_EQ( o.opt_int, std::nullopt );
  EXPECT_EQ( o.opt_str, std::nullopt );
}

TEST_F(Fixture, nullable_partial_set)
{
  auto json_str = R"({"opt_chr":null,"opt_int":12,"opt_str":null})";
  j = thetest::SimpleNullableFields{ .opt_int{ 12 } };
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( json_str ).get<thetest::SimpleNullableFields>();
  EXPECT_EQ( o.opt_chr, std::nullopt );
  EXPECT_EQ( o.opt_int, 12 );
  EXPECT_EQ( o.opt_str, std::nullopt );
}

TEST_F(Fixture, nullable_partial_notset)
{
  auto json_str = R"({"opt_chr":null,"opt_int":12,"opt_str":null})";
  j = thetest::SimpleNullableFields{ .opt_int{ 12 } };
  EXPECT_EQ( j.dump(), json_str );

  auto o =
    json::parse( R"({"opt_int":12})" ).get<thetest::SimpleNullableFields>();
  EXPECT_EQ( o.opt_chr, std::nullopt );
  EXPECT_EQ( o.opt_int, 12 );
  EXPECT_EQ( o.opt_str, std::nullopt );
}

TEST_F(Fixture, optional_hierarchy_not_set)
{
  auto json_str = R"({})";
  j = thetest::OptionalDerived{};
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( json_str ).get<thetest::OptionalDerived>();
  EXPECT_EQ( o.oi, std::nullopt );
  EXPECT_EQ( o.di, std::nullopt );
}

TEST_F(Fixture, optional_hierarchy)
{
  auto json_str = R"({"oi":10000})";
  j = thetest::OptionalDerived{ { 10000 }, std::nullopt };
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( json_str ).get<thetest::OptionalDerived>();
  EXPECT_EQ( o.oi, 10000 );
  EXPECT_EQ( o.di, std::nullopt );
}

TEST_F(Fixture, nullable_hierarchy_not_set)
{
  auto json_str = R"({"di":null,"oi":null})";
  j = thetest::NullableDerived{};
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( json_str ).get<thetest::NullableDerived>();
  EXPECT_EQ( o.oi, std::nullopt );
  EXPECT_EQ( o.di, std::nullopt );
}

TEST_F(Fixture, nullable_hierarchy)
{
  auto json_str = R"({"di":null,"oi":10000})";
  j = thetest::NullableDerived{ { 10000 }, { std::nullopt } };
  EXPECT_EQ( j.dump(), json_str );

  auto o = json::parse( R"({"oi":10000})" ).get<thetest::NullableDerived>();
  EXPECT_EQ( o.oi, 10000 );
  EXPECT_EQ( o.di, std::nullopt );
}

// TODO: tests for variants

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
