#ifndef ARCH_HWPERFCOUNTERS_H
#define ARCH_HWPERFCOUNTERS_H

//"arch/HWPerfCounters.h" defines the architecture-specific parts of the
//PerfCounters.h file

//This file contains the implementation for the "ramcloud-" cluster computers
//For these computers, we use the Module for opening rdpmc counters, not
//perf_event_open.
//static_assert(USE_PERF_EVENT_OPEN == 0);

//Numbers found using the pmu-tools library

namespace DDTrace {

    //Since these are all fully-specified functions, they must use inline
    //otherwise the compiler will generate multiple definitions for every
    //translation unit including this file, which is not the desired behavior
    template <>
    inline void getHWCounter<CYCLES>(int* type, int* config){
        *config = 2;
    };
    template <>
    inline void getHWCounter<L3_REFERENCE>(int* type, int* config){
        *config = 1; 
    };
    template <>
    inline void getHWCounter<L3_MISS>(int* type, int* config){
        *config = 0; 
    };
}

#endif
