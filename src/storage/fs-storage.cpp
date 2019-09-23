#include <istream>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>

#include "fs-storage.hpp"
#include "config.hpp"
#include "index.hpp"


#include <ndn-cxx/util/sha256.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

namespace repo {
using std::string;

const char* FsStorage::FNAME_NAME = "name";
const char* FsStorage::FNAME_DATA = "data";
const char* FsStorage::FNAME_HASH = "locatorHash";
const char* FsStorage::DIRNAME_DATA = "data";
const char* FsStorage::DIRNAME_MANIFEST = "manifest";

uint64_t
hash(std::string const& key)
{
  uint64_t result = 12345;
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
  boost::filesystem::create_directory(m_path / DIRNAME_DATA);
  boost::filesystem::create_directory(m_path / DIRNAME_MANIFEST);
}

FsStorage::~FsStorage() {}

int64_t
FsStorage::insert(const Data& data)
{
  return writeData(data, DIRNAME_DATA);
}

int64_t
FsStorage::insertManifest(const Data& data)
{
  return writeData(data, DIRNAME_MANIFEST);
}

int64_t
FsStorage::writeData(const Data& data, const char* dataType)
{
  uint64_t id = hash(data.getName().toUri());

  Index::Entry entry(data, 0);
  string name = data.getName().toUri();
  std::replace(name.begin(), name.end(), '/', '_');

  auto dirName = m_path / dataType / boost::lexical_cast<std::string>(id);
  boost::filesystem::create_directory(dirName);

  std::ofstream outFileName((dirName / FNAME_NAME).string(), std::ios::binary);
  outFileName.write(
      reinterpret_cast<const char*>(entry.getName().wireEncode().wire()),
      entry.getName().wireEncode().size());

  std::ofstream outFileData((dirName / FNAME_DATA).string(), std::ios::binary);
  outFileData.write(
      reinterpret_cast<const char*>(data.wireEncode().wire()),
      data.wireEncode().size());

  std::ofstream outFileLocator((dirName / FNAME_HASH).string(), std::ios::binary);
  outFileLocator.write(
      reinterpret_cast<const char*>(entry.getKeyLocatorHash()->data()),
      entry.getKeyLocatorHash()->size());

  return (int64_t)id;
}

bool
FsStorage::erase(const int64_t id)
{
  uint64_t id_unsigned = boost::lexical_cast<uint64_t>(id);
  boost::filesystem::path fsPath(m_path / DIRNAME_DATA / std::to_string(id_unsigned));

  boost::filesystem::file_status fsPathStatus = boost::filesystem::status(fsPath);
  if (!boost::filesystem::is_directory(fsPathStatus)) {
    std::cerr << id_unsigned << " is not exists" << std::endl;
    return false;
  }

  boost::filesystem::remove_all(fsPath);
  return true;
}

std::shared_ptr<Data>
FsStorage::read(const int64_t id)
{
  uint64_t id_unsigned = boost::lexical_cast<uint64_t>(id);
  auto dirName = m_path / DIRNAME_DATA / std::to_string(id_unsigned);
  auto data = make_shared<Data>();

  boost::filesystem::ifstream inFileData(dirName / FNAME_DATA, std::ifstream::binary);
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

  for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(m_path / DIRNAME_DATA), {})) {
    size += 1;
  }

  return size;
}

void
FsStorage::FsStorage::fullEnumerate(const std::function<void(const Storage::ItemMeta)>& f)
{
  for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(m_path / DIRNAME_DATA), {})) {

    boost::filesystem::path p = entry.path();

    ItemMeta item;
    item.id = boost::lexical_cast<uint64_t>(std::stoull(p.filename().string()));

    boost::filesystem::ifstream isName(p / FNAME_NAME, std::ifstream::binary);
    isName.seekg(0, isName.end);
    int lengthName = isName.tellg();
    isName.seekg(0, isName.beg);

    char * bufferName = new char [lengthName];
    isName.read(bufferName, lengthName);
    item.fullName.wireDecode(Block(
          reinterpret_cast<const uint8_t*>(bufferName),
          lengthName));

    boost::filesystem::ifstream isLocatorHash(p / FNAME_HASH, std::ifstream::binary);
    isLocatorHash.seekg(0, isLocatorHash.end);
    int lengthLocatorHash = isLocatorHash.tellg();
    isLocatorHash.seekg(0, isLocatorHash.beg);

    char * bufferLocator = new char [lengthLocatorHash];
    isLocatorHash.read(bufferLocator, lengthLocatorHash);
    item.keyLocatorHash = make_shared<const ndn::Buffer>(
        bufferLocator,
        lengthLocatorHash);

    try {
      f(item);
    } catch (...) {
    }

  }
}

}  // namespace repo
// vim: cino=g0,N-s,+0
