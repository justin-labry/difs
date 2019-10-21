/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2017, Regents of the University of California.
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

#include "index-manifest.hpp"

#include <ndn-cxx/util/sha256.hpp>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>

namespace repo {

/** @brief determines if entry can satisfy interest
 *  @param hash SHA256 hash of PublisherPublicKeyLocator if exists in interest, otherwise ignored
 */
static bool
matchesSimpleSelectors(const Interest& interest, ndn::ConstBufferPtr& hash,
                       const IndexManifest::Entry& entry)
{
  const Name& fullName = entry.getName();

  if (!interest.getName().isPrefixOf(fullName))
    return false;

  size_t nSuffixComponents = fullName.size() - interest.getName().size();
  if (interest.getMinSuffixComponents() >= 0 &&
      nSuffixComponents < static_cast<size_t>(interest.getMinSuffixComponents()))
    return false;
  if (interest.getMaxSuffixComponents() >= 0 &&
      nSuffixComponents > static_cast<size_t>(interest.getMaxSuffixComponents()))
    return false;

  if (!interest.getExclude().empty() &&
      entry.getName().size() > interest.getName().size() &&
      interest.getExclude().isExcluded(entry.getName()[interest.getName().size()]))
    return false;
  return true;
}

IndexManifest::IndexManifest(size_t nMaxPackets)
  : m_maxPackets(nMaxPackets)
  , m_size(0)
{
}


bool
IndexManifest::insert(const Manifest& manifest, std::string hash)
{
  if (isFull())
    BOOST_THROW_EXCEPTION(Error("The Index is Full. Cannot Insert Any Data!"));
  Entry entry(manifest, hash);
  bool isInserted = m_indexContainer.insert(entry).second;
  if (isInserted)
    ++m_size;
  return isInserted;
}

std::pair<std::string,Name>
IndexManifest::find(const Interest& interest) const
{
  Name name = interest.getName();
  IndexContainer::const_iterator result = m_indexContainer.lower_bound(name);
  if (result != m_indexContainer.end())
    {
      return selectChild(interest, result);
    }
  else
    {
      return std::make_pair("", Name());
    }
}

std::pair<std::string,Name>
IndexManifest::find(const Name& name) const
{
  IndexContainer::const_iterator result = m_indexContainer.lower_bound(name);
  if (result != m_indexContainer.end())
    {
      return findFirstEntry(name, result);
    }
  else
    {
      return std::make_pair("", Name());
    }
}

bool
IndexManifest::hasData(const Manifest& manifest) const
{
  IndexManifest::Entry entry(manifest, manifest.getHash()); // the id number is useless
  IndexContainer::const_iterator result = m_indexContainer.find(entry);
  return result != m_indexContainer.end();

}

std::pair<std::string,Name>
IndexManifest::findFirstEntry(const Name& prefix,
                      IndexContainer::const_iterator startingPoint) const
{
  BOOST_ASSERT(startingPoint != m_indexContainer.end());
  if (prefix.isPrefixOf(startingPoint->getName()))
    {
      return std::make_pair(startingPoint->getHash(), startingPoint->getName());
    }
  else
    {
      return std::make_pair("", Name());
    }
}

bool
IndexManifest::erase(const Name& fullName)
{
  Entry entry(fullName);
  IndexContainer::const_iterator findIterator = m_indexContainer.find(entry);
  if (findIterator != m_indexContainer.end())
    {
      m_indexContainer.erase(findIterator);
      m_size--;
      return true;
    }
  else
    return false;
}

std::pair<std::string,Name>
IndexManifest::selectChild(const Interest& interest,
                   IndexContainer::const_iterator startingPoint) const
{
  BOOST_ASSERT(startingPoint != m_indexContainer.end());
  bool isLeftmost = (interest.getChildSelector() <= 0);
  ndn::ConstBufferPtr hash;

  if (isLeftmost)
    {
      for (IndexContainer::const_iterator it = startingPoint;
           it != m_indexContainer.end(); ++it)
        {
          if (!interest.getName().isPrefixOf(it->getName()))
            return std::make_pair("", Name());
          if (matchesSimpleSelectors(interest, hash, (*it)))
            return std::make_pair(it->getHash(), it->getName());
        }
    }
  else
    {
      IndexContainer::const_iterator boundary = m_indexContainer.lower_bound(interest.getName());
      if (boundary == m_indexContainer.end() || !interest.getName().isPrefixOf(boundary->getName()))
        return std::make_pair("", Name());
      Name successor = interest.getName().getSuccessor();
      IndexContainer::const_iterator last = interest.getName().size() == 0 ?
                    m_indexContainer.end() : m_indexContainer.lower_bound(interest.getName().getSuccessor());
      while (true)
        {
          IndexContainer::const_iterator prev = last;
          --prev;
          if (prev == boundary)
            {
              bool isMatch = matchesSimpleSelectors(interest, hash, (*prev));
              if (isMatch)
                {
                  return std::make_pair(prev->getHash(), prev->getName());
                }
              else
                return std::make_pair("", Name());
            }
          IndexContainer::const_iterator first =
            m_indexContainer.lower_bound(prev->getName().getPrefix(interest.getName().size() + 1));
          IndexContainer::const_iterator match =
                     std::find_if(first, last, bind(&matchesSimpleSelectors, interest, hash, _1));
          if (match != last)
            {
              return std::make_pair(match->getHash(), match->getName());
            }
          last = first;
        }
    }
  return std::make_pair("", Name());
}

IndexManifest::Entry::Entry(const Manifest& manifest, std::string hash)
  : m_name(manifest.getName())
  , m_hash(hash)
{
}

IndexManifest::Entry::Entry(const Name& name)
  : m_name(name)
{
}

} // namespace repo
