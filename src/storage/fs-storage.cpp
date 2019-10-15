#include <istream>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>

#include "fs-storage.hpp"
#include "config.hpp"
#include "index.hpp"


#include <boost/format.hpp>
#include <boost/uuid/sha1.hpp>
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

int64_t
FsStorage::hash(std::string const& key)
{
  uint64_t result = 12345;
  for (auto current = key.begin(); current != key.end(); current += 1) {
    result = 127 * result + static_cast<unsigned char>(*current);
  }
  return result;
}

std::string
FsStorage::sha1Hash(std::string const& key)
{
  std::string result;
  boost::uuids::detail::sha1 sha1;
  unsigned hashBlock[5] = {0};
  sha1.process_bytes(key.c_str(), key.size());
  sha1.get_digest(hashBlock);

  for (int i = 0; i < 5; i += 1) {
    result += str(boost::format("%08x") % hashBlock[i]);
  }

  return result;
}

boost::filesystem::path
FsStorage::getPath(const Name& name, const char* dataType)
{
  return m_path / dataType / sha1Hash(name.toUri());
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
      if (!boost::filesystem::create_directories(boost::filesystem::path(fsPath))) {
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
  // FIXME: Use id from hash
  return writeData(data, DIRNAME_MANIFEST);
}

int64_t
FsStorage::writeData(const Data& data, const char* dataType)
{
  auto id = hash(data.getFullName().toUri());

  Index::Entry entry(data, 0);

  boost::filesystem::path fsPath = getPath(data.getFullName(), dataType);
  boost::filesystem::create_directory(fsPath);

  std::ofstream outFileName((fsPath / FNAME_NAME).string(), std::ios::binary);
  outFileName.write(
      reinterpret_cast<const char*>(entry.getName().wireEncode().wire()),
      entry.getName().wireEncode().size());

  std::ofstream outFileData((fsPath / FNAME_DATA).string(), std::ios::binary);
  outFileData.write(
      reinterpret_cast<const char*>(data.wireEncode().wire()),
      data.wireEncode().size());

  std::ofstream outFileLocator((fsPath / FNAME_HASH).string(), std::ios::binary);
  outFileLocator.write(
      reinterpret_cast<const char*>(entry.getKeyLocatorHash()->data()),
      entry.getKeyLocatorHash()->size());

  return (int64_t)id;
}

bool
FsStorage::erase(const Name& name)
{
  auto fsPath = getPath(name, DIRNAME_DATA);

  boost::filesystem::file_status fsPathStatus = boost::filesystem::status(fsPath);
  if (!boost::filesystem::is_directory(fsPathStatus)) {
    std::cerr << name.toUri() << " is not exists" << std::endl;
    return false;
  }

  boost::filesystem::remove_all(fsPath);
  return true;
}

std::shared_ptr<Data>
FsStorage::read(const Name& name)
{
  auto fsPath = getPath(name.toUri(), DIRNAME_DATA);
  auto data = make_shared<Data>();

  boost::filesystem::ifstream inFileData(fsPath / FNAME_DATA, std::ifstream::binary);
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

    boost::filesystem::ifstream isName(p / FNAME_NAME, std::ifstream::binary);
    isName.seekg(0, isName.end);
    int lengthName = isName.tellg();
    isName.seekg(0, isName.beg);

    char * bufferName = new char [lengthName];
    isName.read(bufferName, lengthName);
    item.fullName.wireDecode(Block(
          reinterpret_cast<const uint8_t*>(bufferName),
          lengthName));

    item.id = hash(item.fullName.toUri());

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
