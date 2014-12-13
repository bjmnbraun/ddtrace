#include <unordered_map>
#include <stack>
#include <vector>
#include <stdio.h>
#include <utility>
#include <algorithm>
//#include "VectorClock.h"
#include "DDTrace.h"


// The number of records we attempt to read.
#define READ_BATCH_SIZE 1000

using DDTrace::VectorClock;
using std::unordered_map;
using std::vector;

void usage() {
    fprintf(stderr, "Usage: LogParser [options] <eventfile1> <eventfile2> ...\n");
    fprintf(stderr, "    -o   specify output file (defaults to stdout)\n");
    exit(1);
}

/**
 * Read the IntervalRecords out of a particular file and add them to the
 * structure which is passed in.
 * */
void readEvents(const char* filename, 
        unordered_map<uint64_t, std::vector<DDTrace::IntervalRecord> >& eventMap) {
    FILE* eventFile = fopen(filename, "r");
    if (!eventFile)
        PG_DIE("file read error on %s", filename);

    // Here we simply convert from cycles to nanoseconds.
    // When we actually process an individual event and sort, the vector clocks
    // will tell us whether we actually went to a new machine and we can decide
    // whether startTime - previousEndTime is meaningful.
    DDTrace::CounterType toRet = DDTrace::INVALID_COUNTER_TYPE;

    int recordsRead = 0;
    DDTrace::IntervalRecord* buffer = (DDTrace::IntervalRecord*) 
        malloc(READ_BATCH_SIZE * sizeof(DDTrace::IntervalRecord));
    while ((recordsRead = 
                fread(buffer, sizeof(DDTrace::IntervalRecord), READ_BATCH_SIZE, eventFile))) {
        //printf("readBytes = %d\n", recordsRead); 
        for (int i = 0; i < recordsRead; i++) {
            uint64_t id = buffer[i].getClock().id;
            if (!eventMap.count(id))
                eventMap[id] = std::vector<DDTrace::IntervalRecord>();

            eventMap[id].push_back(buffer[i]);
        }

    }
    fclose(eventFile);
}



int dumpToTSV(unordered_map<uint64_t,std::vector<DDTrace::IntervalRecord> >& eventMap, const char* filename) {
    FILE* output = filename? fopen(filename, "r") : stdout;
    for (auto kv = eventMap.begin(); kv != eventMap.end(); kv++) {
        auto& v = kv->second;
        for (auto e = v.begin(); e != v.end(); e++) {
            auto clock = e->getClock();
            // Format is RequestID, ServerID, (Vector Clock in id-count id-count form), startCycles, endCycles, PerfRecord
            fprintf(output, "%zu,%u,(", clock.id, e->getServerID());
            for (int i = 0; i < clock.length; i++) {
               if (i == 0)
                   fprintf(output, "%hu-%hu", clock.entries[i].serverId, clock.entries[i].count);
               else
                   fprintf(output, " %hu-%hu", clock.entries[i].serverId, clock.entries[i].count);
            }
            fprintf(output, "),%zu,%zu,", e->getStartCycles(), e->getEndCycles());

            // Print the perfRecrod
            auto perfRecord =  e->getCountersDiff();
            bool hasCounter;
            uint64_t value;
#define PRINT_COUNTER(x) hasCounter = perfRecord . x (&value); \
            if (hasCounter) { \
                fprintf(output,"%ld", value); \
            } else { \
                fprintf(output,"NA"); \
            }

            PRINT_COUNTER(getUserspaceCycles)
            putc(',',output);
            PRINT_COUNTER(getL2Misses)
            putc(',',output);
            PRINT_COUNTER(getL3Misses)
            putc('\n',output);
        }
    }
    if (output != stdout) fclose(output);
}

/**
 * This is a tool that is designed to parse a collection of binary log files from the
 * DDTrace library and extract useful information out of them.
 *
 * Its default behavior is to cluster all the IntervalRecords associated with a
 * particular request ID and dump them out in a csv format.
 *
 * It can optionally list the end to end server side latency of each request by
 * ID, or print a symbolic textual representation representation of the intervals.
 */
int main(int argc, char** argv) {
    // Read all the event files and pull them by RpcId so it is easy to consider
    // each piece individually.
    // We should probably do batch reading.

    if (argc == 1) usage();

    const char* outfile = NULL;

    char c;
    // Only one option can be selected or none
    // Mutually conflicting options will have the last one win
    while ((c = getopt (argc, argv, "o:")) != -1)
    switch (c)
    {
        case 'o':
            outfile = optarg;
            break;
        case '?':
        default:
            usage();
            abort();
            return 1;
    }

    // Map of Rpc ID to events associated with it
    unordered_map<uint64_t, std::vector<DDTrace::IntervalRecord> > eventMap;

    // Read in each file
    for (int i = optind; i < argc; i++) {
        readEvents(argv[i], eventMap);
    }

    dumpToTSV(eventMap, outfile);

    int count = 0;
}
