#pragma once

#include <nlohmann/json.hpp>
#include <optional>

using json = nlohmann::json;
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE(Type, Base, ...)  \
     friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) {\
       nlohmann::to_json(nlohmann_json_j, static_cast<const Base&>(nlohmann_json_t)); NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
     friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { nlohmann::from_json(nlohmann_json_j, static_cast<Base&>(nlohmann_json_t)); Type nlohmann_json_default_obj; NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) }

namespace
{
  // Try to set the value of type T into the variant data
  // if it fails, do nothing
  template <typename T, typename... Ts>
  void variant_from_json(const nlohmann::json &j, std::variant<Ts...> &data)
  {
    try {
      data = j.get<T>();
    } catch (...) {
    }
  }
}

// Support for optional<T> in json
namespace nlohmann
{
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
      o = j.get<T>();
    }
  };

  template <typename... Ts>
  struct nlohmann::adl_serializer<std::variant<Ts...>>
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
