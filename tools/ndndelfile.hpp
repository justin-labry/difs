#ifndef REPO_NG_TOOLS_NDNDELFILE_HPP
#define REPO_NG_TOOLS_NDNDELFILE_HPP

#include <ndn-cxx/security/command-interest-signer.hpp>

#include "../src/repo-command-parameter.hpp"
#include <ndn-cxx/face.hpp>

namespace repo {

class NdnDelFile : boost::noncopyable
{
public:
  NdnDelFile(const std::string& dataName,
    bool verbose, bool versioned, bool single,
    int interestLifetime, int timeout,
    bool mustBeFresh = false)
  : m_dataName(dataName)
    , m_verbose(verbose)
    , m_hasVersion(versioned)
    , m_isSingle(single)
    , m_isFinished(false)
    , m_isFirst(true)
    , m_interestLifetime(interestLifetime)
    , m_timeout(timeout)
    , m_nextSegment(0)
    , m_totalSize(0)
    , m_retryCount(0)
    , m_mustBeFresh(mustBeFresh)
    , m_cmdSigner(m_keyChain)
  {}

  void
  run();

private:
  void
  fetchData(const ndn::Name& name);

  void
  onVersionedData(const ndn::Interest& interest, const ndn::Data& data);

  void
  onUnversionedData(const ndn::Interest& interest, const ndn::Data& data);

  void
  onTimeout(const ndn::Interest& interest);

  void
  deleteData(const ndn::Data& data);

  void
  fetchNextData(const ndn::Name& name, const ndn::Data& data);

  ndn::Interest
  generateCommandInterest(const ndn::Name& commandPrefix, const std::string& command,
    const RepoCommandParameter& commandParameter);

  void
  onDeleteCommandResponse(const ndn::Interest& interest, const ndn::Data& data);

  void
  onDeleteCommandTimeout(const ndn::Interest& interest);

private:

  ndn::Face m_face;
  ndn::Name m_dataName;
  bool m_verbose;
  bool m_hasVersion;
  bool m_isSingle;
  bool m_isFinished;
  bool m_isFirst;
  ndn::time::milliseconds m_interestLifetime;
  ndn::time::milliseconds m_timeout;
  uint64_t m_nextSegment;
  int m_totalSize;
  int m_retryCount;
  bool m_mustBeFresh;

  ndn::KeyChain m_keyChain;
  ndn::security::CommandInterestSigner m_cmdSigner;
};

}  // namespace repo

#endif  // REPO_NG_TOOLS_NDNDELFILE_HPP
// vim: cino=g0,N-s,+0,(s,m1,t0 sw=2
