#include <unistd.h>

#include "hello_world.h"

int main(){
    //Which hardware counters to include in the performance data:
    CounterType counterType = L3_MISS_ONLY;
    //Pick a server id for this computer 
    // (in this example, this isn't very important)
    uint16_t serverId = 3;
    DDTrace::init(counterType, serverId);    

    //Each thread of the application has to call this
    //See hello_world.h for definition
    DDTrace::initThreadSink(RECORD_SINK_NAME);

    //Measure the performance of printing hello world
    {
        //Start a "hello world" request, with a fresh vector clock 
        //This is the first request. In a real system, this should be a UUID
        //identifying the request
        uint64_t originId = 1;
        VectorClock clock(originId);
        //There is one interval in this request, where we print hello world
        {
            Interval _(&clock); //<-- interval starts here
            _.annotate("Hello world!");
            printf("Hello world!\n");
        } //<-- interval ends here, i.e. when it goes out of scope
    }
}
