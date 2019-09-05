#include <istream>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>

#include "fs-storage.hpp"
#include "config.hpp"
#include "index.hpp"


#include <ndn-cxx/util/sha256.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

namespace repo {
using std::string;

int64_t
hash(std::string const& key)
{
  int64_t result = 12345;
  for (auto current = key.begin(); current != key.end(); current += 1) {
    result = 127 * result + static_cast<unsigned char>(*current);
  }
  return result;
}

FsStorage::FsStorage(const string& dbPath)
{
  if (dbPath.empty()) {
    std::cerr << "Create db path in local location [" << dbPath << "]. " << std::endl;
    m_dbPath = "ndn_repo";
  }
  else {
    boost::filesystem::path fsPath(dbPath);
    boost::filesystem::file_status fsPathStatus = boost::filesystem::status(fsPath);
    if (!boost::filesystem::is_directory(fsPathStatus)) {
      if (!boost::filesystem::create_directory(boost::filesystem::path(fsPath))) {
        BOOST_THROW_EXCEPTION(Error("Directory '" + dbPath + "' does not exists and cannot be created"));
      }
    }

    m_dbPath = dbPath;
  }
  m_path = boost::filesystem::path(m_dbPath);
}

FsStorage::~FsStorage() {}

int64_t
FsStorage::insert(const Data& data)
{

  int64_t id = hash(data.getName().toUri());

  Index::Entry entry(data, 0);
  string name = data.getName().toUri();
  std::replace(name.begin(), name.end(), '/', '_');

  auto dirName = m_path / std::to_string(id);
  boost::filesystem::create_directory(dirName);

  std::ofstream outFileName((dirName / "name").string(), std::ios::binary);
  outFileName.write(
      reinterpret_cast<const char*>(entry.getName().wireEncode().wire()),
      entry.getName().wireEncode().size());

  std::ofstream outFileData((dirName / "data").string(), std::ios::binary);
  outFileData.write(
      reinterpret_cast<const char*>(data.wireEncode().wire()),
      data.wireEncode().size());

  return id;
}

bool
FsStorage::erase(const int64_t id)
{
  boost::filesystem::remove_all(m_path / std::to_string(id));
  return true;
}

std::shared_ptr<Data>
FsStorage::read(const int64_t id)
{
  auto dirName = m_path / std::to_string(id);
  auto data = make_shared<Data>();

  boost::filesystem::ifstream inFileData(dirName / "data", std::ifstream::binary);
  inFileData.seekg(0, inFileData.end);
  int length = inFileData.tellg();
  inFileData.seekg(0, inFileData.beg);

  char * buffer = new char [length];
  inFileData.read(buffer, length);

  data->wireDecode(Block(reinterpret_cast<const uint8_t*>(buffer), length));

  return data;
}

int64_t
FsStorage::size()
{
  int64_t size = 0;

  for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(m_path), {})) {
    size += 1;
  }

  return size;
}

void
FsStorage::FsStorage::fullEnumerate(const std::function<void(const Storage::ItemMeta)>& f)
{
  // TODO: Implement this
}

}  // namespace repo
// vim: cino=g0,N-s,+0
