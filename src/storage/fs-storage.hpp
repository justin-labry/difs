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

  /**
   *  @brief  remove the entry in the database by using id
   *  @param  id   id number of each entry in the database
   */
  virtual bool
  erase(const int64_t id);

  /**
   *  @brief  get the data from database
   *  @para   id   id number of each entry in the database, used to find the data
   */
  virtual std::shared_ptr<Data>
  read(const int64_t id);

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
  std::string m_dbPath;
  boost::filesystem::path m_path;
};

}  // namespace repo

#endif  // REPO_STORAGE_FS_HPPP#endif  // REPO_STORAGE_FS_HPPP#endif  // REPO_STORAGE_FS_HPPP
// vim: cino=g0,N-s,+0
