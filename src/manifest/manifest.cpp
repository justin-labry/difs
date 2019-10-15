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
  Manifest manifest("", name, 0, segment);
  if (manifest.getHash() != hash) {
    std::cerr << "Hash mismatch" << std::endl;
  }
  manifest.setHash(hash);

  return manifest;
}

std::string
Manifest::toInfoJson()
{
  if (m_hash.empty()) {
    m_hash = makeHash();
  }

  namespace pt = boost::property_tree;

  pt::ptree root;
  root.put("name", m_name);
  root.put("hash", m_hash);
  root.put("segment", m_endBlockId);

  std::stringstream os;
  pt::write_json(os, root, false);

  return os.str();
}

std::string
Manifest::toJson()
{
  if (m_hash.empty()) {
    m_hash = makeHash();
  }
  namespace pt = boost::property_tree;

  pt::ptree root;
  root.put("info.name", m_name);
  root.put("info.hash", m_hash);

  if (!m_repo.empty()) {
    pt::ptree children;

    pt::ptree storage;
    storage.put("storage_name", m_repo);
    storage.put("segment.start", m_startBlockId);
    storage.put("segment.end", m_endBlockId);

    children.push_back(std::make_pair("", storage));

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

std::string
Manifest::getRepo()
{
  return m_repo;
}

std::string
Manifest::getName()
{
  return m_name;
}

std::string
Manifest::getHash()
{
  if (m_hash.empty()) {
    m_hash = makeHash();
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
