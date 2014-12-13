#ifndef PERFGRAPH__H
#define PERFGRAPH__H
//Configuration options

/**
  * RECORD_QUEUE_SIZE is the number of record elements to keep in the
  * in-memory record queue for the CustomAggregator to poll from.
  * 
  * After recording this many intervals, we begin dropping records until
  * the consumer catches up.
  */
#include "DDTraceConfig/RECORD_QUEUE_SIZE.h"

//Compilation options
/**
  * DEBUG_DROPPED_RECORDS is used to output a message whenever a record
  * is dropped because the aggregator is not running or has not polled
  * it fast enough
  */
#define DEBUG_DROPPED_RECORDS 0


//Actual includes
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "DDTrace/VectorClock.h"
#include "DDTrace/Cycles.h"
#include "DDTrace/Util.h"
#include "DDTrace/DataStructures.h"
#include "DDTrace/HWPerfCounters.h"

namespace DDTrace {

/**
  * Maximum nesting of intervals on a single thread.
  *
  * Set low because none of our use cases currently nest intervals on a single
  * thread.
  */
const size_t MAX_NESTED_INTERVALS = 4;

/**
  * Maximum threads being traced via DDTrace.
  */
const size_t MAX_THREADS = 64;

/**
 * These represent the combinations of simultaneous counters we currently support.
 * No combination exceeds 
 * MAX_COUNTERS_PER_COUNTERTYPE
 */
const size_t MAX_COUNTERS_PER_COUNTERTYPE = 1;

/**
  * Time interval, in microseconds, that the RecordSource will wait
  * before checking for new / dead worker threads
  */
const uint64_t CHECK_RECORDS_INTERVAL = 1000000;

/**
  * When round-robining around worker thread's records, stay on a
  * particular worker thread for at least this many record poll attempts
  */
const size_t SELECT_RECORDS_REUSE_COUNTER = 8;

enum CounterType {
    TIME_ONLY = 0,
    USERSPACE_CYCLES_ONLY,
    L3_REFERENCE_ONLY,
    L3_MISS_ONLY,
    INVALID_COUNTER_TYPE // This is a useful initialization value
};

const uint16_t INVALID_SERVER_ID = -1;

/**
  * An identifier for each thread being traced 
  * Either equal to THREAD_UNCLAIMED or has a value in [0,MAX_THREADS-1]
  */
typedef uint16_t ThreadId;
const ThreadId THREAD_UNCLAIMED = -1;

class RecordSink;
class RecordSource;
class RecordStorageUtils;
class PerfCounters;
class ThreadInitializer;
class SLARules;

extern CounterType counterType;

/**
  * A function which returns a 16-bit server identifier.
  */
extern uint16_t serverId;
//Thread local record sinks
extern RecordSink recordSinks[MAX_THREADS];
extern RecordSource recordSource;
extern ThreadInitializer threadInitializer;
extern PerfCounters perfCounters[MAX_THREADS];
extern SLARules slaRules;
extern bool initialized;
void init(CounterType type = INVALID_COUNTER_TYPE, 
          uint16_t serverId = INVALID_SERVER_ID);

/**
  * Must be called on every thread that wants to call recordSink functions
  */
void initThread();
void initThreadSink(const std::string& logName);
extern __thread ThreadId threadid; 
extern __thread bool threadInitialized;

/**
  * Describes a set of performance counter measurements
  */
class PerfRecord {
  friend class PerfCounters;
  public:
        /**
         * Extract the userspace cycles from this record.
         * Returns false if the CounterType does not support this measurement
         */
        bool getUserspaceCycles(uint64_t* value) const{
            switch (recordCounterType){
                case USERSPACE_CYCLES_ONLY:
                    *value = counters[0];
                    return true;
                default:
                    return false;
            }
        }

        /**
         * Extract the L2 cache misses from this record.
         * Returns false if the CounterType does not support this measurement
         */
        bool getL2Misses(uint64_t* value) const{
            switch (recordCounterType){
                default:
                    return false;
            }
        }

