#pragma once

#include "api.hpp"
#include "ycmd.hpp"

namespace ycmd::completers::general {
  struct FilenameCompleter
  {
    Async<void> handle_event_notification(
      const requests::EventNotification& request_data )
    {
      co_return;
    }
  };
}
