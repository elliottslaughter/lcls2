#include "EbLfServer.hh"
#include "EbLfClient.hh"

#include "utilities.hh"

#include "xtcdata/xtc/TransitionId.hh"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <climits>
#include <atomic>
#include <string>


using namespace XtcData;
using namespace Pds;
using namespace Pds::Fabrics;
using namespace Pds::Eb;

static const unsigned max_servers      = 64;
static const unsigned max_clients      = 64;
static const unsigned port_base        = 54321; // Base port number
static const unsigned default_iters    = 1000;
static const unsigned default_id       = 1;
static const unsigned default_num_evts = 5;
static const unsigned default_buf_cnt  = 10;
static const unsigned default_buf_size = 4096;

static std::atomic<bool> running(true);


static size_t* _trSpace(size_t& size)
{
  const  size_t dgSz       = 52;
  static size_t trOffset[] =
    { [TransitionId::Unknown        ] = dgSz +  0, // Revisit sizes
      [TransitionId::Reset          ] = dgSz +  1 * sizeof(uint64_t),
      [TransitionId::Map            ] = dgSz +  2 * sizeof(uint64_t),
      [TransitionId::Unmap          ] = dgSz +  3 * sizeof(uint64_t),
      [TransitionId::Configure      ] = dgSz +  4 * sizeof(uint64_t),
      [TransitionId::Unconfigure    ] = dgSz +  5 * sizeof(uint64_t),
      [TransitionId::BeginRun       ] = dgSz +  6 * sizeof(uint64_t),
      [TransitionId::EndRun         ] = dgSz +  7 * sizeof(uint64_t),
      [TransitionId::BeginCalibCycle] = dgSz +  8 * sizeof(uint64_t),
      [TransitionId::EndCalibCycle  ] = dgSz +  9 * sizeof(uint64_t),
      [TransitionId::Enable         ] = dgSz + 10 * sizeof(uint64_t),
      [TransitionId::Disable        ] = dgSz + 11 * sizeof(uint64_t),
      [TransitionId::L1Accept       ] = dgSz +  0 };

  for (unsigned tr = 0; tr < TransitionId::NumberOf; ++tr)
  {
    size_t offset = trOffset[tr];
    trOffset[tr] = size;
    size += offset;
  }

  return trOffset;
}

int server(const char*        ifAddr,
           const std::string& srvPort,
           unsigned           id,
           unsigned           numClients,
           unsigned           numBuffers,
           size_t             bufSize)
{
  size_t      size     = roundUpSize(numBuffers * bufSize);
  size_t*     trOffset = _trSpace(size); // NB size is updated by this call
  void*       region   = allocRegion(numClients * size);
  EbLfServer* svr      = new EbLfServer(ifAddr, srvPort.c_str());
  const unsigned verbose(1);
  int         rc;

  std::vector<EbLfLink*> links(numClients);
  for (unsigned i = 0; i < links.size(); ++i)
  {
    const unsigned tmo(120000);
    if ( (rc = svr->connect(&links[i], tmo)) )
    {
      fprintf(stderr, "Error connecting to EbLfClient[%d]\n", i);
      return rc;
    }
    if ( (rc = links[i]->preparePender((char*)region + i * size, size, i, id, verbose)) < 0)
    {
      fprintf(stderr, "Failed to prepare link[%d]\n", i);
      return rc;
    }

    printf("EbLfClient[%d] (ID %d) connected\n", i, links[i]->id());
  }

  while (running)
  {
    uint64_t data;
    const int tmo = 5000;               // milliseconds
    if (svr->pend(&data, tmo) < 0)  continue;

    unsigned spc = ImmData::spc(data);
    unsigned src = ImmData::src(data);
    unsigned idx = ImmData::idx(data);
    void*    buf = links[src]->lclAdx(spc ? trOffset[idx] : idx * bufSize);

    links[src]->postCompRecv();

    uint64_t* b = (uint64_t*)buf;
    printf("Rcvd buf %p [%2d %2d %2d]: %2ld %2ld %2ld\n",
           buf, spc, src, idx, b[0], b[1], b[2]);
  }

  for (unsigned i = 0; i < links.size(); ++i)
  {
    svr->shutdown(links[i]);
  }
  delete svr;
  free(region);

  return 0;
}

