/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2017, Regents of the University of California.
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

#include "delete-handle.hpp"
#include "util.hpp"
#include "../manifest/manifest.hpp"

namespace repo {

static const milliseconds DEFAULT_INTEREST_LIFETIME(4000);

DeleteHandle::DeleteHandle(Face& face, RepoStorage& storageHandle, KeyChain& keyChain,
                           Scheduler& scheduler,
                           Validator& validator,
                           ndn::Name const& clusterPrefix, int clusterSize)
  : BaseHandle(face, storageHandle, keyChain, scheduler)
  , m_interestLifetime(DEFAULT_INTEREST_LIFETIME)
  , m_validator(validator)
  , m_clusterPrefix(clusterPrefix)
  , m_clusterSize(clusterSize)
{
}

void
DeleteHandle::onDeleteInterest(const Name& prefix, const Interest& interest)
{
  m_validator.validate(interest, bind(&DeleteHandle::onDeleteValidated, this, _1, prefix),
                                 bind(&DeleteHandle::onValidationFailed, this, _1, _2));
}

void
DeleteHandle::onDeleteManifestInterest(const Name& prefix, const Interest& interest)
{
  m_validator.validate(interest, bind(&DeleteHandle::onDeleteManifestValidated, this, _1, prefix),
                                 bind(&DeleteHandle::onValidationFailed, this, _1, _2));
}

void
DeleteHandle::onDeleteDataInterest(const Name& prefix, const Interest& interest)
{
  m_validator.validate(interest, bind(&DeleteHandle::onDeleteDataValidated, this, _1, prefix),
                                 bind(&DeleteHandle::onValidationFailed, this, _1, _2));
}

void
DeleteHandle::onDeleteValidated(const Interest& interest, const Name& prefix)
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
  auto hash = Manifest::getHash(name);

  ProcessId processId = generateProcessId();
  ProcessInfo& process = m_processes[processId];
  process.interest = interest;

  RepoCommandParameter parameters;
  parameters.setName(hash);
  parameters.setProcessId(processId);

  auto repo = Manifest::getManifestStorage(m_clusterPrefix, name, m_clusterSize);
  Interest deleteManifestInterest = util::generateCommandInterest(
      repo, "deleteManifest", parameters, m_interestLifetime);

  getFace().expressInterest(
      deleteManifestInterest,
      bind(&DeleteHandle::onDeleteManifestCommandResponse, this, _1, _2, processId),
      bind(&DeleteHandle::onTimeout, this, _1, processId), // Nack
      bind(&DeleteHandle::onTimeout, this, _1, processId));
}

void
DeleteHandle::onDeleteManifestValidated(const Interest& interest, const Name& prefix)
{
  RepoCommandParameter parameter;

  try {
    extractParameter(interest, prefix, parameter);
  }
  catch (RepoCommandParameter::Error) {
    negativeReply(interest, 403);
    return;
  }

  auto hash = parameter.getName().toUri();
  auto manifest = getStorageHandle().readManifest(hash);

  auto repos = manifest.getRepos();

  ProcessId processId = generateProcessId();
  ProcessInfo& process = m_processes[processId];
  process.interest = interest;
  process.repos = repos;
  process.name = manifest.getName();

  deleteData(processId);

}

void
DeleteHandle::onDeleteDataValidated(const Interest& interest, const Name& prefix)
{
  RepoCommandParameter parameter;

  try {
    extractParameter(interest, prefix, parameter);
  }
  catch (RepoCommandParameter::Error) {
    negativeReply(interest, 403);
    return;
  }

  if (!parameter.hasStartBlockId() || !parameter.hasEndBlockId()) {
    negativeReply(interest, 403);
    return;
  }

  SegmentNo startBlockId = parameter.getStartBlockId();
  SegmentNo endBlockId = parameter.getEndBlockId();

  if (startBlockId > endBlockId) {
    negativeReply(interest, 403);
    return;
  }

  Name dataName = parameter.getName();
  uint64_t nDeletedDatas = 0;
  for (SegmentNo i = startBlockId; i <= endBlockId; i++) {
    Name name = dataName;
    name.appendSegment(i);
    if (getStorageHandle().deleteData(name)) {
      nDeletedDatas += 1;
    }
  }

  positiveReply(interest, parameter, 200, nDeletedDatas);
}

void
DeleteHandle::deleteData(const ProcessId processId) {
  ProcessInfo& process = m_processes[processId];

  Manifest::Repo repo = process.repos.front();
  process.repos.pop_front();
  RepoCommandParameter parameters;
  // /repo/0/data/data/1/%00%00
  auto name = ndn::Name(repo.name);
  name.append("data");
  name.append(process.name);
  parameters.setName(name);
  parameters.setStartBlockId(repo.start);
  parameters.setEndBlockId(repo.end);

  Interest deleteDataInterest = util::generateCommandInterest(
      ndn::Name(repo.name), "deleteData", parameters, m_interestLifetime);

  getFace().expressInterest(
      deleteDataInterest,
      bind(&DeleteHandle::onDeleteDataCommandResponse, this, _1, _2, processId),
      bind(&DeleteHandle::onTimeout, this, _1, processId), // Nack
      bind(&DeleteHandle::onTimeout, this, _1, processId));
}

void
DeleteHandle::onValidationFailed(const Interest& interest, const ValidationError& error)
{
  std::cerr << error << std::endl;
  negativeReply(interest, 401);
}
//listen change the setinterestfilter
void
DeleteHandle::listen(const Name& prefix)
{
  getFace().setInterestFilter(Name(m_clusterPrefix).append("delete"),
                              bind(&DeleteHandle::onDeleteInterest, this, _1, _2));
  getFace().setInterestFilter(Name(prefix).append("deleteManifest"),
                              bind(&DeleteHandle::onDeleteManifestInterest, this, _1, _2));
  getFace().setInterestFilter(Name(prefix).append("deleteData"),
                              bind(&DeleteHandle::onDeleteDataInterest, this, _1, _2));
}

void
DeleteHandle::positiveReply(const Interest& interest, const RepoCommandParameter& parameter,
                            uint64_t statusCode, uint64_t nDeletedDatas)
{
  RepoCommandResponse response;
  if (parameter.hasProcessId()) {
    response.setProcessId(parameter.getProcessId());
    response.setStatusCode(statusCode);
    response.setDeleteNum(nDeletedDatas);
  }
  else {
    response.setStatusCode(403);
  }
  reply(interest, response);
}

void
DeleteHandle::negativeReply(const Interest& interest, uint64_t statusCode)
{
  RepoCommandResponse response;
  response.setStatusCode(statusCode);
  reply(interest, response);
}

void
DeleteHandle::onTimeout(const Interest& interest, ProcessId processId) {
  auto prevInterest = m_processes[processId].interest;
  negativeReply(prevInterest, 405); // Deletion failed
  m_processes.erase(processId);
}

void
DeleteHandle::onDeleteManifestCommandResponse(const Interest& interest, const Data& data, ProcessId processId) {
  auto process = m_processes[processId];
  reply(process.interest, "OK");
  m_processes.erase(processId);
}

void
DeleteHandle::onDeleteDataCommandResponse(const Interest& interest, const Data& data, ProcessId processId) {
  auto process = m_processes[processId];

  auto repos = process.repos;

  if (repos.size() == 0) {
    reply(process.interest, "OK");
    // TODO: getStorageHandle().deleteManifest();
    m_processes.erase(processId);
  } else {
    deleteData(processId);
  }
}

} // namespace repo
