#pragma once

#include <boost/utility/identity_type.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <type_traits>
#include <boost/log/trivial.hpp>
#include <variant>

using json = nlohmann::json;

#undef NLOHMANN_JSON_TO
#define NLOHMANN_JSON_TO(v1) \
  nlohmann::to_json_optional( nlohmann_json_j, \
                              #v1, \
                              nlohmann_json_t.v1 );

#undef NLOHMANN_JSON_FROM
#define NLOHMANN_JSON_FROM(v1) \
  nlohmann::from_json_optional( nlohmann_json_j, \
                                #v1, \
                                nlohmann_json_t.v1 );

#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE(Type, Base, ...) \
     friend void to_json(nlohmann::json& nlohmann_json_j, \
                         const Type& nlohmann_json_t) \
     {\
       nlohmann::to_json(nlohmann_json_j, \
                         static_cast<const Base&>(nlohmann_json_t)); \
       NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, \
                                                __VA_ARGS__)) \
     } \
     friend void from_json(const nlohmann::json& nlohmann_json_j, \
                           Type& nlohmann_json_t) \
     { \
       nlohmann::from_json(nlohmann_json_j, \
                           static_cast<Base&>(nlohmann_json_t));\
       Type nlohmann_json_default_obj; \
       NLOHMANN_JSON_EXPAND( \
         NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT,\
                             __VA_ARGS__)) \
     }

namespace
{
  // Try to set the value of type T into the variant data
  // if it fails, do nothing
  template <typename T, typename... Ts>
  void variant_from_json(const nlohmann::json &j, std::variant<Ts...> &data)
  {
    try {
      data = j.get<T>();
    } catch (const json::exception& e) {
      BOOST_LOG_TRIVIAL(debug) << "Ignoring alternative: "
                               << __PRETTY_FUNCTION__
                               << "\n  Because: "
                               << e.what();
    }
  }
}

// Support for optional<T> in json
namespace nlohmann
{
  // An optional which should be on the wire as null. Normally we don't include
  // optional values when serialising
  template< typename T >
  struct Nullable : std::optional< T > {};

  template<typename T, typename enable = void>
  struct is_absent_if_unset : std::false_type { };

  template<typename T>
  struct is_absent_if_unset<std::optional<T>> : std::true_type { };

  template<typename T>
  struct is_absent_if_unset<Nullable<T>> : std::false_type {};

  static_assert( !is_absent_if_unset<std::string>::value );
  static_assert( is_absent_if_unset<std::optional<std::string>>::value );
  static_assert( !is_absent_if_unset<Nullable<std::string>>::value );

  // we write our own field-by-field code used by the macros below. We do that
  // do change the default behaviour of std::optional to be present/not
  // present, and introduce Nullable<T> as a type which expects/outputs nulls

  template<typename T>
  void to_json_optional( json& j, const char* fieldName, const T& v )
  {
    if constexpr ( is_absent_if_unset< T >::value )
    {
      if ( v.has_value() )
      {
        j[fieldName] = v;
      }
      else
      {
        // convert null json to emtpy object. By default jsons are created as
        // nulls and inmplicitly convert to objects when you use
        // operator[keytype] on them. if we have an object of all optionals and
        // all are unset, we end up with null! so when we do this conversion for
        // optional, and the json type is null, then we _know_ this _should_ be
        // an empty object
        if ( j.is_null() )
        {
          j = json::object();
        }
      }
    }
    else
    {
      j[fieldName] = v;
    }
  }

  // special case for Nullable<T> - use the default behvaiour
  template<typename T>
  void to_json_optional( json& j, const char* fieldName, Nullable<T> v )
  {
    j[fieldName] = static_cast<std::optional<T>>(v);
  }

  template<typename T>
  void from_json_optional( const json& j, const char *fieldName, T& v )
  {
    if constexpr ( is_absent_if_unset< T >::value )
    {
      if ( j.contains( fieldName ) )
      {
        j.at( fieldName ).get_to( v );
      }
    }
    else
    {
        j.at( fieldName ).get_to( v );
    }
  }

  // special case for Nullable<T> - use the default behaviour
  template<typename T>
  void from_json_optional( const json& j,
                           const char *fieldName,
                           Nullable< T >& v )
  {
    if ( j.contains( fieldName ) )
    {
      j.at( fieldName ).get_to( static_cast< std::optional< T >& >( v ) );
    }
  }

  template<typename T>
  struct adl_serializer< std::optional<T> >
  {
    static void to_json( json& j, const std::optional<T>& o )
    {
      if ( o.has_value() )
      {
        j = *o;
      }
    }

    static void from_json( const json& j, std::optional<T>& o )
    {
      // In our definition optional<T> means may or may not be supplied. If
      // supplied, must be one of the valid types (null is only valid where
      // specified). Point is, that if we're going _from_ a json value, then
      // the json value _must exist_ therefore its conversion is trivial
      if ( !j.is_null() )
      {
        o = j.get<T>();
      }
    }
  };

  template< typename T >
  struct adl_serializer< Nullable<T> > : adl_serializer< std::optional<T> >
  {
  };

  template <typename... Ts>
  struct adl_serializer<std::variant<Ts...>>
  {
    static void to_json(nlohmann::json &j, const std::variant<Ts...> &data)
    {
      // Will call j = v automatically for the right type
      std::visit(
        [&j](const auto &v) {
          j = v;
        },
        data);
    }

    static void from_json(const nlohmann::json &j, std::variant<Ts...> &data)
    {
      // Call variant_from_json for all types, only one will succeed
      // TODO: Extremely inefficient..
      (variant_from_json<Ts>(j, data), ...);
    }
  };
}
