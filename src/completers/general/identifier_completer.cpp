#pragma once

#include "core/IdentifierCompleter.h"

#include <vector>
#include "api.h"
#include "ycmd.h"
#include "request_wrap.cpp"

namespace ycmd::completers::general {
  using namespace ycmd;

  struct IdentifierCompleter
  {
    YouCompleteMe::IdentifierCompleter completer;

    std::vector<api::Candidate> compute_candiatdes(
      ycmd::RequestWrap& request_wrap )
    {
      auto completions = completer.CandidatesForQueryAndType(
            request_wrap.query(),
            request_wrap.first_filetype() );

      std::vector<api::Candidate> candidates;
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
      return candidates;
    }
  };
}
