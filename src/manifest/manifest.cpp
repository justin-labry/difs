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
  sha1.process_bytes(m_name.c_str(), m_name.length());
  sha1.get_digest(hashBlock);

  for (int i = 0; i < 5; i += 1) {
    result += str(boost::format("%08x") % hashBlock[i]);
  }

  return result;
}

std::string
Manifest::toJson()
{
  if (m_hash.empty()) {
    m_hash = makeHash();
  }
  namespace pt = boost::property_tree;

  pt::ptree root;
  root.put("name", m_name);
  root.put("hash", m_hash);
  root.put("repo", m_repo);

  root.put("segments.start", m_startBlockId);
  root.put("segments.end", m_endBlockId);

  std::stringstream os;
  pt::write_json(os, root);

  return os.str();
}

} // namespace repo
// vim: cino=g0,N-s,+0 sw=2
