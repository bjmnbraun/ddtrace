#ifndef HWPERFCOUNTERS__H
#define HWPERFCOUNTERS__H

#include <sys/mman.h>

#include "Util.h"

/** 
  * USE_PERF_EVENT_OPEN is 1 if we use perf_event_open to open hardware
  * counters.
  *
  * If 0, we use our own kernel module (Module) which must be loaded before code
  * will work correctly (results undefined if Module hasn't been loaded). 
  * Note that our Module is not as powerful as perf_event_open - counts received
  * when PERF_EVENT_OPEN is 0 may include counts that occurred on other threads.
  * perf_event_open can give (and does give, in our use) counts that exclude all but the current thread.
  * This is more useful for analysis.
  */
#include "DDTraceConfig/USE_PERF_EVENT_OPEN.h"


//NOTE bottom-includes at bottom of file

namespace DDTrace {

/**
  * An enum of the counters themselves
  */ 
enum ConcreteCounterType {
    CYCLES = 0, //explicit numbers so we can debug
    L3_REFERENCE = 1, //=> L2 miss but hit L3
    L3_MISS = 2, //=> L3 miss
    /**
      * On some architectures, L2 line evictions are split into two categories: Clean
      * line evictions and dirty line evictions. We abstract this away by adding
      * the value of the two counters together
      *
      */
    L2_EVICTIONS_CLEAN = 3, 
    L2_EVICTIONS_DIRTY = 4
};


/**
  * This template is used to lookup the type and config arguments for
  * perf_event_open for each ConcreteCounterType. Both arguments are
  * architecture specific.
  * 
  * See also arch/XXX/arch/PerfCounters.h
  * 
  * Throws a runtime exception if a requested counter is unavailable
  * (Runtime exception because the selected counterType is chosen by the user at
  * runtime)
  */
template <ConcreteCounterType Counter> 
void getHWCounter(int* type, int* config)
{
    fprintf(stderr, "ConcreteCounterType %d not supported on this arch\n");
    throw std::runtime_error("ConcreteCounterType unsupported on arch");
}

/** 
  * A class representing a connection to a particular hardware counter. Uses the
  * perf_event_open system call to open the counter and the rdpmc call to actually query the counter
  */
class PerfEventCounter {
  public:
    /**
      * Opens this perf event counter to a particular counter, setting the
      * exclusion parameters of the counter (i.e. include hypervisor, etc.)
      *
      * Note that not all counters are available on all computers, and not all
      * exclusion modes are supported for all counters, and that access
      * privileges can afect access to certain counters.
      */
    template <ConcreteCounterType Counter>
    void init(bool exclude_kernel, 
              bool exclude_hv,
              bool exclude_guest, 
              bool exclude_idle){
#if USE_PERF_EVENT_OPEN == 1
        if (mmapPage){
            return; //Already initialized
        }
        //printf("Opening counter %d\n", Counter);
        //Initializing to 0 is very important 
        struct perf_event_attr perfAttributes;
        memset(&perfAttributes, 0, sizeof(perfAttributes));
        //alias
        struct perf_event_attr& pa = perfAttributes;
        pa.size = sizeof(struct perf_event_attr);
        pa.disabled = 1; //We initially disable 
        pa.exclude_kernel = exclude_kernel;
        pa.exclude_hv = exclude_hv;
        pa.exclude_guest = exclude_guest;
        pa.exclude_idle = exclude_idle;
        getHWCounter<Counter>(&hwCounterType, &hwCounterConfig);
        pa.type = hwCounterType;
        pa.config = hwCounterConfig;
        //printf("%d %x\n", hwCounterType, hwCounterConfig);

        /**
          * See documentation of perf_event_open
          */
        int fd = Util::perf_event_open(&perfAttributes, 0, -1, -1, 0);
        assert(fd != -1);
        if (!(fd != -1)){
            goto err;
        }
        //Now manually enable the counter
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        mmapPage = static_cast<struct perf_event_mmap_page*>( mmap(
            NULL,
            4096, //TODO(bjmnbraun@gmail.com) double check this
            PROT_READ,
            MAP_FILE | MAP_SHARED,
            fd,
            0) );
        assert(mmapPage != MAP_FAILED);
        if (!(mmapPage != MAP_FAILED)){
            goto err;
        }
        //Do a sample read to test the counter - some counters successfully
        //initialize but fail on first read (grr...)
        read();

        return;
        err:
        fprintf(stderr, "Could not open performance counter %d\n", Counter);
        throw new std::runtime_error("Could not open hardware performance counter\n");     
#else
        //XXX Hack when using the kernel module, we use getHWCounter's config
        //ret to determine the rdpmc index
        getHWCounter<Counter>(&hwCounterType, &hwCounterConfig);
#endif
    };

    /**
      * Reads the current value of the opened performance counter on the current
      * thread.
      */
    uint64_t read(){
#if USE_PERF_EVENT_OPEN == 1
        if (!mmapPage){
            assert(false);
            throw std::runtime_error("Use of uninitialized PerfEventCounter object");
        }
        //This is an abridged version of the perf_event_open documentation's
        //reccomended method for using the rdpmc interface.
        uint32_t seq, idx;
        int64_t pmc = 0;
        //Alias so that code matches example in documentation
        struct perf_event_mmap_page* pc = mmapPage;
        //Namespace using so that code matches example in documentation

        do {
            seq = pc->lock;
            Util::barrier();

            idx = pc->index;

            // Note: If the configuration was not done correctly, this line of
            // code will segfault because rdpmc is not allowed from userspace.
            pmc = Util::rdpmc(idx - 1);
            

            Util::barrier();
        } while (pc->lock != seq);
        return pmc;
#else
        return Util::rdpmc(hwCounterConfig);        
#endif
    }

    PerfEventCounter() : hwCounterType(), hwCounterConfig()
#if USE_PERF_EVENT_OPEN == 1
    , mmapPage(NULL) 
#endif
    {}
  private:
    int hwCounterType, hwCounterConfig;
#if USE_PERF_EVENT_OPEN == 1
    struct perf_event_mmap_page* mmapPage;
#endif
};

} //end DDTrace namespace

//BOTTOM INCLUDES
#include "DDTraceConfig/ARCH_HWPERFCOUNTERS.h"

#endif
