#ifndef ARCH_HWPERFCOUNTERS_H
#define ARCH_HWPERFCOUNTERS_H

//"arch/HWPerfCounters.h" defines the architecture-specific parts of the
//PerfCounters.h file

//This file contains the implementation for the "maverick-" cluster computers

//Numbers found using the pmu-tools library

namespace DDTrace {

    //Since these are all fully-specified functions, they must use inline
    //otherwise the compiler will generate multiple definitions for every
    //translation unit including this file, which is not the desired behavior
   
    template <>
    inline void getHWCounter<CYCLES>(int* type, int* config){
        /*
           cpu_clk_unhalted.thread_p                  
           Thread cycles when thread is not in halt state

           This is desired because when frequency scaling
           occurs, the
           readings reflect how many clock ticks occur
           at the new frequency.
        */
        *type = PERF_TYPE_RAW;
        *config = 0x003c;
    };
    template <>
    inline void getHWCounter<L2_EVICTIONS_CLEAN>(int* type, int* config){
        /*
           l2_lines_out.demand_clean                  
           Clean L2 cache lines evicted by demand
        */
        *type = PERF_TYPE_RAW;
        *config = 0x01f2; 
    };
    template <>
    inline void getHWCounter<L2_EVICTIONS_DIRTY>(int* type, int* config){
        /*
           Dirty lines variant of L2_EVICTIONS_CLEAN
        */
        *type = PERF_TYPE_RAW;
        *config = 0x02f2; 
    };
    template <>
    inline void getHWCounter<L3_MISS>(int* type, int* config){
        /*
           longest_lat_cache.miss                     
                Core-originated cacheable demand requests missed LLC
        */
        *type = PERF_TYPE_RAW;
        *config = 0x412e; 
    };
    template <>
    inline void getHWCounter<L3_REFERENCE>(int* type, int* config){
        /*
           longest_lat_cache.reference                     
                Core-originated cacheable demand requests that refer to LLC
        */
        *type = PERF_TYPE_RAW;
        *config = 0x4f2e; 
    };
}

#endif
