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
  auto hash = sha1Hash(name.toUri());
  auto dir1 = hash.substr(0, 2);
  auto dir2 = hash.substr(2, hash.size() - 2);
  return m_path / dataType / dir1 / dir2;
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

std::string
FsStorage::insertManifest(const Manifest& manifest)
{
  boost::filesystem::path fsPath = m_path / DIRNAME_MANIFEST / manifest.getHash();

  auto json = manifest.toJson();

  std::ofstream outFile(fsPath.string());
  outFile.write(
      json.c_str(),
      json.size());

  return manifest.getHash();
}

Manifest
FsStorage::readManifest(const std::string& hash)
{
  boost::filesystem::path fsPath = m_path / DIRNAME_MANIFEST / hash;
  boost::filesystem::ifstream inFileData(fsPath);

  //FIXME: check exists
  if (!inFileData.is_open()) {
    throw NotFoundError("File '" + fsPath.string() + "' does not exists");
  }

  std::string json(
      (std::istreambuf_iterator<char>(inFileData)),
      std::istreambuf_iterator<char>());

  return Manifest::fromJson(json);
}

bool
FsStorage::eraseManifest(const std::string& hash)
{
  boost::filesystem::path fsPath = m_path / DIRNAME_MANIFEST / hash;

  boost::filesystem::file_status fsPathStatus = boost::filesystem::status(fsPath);
  if (!boost::filesystem::exists(fsPathStatus)) {
    std::cerr << hash << " is not exists (" << fsPath << ")" << std::endl;
    return false;
  }

  boost::filesystem::remove_all(fsPath);
  return true;
}

int64_t
FsStorage::writeData(const Data& data, const char* dataType)
{
  auto name = data.getName();
  std::cout << "Saving... " << name.toUri() << std::endl;
  auto id = hash(name.toUri());

  Index::Entry entry(data, 0);

  boost::filesystem::path fsPath = getPath(data.getName(), dataType);
  boost::filesystem::create_directories(fsPath.parent_path());

  std::ofstream outFileData(fsPath.string(), std::ios::binary);
  outFileData.write(
      reinterpret_cast<const char*>(data.wireEncode().wire()),
      data.wireEncode().size());

  return (int64_t)id;
}

bool
FsStorage::erase(const Name& name)
{
  auto fsPath = getPath(name, DIRNAME_DATA);

  boost::filesystem::file_status fsPathStatus = boost::filesystem::status(fsPath);
  if (!boost::filesystem::exists(fsPathStatus)) {
    std::cerr << name.toUri() << " is not exists (" << fsPath << ")" << std::endl;
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

  boost::filesystem::ifstream inFileData(fsPath, std::ifstream::binary);
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

    boost::filesystem::ifstream isName(p, std::ifstream::binary);
    isName.seekg(0, isName.end);
    int lengthName = isName.tellg();
    isName.seekg(0, isName.beg);

    char * bufferName = new char [lengthName];
    isName.read(bufferName, lengthName);
    item.fullName.wireDecode(Block(
          reinterpret_cast<const uint8_t*>(bufferName),
          lengthName));

    item.id = hash(item.fullName.toUri());

    try {
      f(item);
    } catch (...) {
    }

  }
}

}  // namespace repo
// vim: cino=g0,N-s,+0
