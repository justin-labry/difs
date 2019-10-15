#ifndef REPO_MANIFEST_MANIFEST_HPP
#define REPO_MANIFEST_MANIFEST_HPP

#include <ndn-cxx/face.hpp>

namespace repo {

class Manifest
{
public:
  Manifest(std::string repo, std::string name, int startBlockId, int endBlockId)
  : m_repo(repo)
  , m_name(name)
  , m_startBlockId(startBlockId)
  , m_endBlockId(endBlockId)
  {}

public:

  std::string
  toJson();

  std::string
  toInfoJson();

  static Manifest
  fromInfoJson(std::string json);

  ndn::Name
  getManifestStorage(ndn::Name const& prefix, int clusterSize);

  std::string
  getRepo();

  std::string
  getName();

  std::string
  getHash();

  void
  setHash(std::string digest);

  int
  getStartBlockId();

  int
  getEndBlockId();

private:
  std::string m_repo;
  std::string m_name;
  std::string m_hash;

  int m_startBlockId;
  int m_endBlockId;

private:
  std::string
  makeHash();
};

} // namespace repo
#endif // REPO_MANIFEST_MANIFEST_HPP
// vim: cino=g0,N-s,+0,i-s sw=2
