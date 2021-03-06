#ifndef Pds_Eb_EbContributor_hh
#define Pds_Eb_EbContributor_hh

#include "psdaq/eb/eb.hh"

#include "psdaq/eb/BatchManager.hh"

#include "psdaq/service/Histogram.hh"

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
#include <atomic>


namespace XtcData {
  class Dgram;
  class TimeStmp;
};

namespace Pds {
  namespace Eb {

    // Put these in our namespace so as not to break others
    using TimePoint_t = std::chrono::steady_clock::time_point;
    using Duration_t  = std::chrono::steady_clock::duration;
    using us_t        = std::chrono::microseconds;
    using ns_t        = std::chrono::nanoseconds;

    class EbLfLink;
    class EbLfClient;
    class EbCtrbInBase;
    class Batch;
    class StatsMonitor;

    using EbLfLinkMap = std::unordered_map<unsigned, Pds::Eb::EbLfLink*>;

    class EbContributor : public BatchManager
    {
    public:
      EbContributor(const EbCtrbParams&);
      virtual ~EbContributor();
    public:
      void     startup(EbCtrbInBase&);
      void     shutdown();
    public:
      bool     process(const XtcData::Dgram* datagram, const void* appPrm);
    public:                             // For BatchManager
      virtual void post(const Batch* input);
      virtual void post(const XtcData::Dgram* nonEvent);
    public:
      const uint64_t& batchCount()  const { return _batchCount;  }
      unsigned        inFlightCnt() const { return _inFlightOcc; }
    private:
      void    _receiver(EbCtrbInBase&);
      void    _updateHists(TimePoint_t               t0,
                           TimePoint_t               t1,
                           const XtcData::TimeStamp& stamp);
    private:
      EbLfClient*            _transport;
      //std::vector<EbLfLink*> _links;
      EbLfLinkMap            _links;
      unsigned*              _idx2Id;
      const unsigned         _id;
      const unsigned         _numEbs;
    private:
      uint64_t               _batchCount;
    private:
      std::atomic<unsigned>  _inFlightOcc;
      Histogram              _inFlightHist;
      Histogram              _depTimeHist;
      Histogram              _postTimeHist;
      Histogram              _postCallHist;
      TimePoint_t            _postPrevTime;
    private:
      std::atomic<bool>      _running;
      std::thread*           _rcvrThread;
    protected:
      const EbCtrbParams&    _prms;
    };
  };
};

#endif