int client(std::vector<std::string>& svrAddrs,
           std::vector<std::string>& svrPorts,
           unsigned                  id,
           unsigned                  numBuffers,
           size_t                    bufSize,
           unsigned                  iters,
           unsigned                  numEvents)
{
  size_t         size     = roundUpSize(numBuffers * bufSize);
  size_t*        trOffset = _trSpace(size); // NB size is updated by this call
  void*          region   = allocRegion(size);
  EbLfClient*    clt      = new EbLfClient();
  const unsigned tmo(120000);
  const unsigned verbose(1);
  int            rc;

  std::vector<EbLfLink*> links(svrAddrs.size());
  for (unsigned i = 0; i < links.size(); ++i)
  {
    const char* svrAddr = svrAddrs[i].c_str();
    const char* svrPort = svrPorts[i].c_str();

    if ( (rc = clt->connect(svrAddr, svrPort, tmo, &links[i])) )
    {
      fprintf(stderr, "Error connecting to EbLfServer[%d]\n", i);
      return rc;
    }
    if ( (rc = links[i]->preparePoster(region, size, i, id, verbose)) < 0)
    {
      fprintf(stderr, "Failed to prepare link[%d]\n", i);
      return rc;
    }

    printf("EbLfServer[%d] (ID %d) connected\n", i, links[i]->id());
  }

  unsigned cnt = 0;
  unsigned c   = 0;
  while (cnt++ < iters)
  {
    void* buf;
    for (int tr = TransitionId::Unknown; tr < TransitionId::L1Accept; tr += 2)
    {
      size_t offset = trOffset[tr];
      buf = (char*)region + offset;
      uint64_t* b = (uint64_t*)buf;
      *b++ = cnt;
      *b++ = tr;
      *b++ = c++;
      size_t size = (char*)b - (char*)buf;

      for (unsigned dst = 0; dst < links.size(); ++dst)
      {
        unsigned data = ImmData::transition(links[dst]->index(), tr);

        printf("Post %p, sz %zd to 0x%lx\n", buf, size, links[dst]->rmtAdx(offset));

        if ((rc = links[dst]->post(buf, size, offset, data)))
          fprintf(stderr, "Error %d posting transition %d to %d (ID %d)\n",
                  rc, tr, dst, links[dst]->id());
      }
    }

    buf = region;
    unsigned offset = 0;
    unsigned idx    = 0;
    for (unsigned l1a = 0; l1a < numEvents; ++l1a)
    {
      uint64_t* b = (uint64_t*)buf;
      *b++ = cnt;
      *b++ = l1a;
      *b++ = c++;
      size_t size = (char*)b - (char*)buf;

      for (unsigned dst = 0; dst < links.size(); ++dst)
      {
        int      rc;
        uint64_t data = ImmData::buffer(links[dst]->index(), idx);

        printf("Post %p, sz %zd to 0x%lx\n", buf, size, links[dst]->rmtAdx(offset));

        if ((rc = links[dst]->post(buf, size, offset, data)))
          fprintf(stderr, "Error %d posting L1Accept to %d (ID %d)\n",
                  rc, dst, links[dst]->id());
      }

      if (++idx < numBuffers)
      {
        buf     = (char*)buf + bufSize;
        offset += bufSize;
      }
      else
      {
        buf    = region;
        offset = 0;
        idx    = 0;
      }
    }

    for (int tr = TransitionId::Disable; tr >= TransitionId::Reset; tr -= 2)
    {
      size_t offset = trOffset[tr];
      buf = (char*)region + offset;
      uint64_t* b = (uint64_t*)buf;
      *b++ = cnt;
      *b++ = tr;
      *b++ = c++;
      size_t size = (char*)b - (char*)buf;

      for (unsigned dst = 0; dst < links.size(); ++dst)
      {
        unsigned data = ImmData::transition(links[dst]->index(), tr);

        printf("Post %p, sz %zd to 0x%lx\n", buf, size, links[dst]->rmtAdx(offset));

        if ((rc = links[dst]->post(buf, size, offset, data)))
          fprintf(stderr, "Error %d posting transition %d to %d (ID %d)\n",
                  rc, tr, dst, links[dst]->id());
      }
    }
  }

  for (unsigned dst = 0; dst < links.size(); ++dst)
  {
    clt->shutdown(links[dst]);
  }
  delete clt;

  return 0;
}


void sigHandler( int signal )
{
  static std::atomic<unsigned> callCount(0);

  printf("sigHandler() called: call count = %u\n", callCount.load());

  if (callCount == 0)
  {
    running = false;
  }

  if (callCount++)
  {
    fprintf(stderr, "Aborting on 2nd ^C...\n");
    ::abort();
  }
}