        /**
         * Extract the L3 cache misses from this record.
         * Returns false if the CounterType does not support this measurement
         */
        bool getL3Misses(uint64_t* value) const{
            switch (recordCounterType){
                case L3_MISS_ONLY:
                    *value = counters[0];
                    return true;
                default:
                    return false;
            }
        }

        /**
         * Extract the L3 cache references from this record.
         * Returns false if the CounterType does not support this measurement
         */
        bool getL3References(uint64_t* value) const{
            switch (recordCounterType){
                case L3_REFERENCE_ONLY:
                    *value = counters[0];
                    return true;
                default:
                    return false;
            }
        }

        PerfRecord() :
        counters{0},
        recordCounterType(INVALID_COUNTER_TYPE) {}
    private:
        /**
         * The value of the counters in this record
         */
        uint64_t counters[MAX_COUNTERS_PER_COUNTERTYPE];
        /**
          * The counterType used to generate this record
          * This has to be stored with each record because a
          * CustomAggregator may work with records of multiple
          * counterTypes simultaneously.
          */ 
        CounterType recordCounterType;
};

/**
 * This structure holds a self-describing record of an interval event while it
 * is buffered in memory.
 */
struct IntervalRecord {
  public:
    uint64_t getStartCycles() const {
        return startCycles;
    }
    uint64_t getEndCycles() const {
        return endCycles;
    }
    const VectorClock& getClock() const {
        return clock;
    }
    uint16_t getServerID() const {
        return serverId;
    }
    const PerfRecord& getCountersDiff() const {
        return countersDiff;
    }
    //TODO fill in the rest of the functions needed here. Maybe use macros to
    //make this easier to write?
    uint64_t getStartNanoseconds() const {        
        return (uint64_t) (1e09*static_cast<double>(startCycles)/cyclesPerSec + 0.5);
    }
    uint64_t getEndNanoseconds() const {        
        return (uint64_t) (1e09*static_cast<double>(endCycles)/cyclesPerSec + 0.5);
    }
    /**
      * Computes the number of nanoseconds that elapsed over this interval.
      */
    uint64_t getElapsedNanoseconds() const {        
        return (uint64_t) (1e09*static_cast<double>(endCycles - startCycles)/cyclesPerSec + 0.5);
    }
    /**
      * Computes the number of microseconds that elapsed over this interval.
      */
    uint64_t getElapsedMicroseconds() const { 
        return getElapsedNanoseconds() / 1000;
    }
    /**
      * Computes the number of seconds that elapsed over this interval.
      */
    double getElapsedSeconds() const {        
        return static_cast<double>(endCycles - startCycles)/cyclesPerSec;
    }

    IntervalRecord(
            const uint64_t& startCycles, 
            const uint64_t& endCycles, 
            const VectorClock& clock,
            const uint16_t& serverId,
            const double& cyclesPerSec,
            const PerfRecord& countersDiff) :
        startCycles(startCycles),
        endCycles(endCycles),
        clock(clock),
        serverId(serverId),
        cyclesPerSec(cyclesPerSec),
        countersDiff(countersDiff) {}

    /**
      * This constructor is necessary to support data structures holding 
      * IntervalRecords
      */ 
    IntervalRecord():
        startCycles(0),
        endCycles(0),
        clock(0),
        serverId(0),
        cyclesPerSec(0),
        countersDiff() {}

    /**
      * Assigning an IntervalRecord to another can be done with memcpy,
      * for example. This is needed for the semantics of SPSCStack
      */
//    struct is_trivially_copyable : public std::true_type {};
  private:
    uint64_t startCycles;
    uint64_t endCycles;
    VectorClock clock;
    uint16_t serverId;
    double cyclesPerSec; 
    PerfRecord countersDiff;
};

/**
 * Should be incremented whenever a change to the code is made that makes
 * the old RecordState data-incompatible with new ones
 */
inline const char* getRecordStateSchema(){
    return "4";
}

/*
 * State shared between the process generating the records and the
 * aggregator reading the records.
 * 
 * The purpose of this class is not to provide long term storage for
 * IntervalRecords, that is performed by custom aggregators that run in separate
 * processes and use dynamic rules to decide what to log and where.
 */
class RecordStorage {
  friend class RecordSink;
  friend class RecordSource;
  friend class RecordStorageUtils;
  private:
    RecordStorage () : all(), SLAexceeded() {
        //counterType = DDTrace::counterType;
    }

