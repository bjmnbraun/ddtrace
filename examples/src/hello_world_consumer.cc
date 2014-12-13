#include <signal.h>

//Need to agree on RECORD_SINK_NAME
#include "hello_world.h"

enum AggregationMode {
    COUNT_RECORDS = 0,
    LOG_DISK = 1,
    PRINT_RECORDS = 2,
};

//Can choose a different filename.
const char* filename = "hello_world.ddt";

bool shouldExit = false;

void logToDisk() {
    std::vector<IntervalRecord> tempStorage;
    IntervalRecord record;
    // buffer in memory 
    FILE* logFile = fopen(filename, "wb");
    if (!logFile) {
        fprintf(stderr, 
                "Error opening file for writing,...aborting\n");
        abort();
    }
    while(!shouldExit) {
        bool could_poll = recordSource.popRecord(&record);
        // At least make an attempt to batch
        while (could_poll && tempStorage.size() < 1000) { 
            tempStorage.push_back(record);
            could_poll = recordSource.popRecord(&record);
        }
        if (!tempStorage.empty()) {
            fwrite(&tempStorage[0], sizeof(IntervalRecord),
                    tempStorage.size(), logFile);
            tempStorage.clear();
        }
        else { // Delay for a bit if we did not see a record
            cpu_delay();
        }
    }
    fclose(logFile);
    printf("Stopping log\n");
}
void countRecords() {
    int processedTraces = 0;
    IntervalRecord record;
    while(!shouldExit){
        bool could_poll = recordSource.popRecord(&record);
        if (could_poll){
            processedTraces++;
            continue;
        }
        cpu_delay();
    }
}

void printRecord(IntervalRecord* record){
    //Print the vector clock
    VectorClock clock = record->getClock();
    printf("Record %ld\t", 
            clock.id);
    for(uint64_t i = 0; i < clock.length; i++){
        printf("%d=%d;", 
                clock.entries[i].serverId,
                clock.entries[i].count);
    }
    printf("\t");
    printf("%f\t", record->getElapsedSeconds());
    printf("%ld\t", record->getElapsedNanoseconds());
    PerfRecord perfRecord = record->getCountersDiff();
    uint64_t value = 0; 
    bool hasCounter = false;
#define PRINT_COUNTER(x) hasCounter = perfRecord . x (&value); \
    if (hasCounter) { \
        printf("%ld\t", value); \
    } else { \
        printf("X\t"); \
    }

    PRINT_COUNTER(getUserspaceCycles)
        PRINT_COUNTER(getL2Misses)
        PRINT_COUNTER(getL3Misses)

#undef PRINT_COUNTER
        printf("\n");
}


void printRecords() {
    int processedTraces = 0;
    IntervalRecord record;
    while(!shouldExit){
        bool could_poll = recordSource.popRecord(&record);
        if (could_poll){
            printRecord(&record);
            continue;
        }
        cpu_delay();
    }
    printf("Summary of traces processed:\n");
    printf("%d\n", processedTraces);
}

void exit_func(int signal){
    shouldExit = true;   
}

int main(int argc, char** argv){
    AggregationMode mode;
    if (argc >= 2){
        mode = static_cast<decltype(mode)>(atoi(argv[1]));
    } else {
        //Default to log to disk
        mode = LOG_DISK;
    }

    DDTrace::init();
    //Must agree with the target
    DDTrace::recordSource.init(RECORD_SINK_NAME);

    //Process channels until we are interrupted
    //First set up the signal (interruption) handlers
    auto sigret = signal(SIGTERM, exit_func);
    assert(sigret != SIG_ERR);
    sigret = signal(SIGINT, exit_func);
    assert(sigret != SIG_ERR);

    //Now, process channels until we are interrupted
    switch(mode){
        case COUNT_RECORDS:
            countRecords();
            break;
        case LOG_DISK:
            logToDisk();     
            break;
        case PRINT_RECORDS:
            printRecords();
            break;
    }
    //Cleanup dead channels 
    recordSource.cleanupDeadChannels();
}