static void usage(char *name, char *desc)
{
  if (desc)
    fprintf(stderr, "%s\n\n", desc);

  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  %s [OPTIONS]\n", name);

  fprintf(stderr, "\nWhere:\n"
                  "  <spec>, below, has the form '<id>:<addr>:<port>', with\n");
  fprintf(stderr, "  <id> typically in the range 0 - 63.\n");

  fprintf(stderr, "\nOptions:\n");

  fprintf(stderr, " %-20s %s (default: %s)\n",      "-A <interface_addr>",
          "IP address of the interface to use",     "libfabric's 'best' choice");
  fprintf(stderr, " %-20s %s (default: %s)\n",      "-S \"<spec> [<spec> [...]]\"",
          "Servers to connect to",                  "None, must be provided for clients");
  fprintf(stderr, " %-20s %s (port: %d)\n",         "-P <port>",
          "Base port number",                       port_base);

  fprintf(stderr, " %-20s %s (default: %d)\n",      "-i <ID>",
          "Unique ID of this client (0 - 63)",      default_id);
  fprintf(stderr, " %-20s %s (default: %d)\n",      "-n <iters>",
          "Number of times to iterate",             default_iters);
  fprintf(stderr, " %-20s %s (default: %s)\n",      "-c <clients>",
          "Number of clients",                      "None, must be provided for servers");
  fprintf(stderr, " %-20s %s (default: %d)\n",      "-b <buffers>",
          "Number of buffers",                      default_buf_cnt);
  fprintf(stderr, " %-20s %s (default: %d)\n",      "-s <buffer size>",
          "Max buffer size",                        default_buf_size);

  fprintf(stderr, " %-20s %s\n", "-h", "display this help output");
}

static int parseSpec(char*                     spec,
                     unsigned                  maxId,
                     unsigned                  portMin,
                     unsigned                  portMax,
                     std::vector<std::string>& addrs,
                     std::vector<std::string>& ports,
                     uint64_t*                 bits)
{
  do
  {
    char* target = strsep(&spec, ",");
    if (!*target)  continue;
    char* colon1 = strchr(target, ':');
    char* colon2 = strrchr(target, ':');
    if (!colon1 || (colon1 == colon2))
    {
      fprintf(stderr, "Input '%s' is not of the form <ID>:<IP>:<port>\n", target);
      return 1;
    }
    unsigned id   = atoi(target);
    unsigned port = atoi(&colon2[1]);
    if (id > maxId)
    {
      fprintf(stderr, "ID %d is out of range 0 - %d\n", id, maxId);
      return 1;
    }
    if (port < portMax - portMin + 1)  port += portMin;
    if ((port < portMin) || (port > portMax))
    {
      fprintf(stderr, "Port %d is out of range %d - %d\n",
              port, portMin, portMax);
      return 1;
    }
    *bits |= 1ul << id;
    addrs.push_back(std::string(&colon1[1]).substr(0, colon2 - &colon1[1]));
    ports.push_back(std::string(std::to_string(port)));
  }
  while (spec);

  return 0;
}

int main(int argc, char **argv)
{
  int      op, rc   = 0;
  unsigned id       = default_id;
  char*    ifAddr   = nullptr;
  unsigned portBase = port_base;
  unsigned iters    = default_iters;
  unsigned nEvents  = default_num_evts;
  unsigned nClients = 0;
  unsigned nBuffers = default_buf_cnt;
  unsigned bufSize  = default_buf_size;
  char*    spec     = nullptr;

  while ((op = getopt(argc, argv, "h?A:S:P:i:n:N:c:b:s:")) != -1)
  {
    switch (op)
    {
      case 'A':  ifAddr   = optarg;        break;
      case 'S':  spec     = optarg;        break;
      case 'P':  portBase = atoi(optarg);  break;
      case 'i':  id       = atoi(optarg);  break;
      case 'n':  iters    = atoi(optarg);  break;
      case 'N':  nEvents  = atoi(optarg);  break;
      case 'c':  nClients = atoi(optarg);  break;
      case 'b':  nBuffers = atoi(optarg);  break;
      case 's':  bufSize  = atoi(optarg);  break;
      case '?':
      case 'h':
      default:
        usage(argv[0], (char*)"Test of EbLf links");
        return 1;
    }
  }

  if (id >= max_clients)
  {
    fprintf(stderr, "Client ID %d is out of range 0 - %d\n", id, max_clients - 1);
    return 1;
  }

  if ((portBase < port_base) || (portBase > port_base + max_clients))
  {
    fprintf(stderr, "Server port %d is out of range %d - %d\n",
            portBase, port_base, port_base + max_servers);
    return 1;
  }
  std::string srvPort(std::to_string(portBase));

  std::vector<std::string> cltAddrs;
  std::vector<std::string> cltPorts;
  uint64_t clients = 0;
  if (spec)
  {
    int rc = parseSpec(spec,
                       max_servers - 1,
                       port_base,
                       port_base + max_servers - 1,
                       cltAddrs,
                       cltPorts,
                       &clients);
    if (rc)  return rc;
  }

  ::signal( SIGINT, sigHandler );

  if (cltAddrs.size() == 0)
  {
    if (nClients == 0)
    {
      fprintf(stderr, "Number of clients (-c) must be provided\n");
      return 1;
    }

    rc = server(ifAddr, srvPort, id, nClients, nBuffers, bufSize);
  }
  else
  {
    if (spec == nullptr)
    {
      fprintf(stderr, "Server spec(s) (-S) must be provided\n");
      return 1;
    }

    rc = client(cltAddrs, cltPorts, id, nBuffers, bufSize, iters, nEvents);
  }

  return rc;
}