    /**
      * A queue on which all intervals are recorded
      */
    SPSCQueue<IntervalRecord, RECORD_QUEUE_SIZE> all;
    /**
      * A queue on which only exceptional intervals are recorded
      */
    SPSCQueue<IntervalRecord, RECORD_QUEUE_SIZE> SLAexceeded;

    /**
      * The CounterType of records stored by the process being traced
      * Deprecated. We now store the counterType in each record so that
      * a CustomAggregator can work with records from multiple sources /
      * over time (for example, between executions of a RAMCloud app)
      */
    //CounterType counterType;
    /**
      * How many RDTSC ticks occur per second
      * Calculated once so that there is agreement on the ratio between
      * rdtsc and wallclock time
      */
    //double cyclesPerSecond;
};

class RecordStorageUtils {
  public:
    static RecordStorage* openStorageFile(const std::string& storageFile);  
};
typedef uint64_t ChannelsVersion;
class ChannelsVersionUtils {
  public:
    static std::atomic<ChannelsVersion>* openChannelsVersion(const std::string& baseName);
};

class ThreadInitializer {
  public:
    /**
      * Initializes any thread-local state for the calling thread.
      *
      * Throws a std::runtime_error if more than MAX_THREADS call this function
      */
    void initThread(){
        std::lock_guard<std::mutex> _(mutex);
        if (nextAvailableThreadId >= MAX_THREADS){
            fprintf(stderr, 
                    "ERROR: More than %zd threads using DDTrace",
                    MAX_THREADS);
            throw std::runtime_error("Cannot use DDTrace with more than MAX_THREADS threads");
        }
        threadid = nextAvailableThreadId;
        nextAvailableThreadId++;
    }
    ThreadInitializer():
    mutex(),
    nextAvailableThreadId(0){}
private:
    std::mutex mutex;
    ThreadId nextAvailableThreadId;
};

/**
  * A singleton class providing access to performance counters
  *
  * Because we cannot / do not want to open all performance counters, we
  * only open those specified by the CounterType.
  * 
  * A PerfCounters "builds" PerfRecord's, and is a friend.
  */
class PerfCounters {
    private:
        //The maximum number of concrete counters we need to implement any
        //countertype.
        // IMPORTANT: Must update this if you introduce any countertypes that
        // use more!
        static const int MAX_CONCRETE_COUNTERS_PER_COUNTERTYPE = 1;
    public:
        void init(){
            if (threadInitialized) return;
            //printf("Using counterType %d\n", counterType);
            switch(counterType){
                case USERSPACE_CYCLES_ONLY:
                    counters[0].init<CYCLES>(true, true, false, true);
                    break;
                case L3_REFERENCE_ONLY:
                    counters[0].init<L3_REFERENCE>(true, true, false, true);
                    break;
                case L3_MISS_ONLY:
                    counters[0].init<L3_MISS>(true, true, false, true);
                    break;
                case TIME_ONLY:
                    break;
                case INVALID_COUNTER_TYPE:
                    fprintf(stderr, "Invalid type for counterType provided\n");
                    abort();
            }
        }

        //Reads the counters, populating a PerfRecord 
        //Returns false in case of failure 
        bool readCounters(PerfRecord* record){
            if (!threadInitialized) return false;
            //Store the counterType in the record
            record->recordCounterType = counterType;
            switch(counterType){
                case USERSPACE_CYCLES_ONLY:
                    record->counters[0] = counters[0].read();
                    break;
                case L3_REFERENCE_ONLY:
                    record->counters[0] = counters[0].read(); 
                    break;
                case L3_MISS_ONLY:
                    record->counters[0] = counters[0].read();
                    break;
                case TIME_ONLY:
                    break;
                case INVALID_COUNTER_TYPE:
                    fprintf(stderr, "Invalid type for counterType provided\n");
                    abort();
            }
            return true;
        }

