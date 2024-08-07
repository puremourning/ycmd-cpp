// Copyright (C) 2013-2018 ycmd contributors
//
// This file is part of ycmd.
//
// ycmd is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// ycmd is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ycmd.  If not, see <http://www.gnu.org/licenses/>.

#ifndef IDENTIFIERDATABASE_H_ZESX3CVR
#define IDENTIFIERDATABASE_H_ZESX3CVR

#ifdef YCM_ABSEIL_SUPPORTED
#include <absl/container/flat_hash_map.h>
namespace YouCompleteMe {
template< typename K, typename V >
using HashMap = absl::flat_hash_map< K, V >;
} // namespace YouCompleteMe
#else
#include <unordered_map>
namespace YouCompleteMe {
template< typename K, typename V >
using HashMap = std::unordered_map< K, V >;
} // namespace YouCompleteMe
#endif
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace YouCompleteMe {

class Candidate;
class Result;
template< typename Candidate >
class Repository;


// filepath -> identifiers
using FilepathToIdentifiers = HashMap< std::string, std::vector< std::string > >;

// filetype -> (filepath -> identifiers)
using FiletypeIdentifierMap = HashMap< std::string, FilepathToIdentifiers >;


// This class stores the database of identifiers the identifier completer has
// seen. It stores them in a data structure that makes it easy to tell which
// identifier came from which file and what files have which filetypes.
//
// The main point of this class is to isolate the parts of the code that need
// access to this internal data structure so that it's easier to confirm that
// mutexes are used correctly to protect concurrent access.
//
// This class is thread-safe.
class IdentifierDatabase {
public:
  YCM_EXPORT IdentifierDatabase();
  IdentifierDatabase( const IdentifierDatabase& ) = delete;
  IdentifierDatabase& operator=( const IdentifierDatabase& ) = delete;

  void AddSingleIdentifier(
    std::string&& new_candidate,
    std::string&& filetype,
    std::string&& filepath );

  void RecreateIdentifiers( FiletypeIdentifierMap&& filetype_identifier_map );

  void RecreateIdentifiers(
    std::vector< std::string >&& new_candidates,
    std::string&& filetype,
    std::string&& filepath );

  void ClearCandidatesStoredForFile( std::string&& filetype,
                                     std::string&& filepath );

  std::vector< Result > ResultsForQueryAndType(
    std::string_view query,
    const std::string &filetype,
    const size_t max_results ) const;

private:
  std::vector< Candidate > &GetCandidateSet(
    std::string&& filetype,
    std::string&& filepath );

  void RecreateIdentifiersNoLock(
    std::vector< std::string >&& new_candidates,
    std::string&& filetype,
    std::string&& filepath );


  // filepath -> ( candidate )
  using FilepathToCandidates = HashMap< std::string, std::vector< Candidate > >;

  // filetype -> ( filepath -> ( candidate ) )
  using FiletypeCandidateMap = HashMap< std::string, FilepathToCandidates >;


  Repository< Candidate > &candidate_repository_;

  FiletypeCandidateMap filetype_candidate_map_;
  // mutable std::shared_mutex filetype_candidate_map_mutex_;
};

} // namespace YouCompleteMe


#endif /* end of include guard: IDENTIFIERDATABASE_H_ZESX3CVR */

