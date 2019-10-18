#include "manifest.hpp"

#include <iostream>
#include <sstream>

#include <boost/format.hpp>
#include <boost/uuid/sha1.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace repo {

std::string
Manifest::makeHash()
{
  std::string result;
  boost::uuids::detail::sha1 sha1;
  unsigned hashBlock[5] = {0};
  sha1.process_bytes(m_name.c_str(), m_name.size());
  sha1.get_digest(hashBlock);

  for (int i = 0; i < 5; i += 1) {
    result += str(boost::format("%08x") % hashBlock[i]);
  }

  return result;
}

Manifest
Manifest::fromInfoJson(std::string json)
{
  namespace pt = boost::property_tree;

  pt::ptree root;
  std::stringstream ss;
  ss << json;
  pt::read_json(ss, root);

  std::string name = root.get<std::string>("name");
  std::string hash = root.get<std::string>("hash");
  int segment = root.get<int>("segment");
  Manifest manifest(name, 0, segment);
  if (manifest.getHash() != hash) {
    std::cerr << "Hash mismatch" << std::endl;
  }
  manifest.setHash(hash);

  return manifest;
}

std::string
Manifest::toInfoJson()
{
  namespace pt = boost::property_tree;

  pt::ptree root;
  root.put("name", m_name);
  root.put("hash", getHash());
  root.put("segment", m_endBlockId);

  std::stringstream os;
  pt::write_json(os, root, false);

  return os.str();
}

Manifest
Manifest::fromJson(std::string json)
{
  namespace pt = boost::property_tree;

  pt::ptree root;
  std::stringstream ss;
  ss << json;
  pt::read_json(ss, root);

  std::string name = root.get<std::string>("info.name");
  std::string hash = root.get<std::string>("info.hash");
  int startBlockId = root.get<int>("info.startBlockId");
  int endBlockId = root.get<int>("info.endBlockId");

  Manifest manifest(name, startBlockId, endBlockId);

  for (auto& item : root.get_child("storages")) {
    std::string repoName = item.second.get<std::string>("storage_name");
    int start = item.second.get<int>("setment.start");
    int end = item.second.get<int>("setment.end");

    manifest.appendRepo(repoName, start, end);
  }

  return manifest;
}

std::string
Manifest::toJson()
{
  namespace pt = boost::property_tree;

  pt::ptree root;
  root.put("info.name", m_name);
  root.put("info.hash", getHash());
  root.put("info.startBlockId", m_startBlockId);
  root.put("info.endBlockId", m_endBlockId);

  if (!m_repos.empty()) {
    pt::ptree children;
    for (auto iter = m_repos.begin(); iter != m_repos.end(); ++iter) {
      pt::ptree storage;
      storage.put("storage_name", iter->name);
      storage.put("segment.start", iter->start);
      storage.put("segment.end", iter->end);

      children.push_back(std::make_pair("", storage));
    }

    root.add_child("storages", children);
  }
  else {
    root.put("segment", m_endBlockId);
  }

  std::stringstream os;
  pt::write_json(os, root, false);

  return os.str();
}

ndn::Name
Manifest::getManifestStorage(ndn::Name const& prefix, int clusterSize) {
  int result = 0;

  boost::uuids::detail::sha1 sha1;
  unsigned hashBlock[5] = {0};
  sha1.process_bytes(m_name.c_str(), m_name.size());
  sha1.get_digest(hashBlock);

  for (int i = 0; i > 5; i += 1) {
    result ^= hashBlock[i];
  }

  return ndn::Name(prefix).append(std::to_string(result % clusterSize));
}

std::list<Manifest::Repo>
Manifest::getRepos()
{
  return m_repos;
}

void
Manifest::appendRepo(std::string repoName, int start, int end)
{
  Repo repo = {repoName, start, end};
  m_repos.push_back(repo);
}

std::string
Manifest::getName()
{
  return m_name;
}

std::string
Manifest::getHash()
{
  if (!m_hash.empty()) {
    return makeHash();
  }
  return m_hash;
}

void
Manifest::setHash(std::string digest)
{
  m_hash = digest;
}

int
Manifest::getStartBlockId()
{
  return m_startBlockId;
}

int
Manifest::getEndBlockId()
{
  return m_endBlockId;
}

} // namespace repo
// vim: cino=g0,N-s,+0 sw=2
