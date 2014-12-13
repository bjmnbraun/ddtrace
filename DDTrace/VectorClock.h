#ifndef PERFGRAPH_VECTORCLOCK_H
#define PERFGRAPH_VECTORCLOCK_H

#include <string.h>

#include <stdint.h>

namespace DDTrace {

/** 
  *  Limit vector clocks to this many entries.
  *  Once this limit is reached, increments on a server not having
  *  an entry in the clock will do nothing.
  */
const size_t MAX_VECTORCLOCK_ENTRIES = 8;

/**
 * This class tracks the server which has touched a particular Rpc, and the
 * count of the number of touches.
 *
 * The server ID should be a 16-bit value that is provided by the application.
 *
 * We assume this structure will be zeroed out by whichever networking protocol
 * header it is included into. Otherwise we should add a constructor which does
 * the memset.
 *
 * We asssume a byte-wise copy constructor which should be the default for such
 * structures.
 *
 * We also assume zero-initialization by default but we should verify this.
 *
 * This object should start life in the network request, and then get copied
 * into the Interval class. It should then be copied into each outgoing network
 * request as well as each Rpc response. The server who receives such a
 * response will use its vector clock, but the client will ignore it.
 */
struct VectorClock {
    // This is the id that defines the operation that this vector clock is
    // attached to.
    uint64_t id;
    uint64_t length;
    struct Entry {
        uint16_t serverId;
        uint8_t count;
        
        bool operator== (const struct Entry& other) const {
            return other.serverId == serverId && 
                   other.count == count;
        }
    } __attribute__((packed));
    Entry entries[MAX_VECTORCLOCK_ENTRIES];

    VectorClock(uint64_t id = 0) : id(id), length(0), entries{{0,0}} { }

    // Reset the vector clock (to a vector of all 0's, essentially)
    /*
    void reset(){
        length = 0;
    }
    */

    // Increment the entry associated with a given server ID
    // This operation is not thread safe, because we assume that this method
    // will only ever be called sequentially on any given server for a given
    // VectorClock which is tied to an Rpc.
    void increment(uint16_t serverId) {
       // Look for it and return after increment if we find it. 
       for (uint64_t i = 0; i < length; i++) {
          if (entries[i].serverId == serverId) {
           entries[i].count++;
           return;
          }
       }
       // We did not find it so make a new one
       if (length >= MAX_VECTORCLOCK_ENTRIES){
          return; //Can't add any more entries.
       }
       entries[length] = {serverId, 1};
       length++;
    }

    /**
      * Returns true iff two vector clocks are identical
      */
    bool operator== (const VectorClock& other) const{
        if (other.id != id){
            return false;
        }
        if (other.length != length){
            return false;
        }
        for(uint64_t i = 0; i < length; i++){
            if (!(other.entries[i] == entries[i])){
                return false;
            }
        }
        return true;
    }
} __attribute__((packed));
} // End DDTrace
#endif
