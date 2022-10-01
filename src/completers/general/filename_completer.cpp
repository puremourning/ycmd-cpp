#pragma once

#include "api.h"
#include "ycmd.h"

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