        /**
         * Populates diffRecord with the differences
         * in each counter from the startRecord to endRecord.
         *
         * Returns false if startRecord and endRecord differ in the counters
         * they contain (i.e. if openCounters was called in the middle of an
         * Interval)
         * 
         * diffRecord can safely be equal to endRecord or startRecord.
         */ 
        static bool subtractCounters(const PerfRecord* startRecord,
                const PerfRecord* endRecord,
                PerfRecord* diffRecord){
            //Store the counterType in the record
            diffRecord->recordCounterType = counterType;
            for(size_t i = 0; i < MAX_COUNTERS_PER_COUNTERTYPE; i++){
                diffRecord->counters[i] = 
                    endRecord->counters[i] - startRecord->counters[i];
            }
            return true;
        }

        PerfCounters(){}

        ~PerfCounters()
        {
        }
    private:
        /**
          * The currently open counters (=> this determines what rdpmc may
          * return.)
          */
        PerfEventCounter counters[MAX_CONCRETE_COUNTERS_PER_COUNTERTYPE];
};

class SLARules {
  public:
    //TODO(bjmnbraun@gmail.com) better rules
    //100 microseconds
    const static uint64_t LONG_THRESHOLD_NS = 100000;
    bool exceedsSLAs(const IntervalRecord& record){
        //Simple rules for now.
        //TODO(bjmnbraun@gmail.com) better rules
        bool tooLong = 
        Cycles::toNanoseconds(record.getEndCycles() - record.getStartCycles()) 
        > LONG_THRESHOLD_NS;
        if (tooLong){
            return true;
        }
        return false;
    }
};

/** 
  * Used to write record data.
  * init() must be called before using any of the other methods
  */
#pragma GCC diagnostic ignored "-Weffc++"
class RecordSink {
  public:
    //Hmm. Clang wants static variables up-front. Maybe a compiler bug?
    static bool enabled;

    RecordSink();
    /**
     * The name passed into this function is recommended to be derived from
     * serverId, but it suffices for it to be different for each server
     *
     * \param baseName 
     *   a string used for the name of the sink. It must consist only
     *   of characters that are allowed in filenames.
     */
    void init(const std::string& baseName);

#define ENABLE_EXTRA_LOGGING 1

    /**
    * Store the value of the system cycle counter at a particular point in
    * time, and notify the background thread to flush in-memory buffers to
    * file when LOW_THRESHOLD numbers have been recorded.
    *
    * If HIGH_THRESHOLD intervals have been accumulated in memory, it will
    * begin dropping intervals.
    *
    * \param startCycles
    *      The value of the CPU cycle counter at the start of this event.
    *
    * \param endCycles
    *      The value of the CPU cycle counter at the end of this event.
    *
    * \param clock
    *      The current value of the vector clock, which will eventually be
    *      output. Includes the ID of the original RPC this interval is
    *      part of. Must correspond with a recordIntervalBegin's clock.
    *
    * \param countersDiff
    *      The difference in the counters we care about, over the interval
    *      named by the VectorClock for a given Rpc.
    */
    void recordIntervalEnd(
           const uint64_t& startCycles,
           const uint64_t& endCycles,
           const PerfRecord& countersDiff,
           const VectorClock* clock) {
       if (!initialized) return;
       if (!enabled) return;

#if ENABLE_EXTRA_LOGGING == 1
       IntervalRecord intervalRecord (startCycles, endCycles, *clock, 
       serverId, Cycles::perSecond(), countersDiff);
       {
#if DEBUG_DROPPED_RECORDS == 1
           bool couldPush = records->all.push(intervalRecord);
           if (!couldPush){
                fprintf(stderr, "Disk thread has fallen behind, dropping a packet\n");
           }
#else
           records->all.push(intervalRecord);
#endif
       }
       if (slaRules.exceedsSLAs(intervalRecord)){
#if DEBUG_DROPPED_RECORDS == 1
           bool couldPush = records->SLAexceeded.push(intervalRecord);
           if (!couldPush){
                fprintf(stderr, "Disk thread has fallen behind, dropping a packet\n");
           }
#else
           records->SLAexceeded.push(intervalRecord);
#endif
       }
#if 0
       auto openIntervals = &records->getPerThreadStorage()->openIntervals;
       if (openIntervals->empty()){
         throw std::runtime_error("recordIntervalEnd without paired recordIntervalBegin");
       }
       openIntervals->pop();
#endif
#endif
    }

