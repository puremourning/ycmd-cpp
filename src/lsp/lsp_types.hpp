#pragma once

#include <optional>
#include <variant>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "json/json_serialisation.hpp"

namespace lsp
{
  // LSP 3.16.0

  using json = nlohmann::json;

  // Types {{{
  using integer = int64_t;
  using uinteger = uint64_t;
  using decimal = double;
  using number = double;
  using boolean = bool;
  using string = std::string;
  using object = nlohmann::json::object_t;
  using any = nlohmann::json;
  template< typename... Args> using array = std::vector<Args...>;
  template< typename... Args> using one_of = std::variant<Args...>;
  template< typename... Args> using optional = std::optional<Args...>;

  // common types that are repated
  using DocumentURI = string;
  using URI = string;

  // An optional which should be on the wire as null. Normally we don't include
  // optional values when serialising
  template< typename T >
  struct Nullable : optional< T > {};

  template<typename T, typename enable = void>
  struct is_optional : std::false_type { };

  template<typename T>
  struct is_optional<optional<T>> : std::true_type { };

  template<typename T>
  struct is_optional<Nullable<T>> : std::false_type {};

  static_assert( !is_optional<std::string>::value );
  static_assert( is_optional<optional<std::string>>::value );
  static_assert( !is_optional<Nullable<std::string>>::value );

  // we write our own field-by-field code used by the macros below. We do that
  // do change the default behaviour of optional to be present/not
  // present, and introduce Nullable<T> as a type which expects/outputs nulls

  void to_json_optional( json& j, const char* fieldName, auto&& v )
  {
    using T = std::remove_cvref_t<decltype( v )>;
    if constexpr ( is_optional< T >::value )
    {
      if ( v.has_value() )
      {
        j[fieldName] = v;
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
    j[fieldName] = static_cast<optional<T>>(v);
  }

  void from_json_optional( const json& j, const char *fieldName, auto&& v )
  {
    j.at( fieldName ).get_to( v );
  }

  // special case for Nullable<T> - use the default behvaiour
  template<typename T>
  void from_json_optional( const json& j,
                           const char *fieldName,
                           Nullable< T >& v )
  {
    j.at( fieldName ).get_to( static_cast< optional< T >& >( v ) );
  }

#undef NLOHMANN_JSON_TO
#define NLOHMANN_JSON_TO(v1) \
    to_json_optional( nlohmann_json_j, #v1, nlohmann_json_t.v1 );

#undef NLOHMANN_JSON_FROM
#define NLOHMANN_JSON_FROM(v1) \
    from_json_optional( nlohmann_json_j, #v1, nlohmann_json_t.v1 );


  // }}}

  // Errors {{{

  enum class ErrorCodes
  {
    // Defined by JSON RPC
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,

    /**
     * This is the start range of JSON RPC reserved error codes.
     * It doesn't denote a real error code. No LSP error codes should
     * be defined between the start and end range. For backwards
     * compatibility the `ServerNotInitialized` and the `UnknownErrorCode`
     * are left in the range.
     *
     * @since 3.16.0
     */
    jsonrpcReservedErrorRangeStart = -32099,
    /** @deprecated use jsonrpcReservedErrorRangeStart */
    serverErrorStart = jsonrpcReservedErrorRangeStart,

    /**
     * Error code indicating that a server received a notification or
     * request before the server has received the `initialize` request.
     */
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,

    /**
     * This is the end range of JSON RPC reserved error codes.
     * It doesn't denote a real error code.
     *
     * @since 3.16.0
     */
    jsonrpcReservedErrorRangeEnd = -32000,
    /** @deprecated use jsonrpcReservedErrorRangeEnd */
    serverErrorEnd = jsonrpcReservedErrorRangeEnd,

    /**
     * This is the start range of LSP reserved error codes.
     * It doesn't denote a real error code.
     *
     * @since 3.16.0
     */
    lspReservedErrorRangeStart = -32899,

    ContentModified = -32801,
    RequestCancelled = -32800,

    /**
     * This is the end range of LSP reserved error codes.
     * It doesn't denote a real error code.
     *
     * @since 3.16.0
     */
    lspReservedErrorRangeEnd = -32800,
  };

  // }}}

  // Base Protocol {{{

  struct Message
  {
    std::string jsonrpc;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( Message,
                                    jsonrpc );
  }; // TODO(Ben): This message object is compeltely pointless, remove it

  template< typename Payload = any >
  struct RequestMessage : Message
  {
    string id; // TODO: I think this can also be a number
    string method;
    optional< Payload > params;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE( RequestMessage,
                                                          Message,
                                                          id,
                                                          method,
                                                          params );

  };

  template< typename Data = any >
  struct ResponseError
  {
    integer code;
    string message;
    Data data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( ResponseError,
                                    code,
                                    message,
                                    data );

  };

  template< typename ResultType = any, typename ErrorType = any >
  struct ResponseMessage : Message
  {
    string id; // TODO(Ben) when is null valid?
    ResultType result;
    optional< ResponseError< ErrorType> > error;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE( ResponseMessage,
                                                          Message,
                                                          id,
                                                          result,
                                                          error );

  };

  template< typename Payload = one_of< array<any>, object > >
  struct NotificationMessage : Message
  {
    string method;
    optional< Payload > params;
  };

  // }}}

  // Basic Structures {{{

  struct Position
  {
    uinteger line;
    uinteger character;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( Position,
                                    line,
                                    character );
  };

  struct Range
  {
    Position start;
    Position end;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( Range,
                                    start,
                                    end );
  };

  struct Location
  {
    DocumentURI uri;
    Range range;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( Location,
                                    uri,
                                    range );
  };

  // }}}

  // General Messages {{{

  struct WorkspaceFolder
  {
    DocumentURI uri;
    string name;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( WorkspaceFolder,
                                    uri,
                                    name );
  };

  struct ClientCapabilities
  {
    struct Workspace {
      bool applyEdit;
      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( Workspace,
                                                   applyEdit );
    } workspace;

    struct TextDocumentClientCapabilities {
      struct CompletionClientCapabilities {
        bool snippetSupport;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
          CompletionClientCapabilities,
          snippetSupport );
      } completion;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        TextDocumentClientCapabilities,
        completion );
    } textDocument;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( ClientCapabilities,
                                                 workspace,
                                                 textDocument );

  };

  struct InitializeParams
  {
    Nullable<integer> processId;
    string rootUri;
    any initializationOptions;
    Nullable< array< WorkspaceFolder > > workspaceFolders;
    ClientCapabilities capabilities;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( InitializeParams,
                                    processId,
                                    rootUri,
                                    initializationOptions,
                                    workspaceFolders,
                                    capabilities );

  };

  // }}}

}

// vim: foldmethod=marker
