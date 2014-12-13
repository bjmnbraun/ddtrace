#ifndef HELLO_WORLD_H 
#define HELLO_WORLD_H 

#include "DDTrace.h"
#include "immintrin.h"

using namespace DDTrace;

//can clean up log files by rm -rf /dev/shm/ddtrace_hello_world
//log files will also go away on reboot
const char* RECORD_SINK_NAME = "ddtrace_hello_world";

//x86 pause instruction for optimized busy loops
inline void cpu_delay(){
    _mm_pause();
}


#endif
