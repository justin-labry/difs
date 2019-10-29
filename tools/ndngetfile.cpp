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

#include "ndngetfile.hpp"

#include <fstream>
#include <iostream>

#include <boost/lexical_cast.hpp>

#include "../src/manifest/manifest.cpp"
#include "../src/util.cpp"

#include "../src/repo-command-parameter.hpp"
#include "../src/repo-command-response.hpp"

namespace repo {

using ndn::Name;
using ndn::Interest;
using ndn::Data;
using ndn::Block;

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;

static const int MAX_RETRY = 3;

void
Consumer::fetchData(const Manifest& manifest, uint64_t segmentId)
{
  auto repoName = selectRepoName(manifest, segmentId);
  auto name = manifest.getName();
  Interest interest(repoName.append("data").append(name).appendSegment(segmentId));
  interest.setInterestLifetime(m_interestLifetime);
  //std::cout<<"interest name = "<<interest.getName()<<std::endl;
  interest.setMustBeFresh(true);

  m_face.expressInterest(interest,
                         bind(&Consumer::onUnversionedData, this, _1, _2),
                         bind(&Consumer::onTimeout, this, _1), // Nack
                         bind(&Consumer::onTimeout, this, _1));
}

ndn::Name
Consumer::selectRepoName(const Manifest& manifest, int segmentId)
{
  auto repos = manifest.getRepos();
  for (auto iter = repos.begin(); iter != repos.end(); ++iter)
  {
    auto start = iter->start;
    auto end = iter->end;
    if (start <= segmentId && segmentId <= end)
    {
      return Name(iter->name);
    }
  }

  // Should not be here
  return Name("");
}

void
Consumer::run()
{
  // Get Manifest
  RepoCommandParameter parameter;
  parameter.setName(m_dataName);
  Interest interest = util::generateCommandInterest(Name("get"), "", parameter, m_interestLifetime);

  m_face.expressInterest(
      interest,
      bind(&Consumer::onManifest, this, _1, _2),
      bind(&Consumer::onTimeout, this, _1),
      bind(&Consumer::onTimeout, this, _1));

  // processEvents will block until the requested data received or timeout occurs
  m_face.processEvents(m_timeout);
}

void
Consumer::onManifest(const Interest& interest, const Data& data)
{
  auto content = data.getContent();
  std::string json(
      content.value(),
      content.value() + content.value_size()
      );
  std::cerr << "Got Manifest: " << json << std::endl;

  auto manifest = Manifest::fromJson(json);
  m_manifest = manifest;

  auto end = manifest.getEndBlockId();
  m_finalBlockId = end;

  std::cerr << "End block id: " << end << std::endl;

  fetchData(manifest, m_currentSegment);
}

void
Consumer::onUnversionedData(const Interest& interest, const Data& data)
{
  fetchNextData();
  readData(data);
}

void
Consumer::readData(const Data& data)
{
  const Block& content = data.getContent();
  m_os.write(reinterpret_cast<const char*>(content.value()), content.value_size());
  m_totalSize += content.value_size();
  if (m_verbose)
  {
    std::cerr << "LOG: received data = " << data.getName() << std::endl;
  }
  if (m_isFinished) {
    std::cerr << "INFO: End of file is reached." << std::endl;
    std::cerr << "INFO: Total # of segments received: " << m_currentSegment  << std::endl;
    std::cerr << "INFO: Total # bytes of content received: " << m_totalSize << std::endl;
  }
}

void
Consumer::fetchNextData()
{
  if (m_currentSegment >= m_finalBlockId) {
    m_isFinished = true;
  }
  else
  {
    // Reset retry counter
    m_retryCount = 0;
    m_currentSegment += 1;
    fetchData(m_manifest, m_currentSegment);
  }
}

void
Consumer::onTimeout(const Interest& interest)
{
  if (m_retryCount++ < MAX_RETRY)
    {
      // Retransmit the interest
      fetchData(m_manifest, m_currentSegment);
      if (m_verbose)
        {
          std::cerr << "TIMEOUT: retransmit interest for " << interest.getName() << std::endl;
        }
    }
  else
    {
      std::cerr << "TIMEOUT: last interest sent for segment #" << (m_currentSegment) << std::endl;
      std::cerr << "TIMEOUT: abort fetching after " << MAX_RETRY
                << " times of retry" << std::endl;
    }
}


int
usage(const std::string& filename)
{
  std::cerr << "Usage: \n    "
            << filename << " [-v] [-s] [-u] [-l lifetime] [-w timeout] [-o filename] ndn-name\n\n"
            << "-v: be verbose\n"
            << "-s: only get single data packet\n"
            << "-u: versioned: ndn-name contains version component\n"
            << "    if -u is not specified, this command will return the rightmost child for the prefix\n"
            << "-l: InterestLifetime in milliseconds\n"
            << "-w: timeout in milliseconds for whole process (default unlimited)\n"
            << "-o: write to local file name instead of stdout\n"
            << "ndn-name: NDN Name prefix for Data to be read\n";
  return 1;
}


int
main(int argc, char** argv)
{
  std::string name;
  const char* outputFile = 0;
  bool verbose = false, versioned = false, single = false;
  int interestLifetime = 4000;  // in milliseconds
  int timeout = 0;  // in milliseconds

  int opt;
  while ((opt = getopt(argc, argv, "vsul:w:o:")) != -1)
    {
      switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 's':
        single = true;
        break;
      case 'u':
        versioned = true;
        break;
      case 'l':
        try
          {
            interestLifetime = boost::lexical_cast<int>(optarg);
          }
        catch (const boost::bad_lexical_cast&)
          {
            std::cerr << "ERROR: -l option should be an integer." << std::endl;
            return 1;
          }
        interestLifetime = std::max(interestLifetime, 0);
        break;
      case 'w':
        try
          {
            timeout = boost::lexical_cast<int>(optarg);
          }
        catch (const boost::bad_lexical_cast&)
          {
            std::cerr << "ERROR: -w option should be an integer." << std::endl;
            return 1;
          }
        timeout = std::max(timeout, 0);
        break;
      case 'o':
        outputFile = optarg;
        break;
      default:
        return usage(argv[0]);
      }
    }

  if (optind < argc)
    {
      name = argv[optind];
    }

  if (name.empty())
    {
      return usage(argv[0]);
    }

  std::streambuf* buf;
  std::ofstream of;

  if (outputFile != 0)
    {
      of.open(outputFile, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!of || !of.is_open()) {
        std::cerr << "ERROR: cannot open " << outputFile << std::endl;
        return 1;
      }
      buf = of.rdbuf();
    }
  else
    {
      buf = std::cout.rdbuf();
    }

  std::ostream os(buf);

  Consumer consumer(name, os, verbose, interestLifetime, timeout);

  try
    {
      consumer.run();
    }
  catch (const std::exception& e)
    {
      std::cerr << "ERROR: " << e.what() << std::endl;
    }

  return 0;
}

} // namespace repo

int
main(int argc, char** argv)
{
  return repo::main(argc, argv);
}
