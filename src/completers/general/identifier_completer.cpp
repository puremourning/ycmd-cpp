#pragma once

#include "core/IdentifierCompleter.h"

#include <vector>
#include "api.h"
#include "ycmd.h"
#include "identifier_utils.cpp"
#include "request_wrap.cpp"

namespace ycmd::completers::general {
  using namespace ycmd;

  struct IdentifierCompleter
  {
    YouCompleteMe::IdentifierCompleter completer;

    Async<void> handle_event_notification(
      const requests::EventNotification& request_data )
    {
      const auto& file = request_data.file_data.at( request_data.filepath );

      using enum requests::EventNotification::Event;
      switch ( request_data.event_name )
      {
        case FileReadyToParse:
        {
          completer.ClearForFileAndAddIdentifiersToDatabase(
            IdentifiersFromBuffer( file ),
            file.filetypes[ 0 ],
            request_data.filepath.string() );
          break;

          // TODO: AddIdentifiersFromTagFiles
          // TODO: AddIdentifiersFromSyntax
        }
        case FileSave:
          break;
        case BufferVisit:
          break;
        case BufferUnload:
          break;
        case InsertLeave:
          completer.AddSingleIdentifierToDatabase(
            IdentifierUnderCursor( request_data ),
            file.filetypes[ 0 ],
            request_data.filepath.string() );
          break;
        case CurrentIdentifierFinished:
          completer.AddSingleIdentifierToDatabase(
            IdentifierBeforeCursor( request_data ),
            file.filetypes[ 0 ],
            request_data.filepath.string() );
          break;
      }

      co_return;
    }

    Async<std::vector<api::Candidate>> compute_candiatdes(
      const ycmd::RequestWrap& request_wrap )
    {
      auto completions = completer.CandidatesForQueryAndType(
            request_wrap.query_bytes(), // utf-8, as required by this lib
            request_wrap.first_filetype() );

      std::vector<api::Candidate> candidates;
      candidates.reserve( completions.size() );
      for ( auto& completion_sring : completions )
      {
        if ( completion_sring.length() >
              server::user_options[ "min_num_identifier_candidate_chars" ] )
        {
          candidates.push_back( api::Candidate{
            .insertion_text = completion_sring,
            .extra_menu_info = "[ID]",
          } );
        }
      }
      co_return candidates;
    }
  };
}