    ~RecordSink();
  private:
    /**
     * The name of the file that we write our self-describing counters
     * to.
     */
    std::string logFile;

    /**
     * Communication buffer between RecordSink and RecordSource
     */ 
    RecordStorage* records;
};

/**
 * Used to read recorded data.
 * init() must be called before using any of the other methods
 */
class RecordSource {
  public:
    /**
     * The name passed into this function is recommended to be derived from
     * serverId, but it suffices for it to be different for each server.
     *
     * Scans for open channels and connects to all of them
     *
     * \param baseName 
     *   a name of the sink to connect to. 
     */
    void init(const std::string& logFile);
    
    /**
      * Scans for new channels and connects to them.
      * Resets recordsIterator 
      */
    void updateChannels();

    /**
      * Scans through all open channels, permanently removing any
      * channels where the RecordSink has terminated
      * Resets recordsIterator 
      */
    void cleanupDeadChannels();

    /**
      * Pops an IntervalRecord from the ALL queue
      *
      * Returns false if there are no records to get
      */
    bool popRecord(IntervalRecord* out){
        if (!initialized) return false;
        RecordStorage* records = selectRecords();
        if (!records){
            return false;
        }
        return records->all.pop(out);
    }
    
    /**
      * Pops an IntervalRecord from the SLAexceeded queue
      *
      * Returns false if there are no records to get
      */
    bool popSLAExceededRecord(IntervalRecord* out) {
        if (!initialized) return false;
        RecordStorage* records = selectRecords();
        if (!records){
            return false;
        }
        return records->SLAexceeded.pop(out);
    }

    /*
    double getCyclesPerSec() {
        RecordStorage* records = selectRecords();
        return records->cyclesPerSecond;
    }
    */
    
  private:
    /**
      * Arbitrates access to the RecordSinks (one for each worker
      * thread) by this RecordSource.
      */
    RecordStorage* selectRecords(){
        //Check if we have new channels
        if (channelsVersion != channelsAvailableVersion->load(std::memory_order_relaxed)){
            //Updates records and updates channelsVersion
            updateChannels(); 
        }

        /*
        if (Cycles::toMicroseconds(Cycles::rdtsc() - timeLastUpdateRecords) > CHECK_RECORDS_INTERVAL){
            //Updates records and sets invalidateRecords to some time in
            //the future
            updateChannels(); 
        }
        */
        if (++selectRecordsCounter >= SELECT_RECORDS_REUSE_COUNTER){
            if (recordsIterator == recordStorageSet.end()){
                recordsIterator = recordStorageSet.begin();
            } else {
                ++recordsIterator;
            }
            selectRecordsCounter = 0;
        }
        if (recordsIterator == recordStorageSet.end()){
            //No open channels.
            return NULL;
        }
        return recordsIterator->second;
    }

    typedef std::unordered_map<std::string, RecordStorage*> RecordStorageSet;
    RecordStorageSet recordStorageSet;
    typedef RecordStorageSet::iterator RecordsIterator;
    RecordsIterator recordsIterator;
    /** 
      * Number of selectRecords called made since last call to
      * checkUpdateRecords
      */
    uint64_t selectRecordsCounter;

    /**
      * Base name used for locating shared memory files
      * Must equal the name passed to RecordSink::init
      */
    std::string baseName;

