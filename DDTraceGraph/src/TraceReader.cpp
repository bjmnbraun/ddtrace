#include "TraceReader.h"

// The number of records we attempt to read.
static const int READ_BATCH_SIZE = 1000;

void readEvents(const char* filename, IntervalMap* events){
    FILE* eventFile = fopen(filename, "r");
    if (!eventFile)
        PG_DIE("file read error on %s", filename);
    
    // Just convert from cycles to nanoseconds.
    // When we actually process an individual event and sort, the vector clocks
    // will tell us whether we actually went to a new machine and we can decide
    // whether startTime - previousEndTime is meaningful.

    int recordsRead = 0;
    DDTrace::IntervalRecord* buffer = (DDTrace::IntervalRecord*) 
        malloc(READ_BATCH_SIZE * sizeof(DDTrace::IntervalRecord));
    int count = 0;
    while ((recordsRead = 
    fread(buffer, sizeof(DDTrace::IntervalRecord), READ_BATCH_SIZE, eventFile))) {
       //printf("readBytes = %d\n", recordsRead); 
       for (int i = 0; i < recordsRead; i++) {
           uint64_t id = buffer[i].getClock().id;
           /*
           if (!eventMap.count(id))
               eventMap[id] = std::vector<DDTrace::IntervalRecord>();
               */

            /*
           printf("startCycles = %zu, endCycles = %zu\n", buffer[i].startCycles,
                    buffer[i].endCycles);
           buffer[i].startCycles  = (uint64_t) (buffer[i].startCycles /
               cyclesPerSececond * 1E9);
           buffer[i].endCycles  = (uint64_t) (buffer[i].endCycles /
               cyclesPerSececond * 1E9);
            */

           (*events)[id].push_back(buffer[i]);
           count++;
       }
    }
    fclose(eventFile);
}
