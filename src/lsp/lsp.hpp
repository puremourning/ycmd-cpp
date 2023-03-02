#pragma once

#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/process/async_pipe.hpp>
#include <nlohmann/json.hpp>
#include <utility>

#include "lsp_types.hpp"

namespace lsp {
  using json = nlohmann::json;
  namespace asio = boost::asio;

  using Pipe = boost::process::async_pipe;

  asio::experimental::coro<json> read_message( Pipe& pipe );

  asio::awaitable<void> send_message(
    Pipe& out,
    const json& message );

  template< typename Payload = any >
  asio::awaitable<void> send_request(
    Pipe& out,
    const lsp::ID& id,
    std::string_view method,
    Payload&& payload )
  {
    co_await send_message( out, lsp::RequestMessage<Payload>{
      .id = id,
      .method = std::string(method),
      .params = std::forward<Payload>(payload),
    } );
  }

  template< typename Payload = any >
  asio::awaitable<void> send_reply(
    Pipe& out,
    const lsp::ID& reply_to,
    Payload&& result )
  {
    co_await send_message( out, lsp::ResponseMessage<Payload>{
      .id = reply_to,
      .result = std::forward<Payload>(result)
    } );
  }

  template< typename ErrorType = any >
  asio::awaitable<void> send_reject(
    Pipe& out,
    const lsp::ID& reply_to,
    ResponseError<ErrorType>&& result )
  {
    co_await send_message( out, lsp::ResponseMessage<any, ErrorType>{
      .id = reply_to,
      .result = nullptr,
      .error = std::forward<ResponseError<ErrorType>>(result)
    } );
  }

  template< typename Payload = any >
  asio::awaitable<void> send_notification(
    Pipe& out,
    std::string_view method,
    Payload&& params )
  {
    co_await send_message( out, lsp::NotificationMessage<Payload>{
      .method = std::string(method),
      .params = std::forward<Payload>(params)
    } );
  }
}

