#ifndef TRACEREADER_H
#define TRACEREADER_H

#include <unordered_map>
#include <vector>

#include <DDTrace.h>

typedef std::unordered_map<uint64_t, std::vector<DDTrace::IntervalRecord> > 
    IntervalMap;

void readEvents(const char* filename, IntervalMap* events); 

#endif
