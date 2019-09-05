#include "fs-storage.hpp"
#include "config.hpp"
#include "index.hpp"

#include <ndn-cxx/util/sha256.hpp>
#include <boost/filesystem.hpp>
#include <istream>

namespace repo {
using std::string;

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
}

FsStorage::~FsStorage() {}

int64_t
FsStorage::insert(const Data& data)
{
  // TODO: Implement this
  return 0;
}

bool
FsStorage::erase(const int64_t id)
{
  // TODO: Implement this
  return false;
}

std::shared_ptr<Data>
FsStorage::read(const int64_t id)
{
  // TODO: Implement this
  return nullptr;
}

int64_t
FsStorage::size()
{
  // TODO: Implement this
  return 0;
}

void
FsStorage::FsStorage::fullEnumerate(const std::function<void(const Storage::ItemMeta)>& f)
{
  // TODO: Implement this
}

}  // namespace repo
// vim: cino=g0,N-s,+0
