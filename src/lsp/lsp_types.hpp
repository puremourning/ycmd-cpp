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
  using ID = one_of<string,number>;
  template<typename T>
  using Nullable = nlohmann::Nullable<T>;
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
    std::string jsonrpc{ "2.0" };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( Message,
                                    jsonrpc );
  }; // TODO(Ben): This message object is compeltely pointless, remove it

  template< typename Payload = any >
  struct RequestMessage : Message
  {
    ID id; // TODO: I think this can also be a number
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
    ID id;
    optional< ResultType > result;
    optional< ResponseError< ErrorType> > error;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE( ResponseMessage,
                                                          Message,
                                                          id,
                                                          result,
                                                          error );

  };

  template< typename Params = any >
  struct NotificationMessage : Message
  {
    string method;
    optional< Params > params;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE( NotificationMessage,
                                                          Message,
                                                          method,
                                                          params );
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

  struct InitializeResult
  {
    json capabilities; // TODO!

    struct ServerInfo
    {
      lsp::string name;
      lsp::string version;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(ServerInfo,
                                     name,
                                     version);
    };
    optional<ServerInfo> serverInfo;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(InitializeResult,
                                   capabilities,
                                   serverInfo);
  };

  // }}}

  // Text Document Sync {{{

  struct TextDocumentItem
  {
    DocumentURI uri;
    string languageId;
    integer version;
    string text;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( TextDocumentItem,
                                                 uri,
                                                 languageId,
                                                 version,
                                                 text );
  };

  struct TextDocumentIdentifier
  {
    DocumentURI uri;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT( TextDocumentIdentifier,
                                                 uri );
  };
  struct VersionedTextDocumentIdentifier : TextDocumentIdentifier
  {
    integer version;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE(
      VersionedTextDocumentIdentifier,
      TextDocumentIdentifier,
      version );
  };

  struct OptionalVersionedTextDocumentIdentifier : TextDocumentIdentifier
  {
    std::optional<integer> version;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT_AND_BASE(
      OptionalVersionedTextDocumentIdentifier,
      TextDocumentIdentifier,
      version );
  };

  struct DidOpenTextDocumentParams
  {
    TextDocumentItem textDocument;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( DidOpenTextDocumentParams,
                                    textDocument );
  };

  struct DidChangeTextDocumentParams
  {
    VersionedTextDocumentIdentifier textDocument;

    struct TextDocumentChangeEvent
    {
      string text;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE( TextDocumentChangeEvent,
                                      text );
    };

    std::vector<TextDocumentChangeEvent> contentChanges;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( DidChangeTextDocumentParams,
                                    textDocument,
                                    contentChanges );
  };

  struct DidCloseTextDocumentParams
  {
    TextDocumentIdentifier textDocument;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE( DidCloseTextDocumentParams,
                                    textDocument );
  };

  // }}}
}

// vim: foldmethod=marker
