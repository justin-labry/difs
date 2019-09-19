#include "../src/repo-command-parameter.hpp"
#include "../src/repo-command-response.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/command-interest-signer.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/selectors.hpp>

#include "ndndelfile.hpp"

#include <iostream>

#include <boost/lexical_cast.hpp>

namespace repo {

using ndn::Name;
using ndn::Interest;
using ndn::Data;
using ndn::Block;

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;

static const int MAX_RETRY = 3;

void NdnDelFile::fetchData(const Name& name)
{
  Interest interest(name);
  interest.setInterestLifetime(m_interestLifetime);

  if (m_hasVersion) {
    // pass
  } else {
    interest.setChildSelector(1);
  }

  std::cout << "Fetching " << name << std::endl;

  m_face.expressInterest(interest,
    m_hasVersion ?
    bind(&NdnDelFile::onVersionedData, this, _1, _2)
    :
    bind(&NdnDelFile::onUnversionedData, this, _1, _2),
    bind(&NdnDelFile::onTimeout, this, _1),  // onNack
    bind(&NdnDelFile::onTimeout, this, _1)
  );
}


void
NdnDelFile::run()
{
  Name name(m_dataName);

  m_nextSegment += 1;
  fetchData(name);

  m_face.processEvents(m_timeout);
}

void
NdnDelFile::onVersionedData(const Interest& interest, const Data& data)
{
  const Name& name = data.getName();

  std::cout << "name: " << name << ", dataName: " << m_dataName << std::endl;

  const ndn::name::Component& finalBlockId = data.getMetaInfo().getFinalBlockId();
  std::cout << "versioned final block id: " << finalBlockId << std::endl;

  // the received data name may have segment number or not
  if (name.size() == m_dataName.size()) {
    if (!m_isSingle) {
      Name delName = name;
      delName.appendSegment(0);
      fetchData(delName);
    }
  } else if (name.size() == m_dataName.size() + 1) {
    if (m_isSingle) {
      std::cerr << "ERROR: Data is not stored in a single packet" << std::endl;
      return;
    }

    //TODO: send delete command interest

  } else {
    std::cerr << "ERROR: Name size does not match" << std::endl;
    return;
  }

  deleteData(data);
}

void
NdnDelFile::onUnversionedData(const Interest& interest, const Data& data)
{
  const Name& name = data.getName();

  const ndn::name::Component& finalBlockId = data.getMetaInfo().getFinalBlockId();
  std::cout << "unversioned final block id: " << finalBlockId << std::endl;

  if (name.size() == m_dataName.size() + 1) {
    if (!m_isSingle) {
      Name fetchName = name;
      fetchName.append(name[-1]).appendSegment(0);
      fetchData(fetchName);
    }
  } else if (name.size() == m_dataName.size() + 2) {  // segmented
    if (m_isSingle) {
      std::cerr << "ERROR: Data is not stored in a single packet" << std::endl;
      return;
    }

    // TODO: send delete command interest
  } else {
    std::cerr << "ERROR: Name size does not match" << std::endl;
    return;
  }

  deleteData(data);
}

void
NdnDelFile::deleteData(const Data& data)
{
  // TODO Implement this
  // See ndnputfile

  std::cout << "Delete " << m_dataName << std::endl;


  RepoCommandParameter parameters;
  parameters.setProcessId(0);  // FIXME: set process id properly
  if (m_isSingle) {
    parameters.setName(m_dataName);
  }
  if (!m_isSingle) {
    Name prefix(m_dataName.append(data.getName()[-2]));
    parameters.setName(prefix);
    const ndn::name::Component& finalBlockId = data.getMetaInfo().getFinalBlockId();
    if (!finalBlockId.empty()) {
      parameters.setEndBlockId(finalBlockId.toNumber());
    }
  }

  // TODO: send delete command interest
  ndn::Interest commandInterest = generateCommandInterest(m_repoPrefix, "delete", parameters);
  m_face.expressInterest(commandInterest,
    bind(&NdnDelFile::onDeleteCommandResponse, this, _1, _2),
    bind(&NdnDelFile::onDeleteCommandTimeout, this, _1),  // Nack
    bind(&NdnDelFile::onDeleteCommandTimeout, this, _1));
}

void
NdnDelFile::onTimeout(const Interest& interest)
{
  if (m_retryCount++ < MAX_RETRY) {
    fetchData(interest.getName());
    if (m_verbose) {
      std::cerr << "TIMEOUT: retransmit interest for " << interest.getName() << std::endl;
    }
  } else {
    std::cerr << "TIMEOUT: last interest sent for segment #" << (m_nextSegment - 1) << std::endl
    << "TIMEOUT: abort fetching after " << MAX_RETRY << " times of retry" << std::endl;
  }
}


ndn::Interest
NdnDelFile::generateCommandInterest(const ndn::Name& commandPrefix, const std::string& command,
                                    const RepoCommandParameter& commandParameter)
{
  Name cmd = commandPrefix;
  cmd
    .append(command)
    .append(commandParameter.wireEncode());
  ndn::Interest interest;

  interest = m_cmdSigner.makeCommandInterest(cmd);

  interest.setInterestLifetime(m_interestLifetime);
  return interest;
}

void
NdnDelFile::onDeleteCommandResponse(const ndn::Interest& interest, const ndn::Data& data)
{
  RepoCommandResponse response(data.getContent().blockFromValue());
  int statusCode = response.getStatusCode();
  if (statusCode >= 400) {
    std::cerr << "delete command failed with code " << statusCode << interest.getName() << std::endl;
    return;
  }
}

void
NdnDelFile::onDeleteCommandTimeout(const ndn::Interest& interest)
{
  std::cerr << "ERROR: timeout while deleting " << interest.getName() << std::endl;
}

int
usage(const std::string& filename)
{
  std::cerr << "Usage: \n    "
            << filename << " [-v] [-s] [-u] [-l lifetime] [-w timeout] repo-name ndn-name\n\n"
            << "-v: be verbose\n"
            << "-s: only get single data packet\n"
            << "-u: versioned: ndn-name contains version component\n"
            << "    if -u is not specified, this command will return the rightmost child for the prefix\n"
            << "-l: InterestLifetime in milliseconds\n"
            << "-w: timeout in milliseconds for whole process (default unlimited)\n"
            << "repo-prefix: repo command prefix\n"
            << "ndn-name: NDN Name prefix for Data to be read\n";
  return 1;
}


int
main(int argc, char** argv)
{
  std::string repoPrefix;
  std::string name;
  bool verbose = false, versioned = false, single = false;
  int interestLifetime = 4000;  // in milliseconds
  int timeout = 0;  // in milliseconds

  int opt;
  while ((opt = getopt(argc, argv, "vsul:w:o:")) != -1)
  {
    switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 's':
        single = true;
        break;
      case 'u':
        versioned = true;
        break;
      case 'l':
        try
        {
          interestLifetime = boost::lexical_cast<int>(optarg);
        }
        catch (const boost::bad_lexical_cast&)
        {
          std::cerr << "ERROR: -l option should be an integer." << std::endl;
          return 1;
        }
        interestLifetime = std::max(interestLifetime, 0);
        break;
      case 'w':
        try
        {
          timeout = boost::lexical_cast<int>(optarg);
        }
        catch (const boost::bad_lexical_cast&)
        {
          std::cerr << "ERROR: -w option should be an integer." << std::endl;
          return 1;
        }
        timeout = std::max(timeout, 0);
        break;
      default:
        return usage(argv[0]);
    }
  }

  if (optind + 2 != argc) {
    return usage(argv[0]);
  }

  repoPrefix = argv[optind];
  name = argv[optind+1];

  if (name.empty() || repoPrefix.empty())
  {
    return usage(argv[0]);
  }

  NdnDelFile ndnDelFile(repoPrefix, name, verbose, versioned, single,
    interestLifetime, timeout);

  try
  {
    ndnDelFile.run();
  }
  catch (const std::exception& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }

  return 0;
}

}  // namespace repo

int main(int argc, char** argv)
{
  return repo::main(argc, argv);
}
// vim: cino=g0,N-s,+0,(s,m1,t0 sw=2