    /**
     * Communication integer used to discover new channels
     * channelsVersion is the current version loaded
     */
    std::atomic<ChannelsVersion>* channelsAvailableVersion;
    ChannelsVersion channelsVersion;
};

/**
 * This class provides automatic management for calling functions to record a
 * particular performance counter across a sequence of statements.
 */
class Interval {
    public:
        /**
         * Construct and start an interval.
         */
        Interval(VectorClock* clock = NULL, bool start = false) : 
              startTime(0),
              clock(clock),
              stopped(true)
        {
            /**
             * The value of the argument  start should be known at compile
             * time, so we expect the compiler to optimize the branch away.
             */
            if (clock || start)
                this->start();
        }

        /**
         * Ends an interval on destruction, if the interval is not already
         * ended.
         */
        ~Interval() {
            stop();
        }

        void start() {
            if (!threadInitialized) return;
            stopped = false;
            startTime = Cycles::rdtsc();
            perfCounters[threadid].readCounters(&this->startCounters);
            //recordSinks[threadid].recordIntervalBegin(startTime, clock);
            //Increment the clock
//            clock->increment(serverId);
        }

        /**
          * This version of the method should be called when the VectorClock
          * is most easily available at start.
          */
        void start(VectorClock* clock) {
            this->clock = clock;
            start();
        }

        /**
         * End an interval, record the current vector clock and bump it on the
         * current server.
         *
         * This method is not thread-safe and we assume that the same vector
         * clock object will only be touched by one thread at a time. This is
         * equivalent to assuming that only one thread will be processing any
         * given request at any time.
         *
         */
        void stop() {
            if (!threadInitialized || stopped || !clock) return;
            stopped = true;
            uint64_t stopTime = Cycles::rdtsc();
            // Bump for the clock for this interval so we can correctly
            // serialize using the clock.
            clock->increment(serverId);
            PerfRecord diffCounters;
            perfCounters[threadid].readCounters(&diffCounters);
            PerfCounters::subtractCounters(&startCounters,
                                          &diffCounters, 
                                          &diffCounters);
            recordSinks[threadid].recordIntervalEnd(startTime, 
                                     stopTime, 
                                     diffCounters, 
                                     clock);
        }

        /**
          * This version of the method should be called when the VectorClock is
          * not available at the time of construction for the Rpc.
          */
        void stop(VectorClock* clock) {
            this->clock = clock;
            stop();
        }
        
        /**
          * End the current interval and recycles the end values to effectively
          * start a new one simultaneously.
          */
        void checkpoint() {
            if (!initialized || stopped || !clock) return;
            stopped = false;
           
            // Bump for the clock for the next interval so we can correctly
            // serialize using the clock.
            clock->increment(serverId);

            uint64_t stopTime = Cycles::rdtsc();
            PerfRecord diffCounters, stopCounters;
            perfCounters[threadid].readCounters(&stopCounters);
            PerfCounters::subtractCounters(&startCounters,
                                          &stopCounters, 
                                          &diffCounters);
            recordSinks[threadid].recordIntervalEnd(
                                     startTime, 
                                     stopTime,
                                     diffCounters,
                                     clock);

            // Set startTime and startCounters
            startTime = stopTime;
            startCounters = stopCounters;

            //Record the beginning of the next interval
            //recordSinks[threadid].recordIntervalBegin(startTime, clock);
        }

        /**
          * This version of the method should be called when the VectorClock is
          * not available at the time of construction for the Rpc.
          */
        void checkpoint(VectorClock* clock) {
            this->clock = clock;
            checkpoint();
        }

        /**
         * Call this method to prevent a half-opened interval from recording a
         * value on destruction.
         */
        void abort() {
            stopped = true;
        }

        VectorClock* getClock() {
            return clock;
        }
        /** 
         * Set the clock only  if it is not already set.
         */
        void setClock(VectorClock* clock) {
            if (!this->clock)
                this->clock = clock;
        }

    private:
        /**
         * The time that this interval began recording.
         */
        uint64_t startTime;

        /**
         * The value of the performance counters when this inverval began
         * recording
         */
        PerfRecord startCounters;

         /**
          * The clock that is owned by this Interval object. 
          */
        VectorClock* clock;

        /**
         * A sanity check to prevent double calls to stop.
         */
        bool stopped;
};



} // End DDTrace
#endif
