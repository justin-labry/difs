/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
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

#ifndef REPO_NG_TOOLS_NDNGETFILE_HPP
#define REPO_NG_TOOLS_NDNGETFILE_HPP

#include <ndn-cxx/face.hpp>

#include "../src/manifest/manifest.hpp"

namespace repo {

class Consumer : boost::noncopyable
{
public:
  Consumer(const std::string& dataName, std::ostream& os,
           bool verbose,
           int interestLifetime, int timeout,
           bool mustBeFresh = false)
    : m_dataName(dataName)
    , m_os(os)
    , m_verbose(verbose)
    , m_isFinished(false)
    , m_isFirst(true)
    , m_interestLifetime(interestLifetime)
    , m_timeout(timeout)
    , m_currentSegment(0)
    , m_totalSize(0)
    , m_retryCount(0)
    , m_mustBeFresh(mustBeFresh)
    , m_manifest("", 0, 0)
    , m_finalBlockId(0)
  {
  }

  void
  run();

private:
  void
  fetchData(const Manifest& manifest, uint64_t segmentId);

  ndn::Name
  selectRepoName(const Manifest& manifest, int segmentId);

  void
  onManifest(const ndn::Interest& interest, const ndn::Data& data);

  void
  onVersionedData(const ndn::Interest& interest, const ndn::Data& data);

  void
  onUnversionedData(const ndn::Interest& interest, const ndn::Data& data);

  void
  onTimeout(const ndn::Interest& interest);

  void
  readData(const ndn::Data& data);

  void
  fetchNextData();

private:

  ndn::Face m_face;
  ndn::Name m_dataName;  // /example/data/1
  ndn::Name m_locationName;  // /storage0/example/data/1
  std::ostream& m_os;
  bool m_verbose;
  bool m_isFinished;
  bool m_isFirst;
  ndn::time::milliseconds m_interestLifetime;
  ndn::time::milliseconds m_timeout;
  uint64_t m_currentSegment;
  int m_totalSize;
  int m_retryCount;
  bool m_mustBeFresh;

  Manifest m_manifest;
  uint64_t m_finalBlockId;
};

} // namespace repo

#endif // REPO_NG_TOOLS_NDNGETFILE_HPP