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
    json message );

  template< typename id_t, typename Payload = any >
  asio::awaitable<void> send_request(
    Pipe& out,
    id_t&& id,
    std::string_view method,
    Payload&& payload )
  {
    json message( json::value_t::object );
    message[ "jsonrpc" ] = "2.0";
    message[ "id" ] = std::forward<id_t>( id );
    message[ "method" ] = method;
    message[ "params" ] = std::forward<Payload>( payload );

    co_await send_message( out, std::move( message ) );
  }

  template< typename id_t, typename Payload = any >
  asio::awaitable<void> send_reply(
    Pipe& out,
    id_t&& reply_to,
    Payload&& result )
  {
    json message( json::value_t::object );
    message[ "jsonrpc" ] = "2.0";
    message[ "id" ] = std::forward<id_t>( reply_to );
    message[ "result" ] = std::forward<Payload>( result );

    co_await send_message( out, std::move( message ) );
  }

  template< typename id_t, typename ErrorType = any >
  asio::awaitable<void> send_reject(
    Pipe& out,
    id_t&& reply_to,
    ResponseError<ErrorType>&& result )
  {
    json message( json::value_t::object );
    message[ "jsonrpc" ] = "2.0";
    message[ "id" ] = std::forward<id_t>( reply_to );
    message[ "result" ] = std::forward<ResponseError<ErrorType>>( result );

    co_await send_message( out, std::move( message ) );
  }

  template< typename id_t, typename Payload = any >
  asio::awaitable<void> send_notification(
    id_t&& id,
    Pipe& out,
    std::string_view method,
    Payload&& params )
  {
    json message( json::value_t::object );
    message[ "jsonrpc" ] = "2.0";
    message[ "id" ] = id,
    message[ "method" ] = method;
    if ( params )
    {
      message[ "params" ] = std::forward<Payload>( params );
    }

    co_await send_message( out, std::move( message ) );
  }
}

