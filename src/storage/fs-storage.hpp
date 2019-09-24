#ifndef REPO_STORAGE_FS_HPP
#define REPO_STORAGE_FS_HPP

#include "storage.hpp"
#include "index.hpp"
#include <string>
#include <iostream>

namespace repo {

class FsStorage : public Storage
{
public:
  class Error : public std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
    : std::runtime_error(what)
    {}
  };

  explicit
  FsStorage(const std::string& dbPath);

  virtual
  ~FsStorage();

  /**
   *  @brief  put the data into database
   *  @param  data     the data should be inserted into databse
   *  @return int64_t  the id number of each entry in the database
   */
  virtual int64_t
  insert(const Data& data);

  virtual int64_t
  insertManifest(const Data& data);

  virtual bool
  erase(const Name& name);

  virtual std::shared_ptr<Data>
  read(const Name& name);

  /**
   *  @brief  return the size of database
   */
  virtual int64_t
  size();

  /**
   *  @brief enumerate each entry in database and call the function
   *         insertItemToIndex to reubuild index from database
   */
  void
  fullEnumerate(const std::function<void(const Storage::ItemMeta)>& f);

private:
  uint64_t
  hash(std::string const& key);

  std::string
  sha1Hash(std::string const& key);

  int64_t
  writeData(const Data& data, const char* dataType);

private:
  std::string m_dbPath;
  boost::filesystem::path m_path;

  static const char* FNAME_NAME;
  static const char* FNAME_DATA;
  static const char* FNAME_HASH;
  static const char* DIRNAME_DATA;
  static const char* DIRNAME_MANIFEST;
};

}  // namespace repo

#endif  // REPO_STORAGE_FS_HPPP#endif  // REPO_STORAGE_FS_HPPP#endif  // REPO_STORAGE_FS_HPPP
// vim: cino=g0,N-s,+0
