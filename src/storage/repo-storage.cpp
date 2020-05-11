/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2018, Regents of the University of California.
 *
 * This file is part of NDN repo-ng (Next generation of NDN repository).
 * See AUTHORS.md for complete list of repo-ng authors and contributors.
 *
 * repo-ng is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * repo-ng is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * repo-ng, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "repo-storage.hpp"
#include "config.hpp"

#include <istream>

#include <ndn-cxx/util/logger.hpp>

namespace repo {

NDN_LOG_INIT(repo.RepoStorage);

RepoStorage::RepoStorage(const int64_t& nMaxPackets, Storage& store)
  : m_index(nMaxPackets)
  , m_manifestIndex(nMaxPackets)
  , m_storage(store)
{
}

void
RepoStorage::initialize()
{
  NDN_LOG_DEBUG("Initialize");
  /* m_storage.fullEnumerate(bind(&RepoStorage::insertItemToIndex, this, _1)); */
}

void
RepoStorage::insertItemToIndex(const Storage::ItemMeta& item)
{
  NDN_LOG_DEBUG("Insert data to index " << item.fullName);
  /* m_index.insert(item.fullName, item.id, item.keyLocatorHash); */
  afterDataInsertion(item.fullName);
}

bool
RepoStorage::insertData(const Data& data)
{
   /* bool isExist = m_index.hasData(data); */
   /* if (isExist) */
   /*   BOOST_THROW_EXCEPTION(Error("The Entry Has Already In the Skiplist. Cannot be Inserted!")); */
   int64_t id = m_storage.insert(data);
   if (id == -1)
     return false;
   /* bool didInsert = m_index.insert(data, id); */
   /* if (didInsert) */
   /*   afterDataInsertion(data.getName()); */
   /* return didInsert; */
   return true;
}

bool
RepoStorage::insertManifest(const Manifest& manifest)
{
  bool isExist = m_manifestIndex.hasData(manifest);
  if (isExist)
    BOOST_THROW_EXCEPTION(Error("The entry manifest has already in the skiplist. Cannot be inserted!"));
  std::string hash = m_storage.insertManifest(manifest);
  if (hash == "")
    return false;
  bool didInsert = m_manifestIndex.insert(manifest, hash);
  if (didInsert)
    afterManifestInsertion(manifest.getHash());
  return didInsert;
}

Manifest
RepoStorage::readManifest(const std::string& hash)
{
  std::cout << "Read Manifest: " << hash << std::endl;
  Manifest manifest = m_storage.readManifest(hash);
  return manifest;
}

ssize_t
RepoStorage::deleteData(const Name& name)
{
  m_storage.erase(name);
  return 1;
}

ssize_t
RepoStorage::deleteData(const Interest& interest)
{
  Interest interestDelete = interest;
  interestDelete.setChildSelector(0);  //to disable the child selector in delete handle
  int64_t count = 0;
  bool hasError = false;
  std::pair<int64_t,ndn::Name> idName = m_index.find(interestDelete);
  while (idName.first != 0) {
    bool resultDb = m_storage.erase(idName.second);
    bool resultIndex = m_index.erase(idName.second); //full name
    if (resultDb && resultIndex) {
      afterDataDeletion(idName.second);
      count++;
    }
    else {
      hasError = true;
    }
    idName = m_index.find(interestDelete);
  }
  if (hasError)
    return -1;
  else
    return count;
}

shared_ptr<Data>
RepoStorage::readData(const ndn::Name& name) const
{
  shared_ptr<Data> data = m_storage.read(name);
  if (data) {
    return data;
  }
  return shared_ptr<Data>();
}


} // namespace repo
