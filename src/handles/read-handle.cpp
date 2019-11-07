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

#include "read-handle.hpp"
#include "repo.hpp"
#include "util.hpp"
#include "../manifest/manifest.hpp"

namespace repo {

static const milliseconds DEFAULT_INTEREST_LIFETIME(4000);

ReadHandle::ReadHandle(Face& face, RepoStorage& storageHandle, KeyChain& keyChain,
                       Scheduler& scheduler, size_t prefixSubsetLength,
                       ndn::Name const& clusterPrefix, int clusterSize)
  : BaseHandle(face, storageHandle, keyChain, scheduler)
  , m_prefixSubsetLength(prefixSubsetLength)
  , m_interestLifetime(DEFAULT_INTEREST_LIFETIME)
  , m_clusterPrefix(clusterPrefix)
  , m_clusterSize(clusterSize)
{
  connectAutoListen();
}

void
ReadHandle::connectAutoListen()
{
  // Connect a RepoStorage's signals to the read handle
  if (m_prefixSubsetLength != RepoConfig::DISABLED_SUBSET_LENGTH) {
    afterDataDeletionConnection = m_storageHandle.afterDataInsertion.connect(
      [this] (const Name& prefix) {
        onDataInserted(prefix);
      });
    afterDataInsertionConnection = m_storageHandle.afterDataDeletion.connect(
      [this] (const Name& prefix) {
        onDataDeleted(prefix);
      });
  }
}

void
ReadHandle::onInterest(const Name& prefix, const Interest& interest)
{
  // FIXME: Implement it

  auto fullName = interest.getName();
  auto dataName = fullName.getSubName(prefix.size(), fullName.size() - prefix.size());

  shared_ptr<ndn::Data> data = m_storageHandle.readData(dataName);
  if (data != nullptr) {
    reply(interest, data);
  }
}

void
ReadHandle::onGetInterest(const Name& prefix, const Interest& interest)
{
  RepoCommandParameter parameter;
  try {
    extractParameter(interest, prefix, parameter);
  }
  catch (RepoCommandParameter::Error) {
    negativeReply(interest, 403);
    return;
  }
  auto name = parameter.getName().toUri();
  std::cout << "Received read interest: " << name << std::endl;
  // FIXME: name will be sent already hashed form
  auto hash = Manifest::getHash(name);
  auto repo = Manifest::getManifestStorage(m_clusterPrefix, name, m_clusterSize);

  std::cout << "Find " << hash << " using repo " << repo << std::endl;
  // TODO: Implement to return manifest

  ProcessId processId = generateProcessId();

  ProcessInfo& process = m_processes[processId];
  process.interest = interest;

  RepoCommandParameter parameters;
  parameters.setName(hash);

  parameters.setProcessId(processId);
  Interest findInterest = util::generateCommandInterest(
    repo, "find", parameters, m_interestLifetime);
  findInterest.setMustBeFresh(true);

  getFace().expressInterest(
      findInterest,
      bind(&ReadHandle::onFindCommandResponse, this, _1, _2, processId),
      bind(&ReadHandle::onFindCommandTimeout, this, _1, processId), // Nack
      bind(&ReadHandle::onFindCommandTimeout, this, _1, processId));
}

void
ReadHandle::onFindCommandResponse(const Interest& interest, const Data& data, ProcessId processId)
{
  auto process = m_processes[processId];
  Data responseData(process.interest.getName());
  auto content = data.getContent();
  std::string json(
      content.value(),
      content.value() + content.value_size()
      );
  reply(process.interest, json);
  m_processes.erase(processId);
}

void
ReadHandle::onFindCommandTimeout(const Interest& interest, ProcessId processId)
{
  m_processes.erase(processId);
}

void
ReadHandle::onRegisterFailed(const Name& prefix, const std::string& reason)
{
  std::cerr << "ERROR: Failed to register prefix in local hub's daemon" << std::endl;
  getFace().shutdown();
}

void
ReadHandle::listen(const Name& prefix)
{
  std::cout << "Listen: " << prefix << std::endl;
  getFace().setInterestFilter(Name(prefix).append("data"),
                              bind(&ReadHandle::onInterest, this, _1, _2),
                              bind(&ReadHandle::onRegisterFailed, this, _1, _2));
  getFace().setInterestFilter(Name("get"),
                              bind(&ReadHandle::onGetInterest, this, _1, _2),
                              bind(&ReadHandle::onRegisterFailed, this, _1, _2));
}

void
ReadHandle::onDataDeleted(const Name& name)
{
  // We add one here to account for the implicit digest at the end,
  // which is what we get from the underlying storage when deleting.
  Name prefix = name.getPrefix(-(m_prefixSubsetLength + 1));
  auto check = m_insertedDataPrefixes.find(prefix);
  if (check != m_insertedDataPrefixes.end()) {
    if (--(check->second.useCount) <= 0) {
      getFace().unsetInterestFilter(check->second.prefixId);
      m_insertedDataPrefixes.erase(prefix);
    }
  }
}

void
ReadHandle::onDataInserted(const Name& name)
{
  // XXX: Unnecessary
  return;
}

void
ReadHandle::negativeReply(const Interest& interest, int statusCode)
{
  RepoCommandResponse response;
  response.setStatusCode(statusCode);
  reply(interest, response);
}

} // namespace repo
