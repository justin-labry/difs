/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018, Regents of the University of California.
 *
 * This file is part of NDN repo-ng (Next generation of NDN repository).
 * See AUTHORS.md for complete list of repo-ng authors and contributors.
 *
 * repo-ng is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * repo-ng is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * repo-ng, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef REPO_HANDLES_READ_HANDLE_HPP
#define REPO_HANDLES_READ_HANDLE_HPP

#include "common.hpp"
#include "base-handle.hpp"

namespace repo {

class ReadHandle : public BaseHandle
{

public:
  using DataPrefixRegistrationCallback = std::function<void(const ndn::Name&)>;
  using DataPrefixUnregistrationCallback = std::function<void(const ndn::Name&)>;
  struct RegisteredDataPrefix
  {
    const ndn::RegisteredPrefixId* prefixId;
    int useCount;
  };

  ReadHandle(Face& face, RepoStorage& storageHandle, KeyChain& keyChain,
             Scheduler& scheduler, size_t prefixSubsetLength,
             ndn::Name const& clusterPrefix,
             int clusterSize);

  void
  listen(const Name& prefix) override;

PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  const std::map<ndn::Name, RegisteredDataPrefix>&
  getRegisteredPrefixes()
  {
    return m_insertedDataPrefixes;
  }

  /**
   * @param after Do something after actually removing a prefix
   */
  void
  onDataDeleted(const Name& name);

  /**
   * @param after Do something after successfully registering the data prefix
   */
  void
  onDataInserted(const Name& name);

  void
  connectAutoListen();

private:
  struct ProcessInfo
  {
    Interest interest; // requested from ndngetfile. Save it for later reply

    /**
     * @brief the latest time point at which EndBlockId must be determined
     *
     * Segmented fetch process will terminate if EndBlockId cannot be
     * determined before this time point.
     * It is initialized to now()+noEndTimeout when segmented fetch process begins,
     * and reset to now()+noEndTimeout each time an insert status check command is processed.
     */
    ndn::time::steady_clock::TimePoint noEndTime;
  };

private:
  /**
   * @brief Read data from backend storage
   */
  void
  onInterest(const Name& prefix, const Interest& interest);

  void
  onRegisterFailed(const Name& prefix, const std::string& reason);

  void
  onFindCommandResponse(const Interest& interest, const Data& data, ProcessId processId);

  void
  onFindCommandTimeout(const Interest& interest, ProcessId processId);

private:
  size_t m_prefixSubsetLength;
  std::map<ndn::Name, RegisteredDataPrefix> m_insertedDataPrefixes;
  ndn::util::signal::ScopedConnection afterDataDeletionConnection;
  ndn::util::signal::ScopedConnection afterDataInsertionConnection;

  ndn::time::milliseconds m_interestLifetime;

  std::map<ProcessId, ProcessInfo> m_processes;

  ndn::Name m_clusterPrefix;
  int m_clusterSize;
};

} // namespace repo

#endif // REPO_HANDLES_READ_HANDLE_HPP
