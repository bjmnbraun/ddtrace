#ifndef PERFGRAPHDATASTRUCTURES__H
#define PERFGRAPHDATASTRUCTURES__H

#include "DDTraceConfig/atomic.h"
//#include <type_traits>

namespace DDTrace {

/** 
  * Simple in-place queue implementation.
  * Not thread safe.
  */ 
template <class T, size_t N>
class Queue {
  public:
    /**
      * Pushes an element to the queue. Returns false if there is no 
      * space left in the queue.
      */ 
    bool push(const T& elt){
        size_t nextWriteIndex = (writeIndex + 1) % N;
        if (nextWriteIndex == readIndex){
            return false;
        }
        data[writeIndex] = elt;
        writeIndex = nextWriteIndex;
        return true;
    }
    /**
      * Retrieves the next element off the queue, storing it in value.
      * If there is no element in the queue, returns false.
      */
    bool pop(T* value){
        if (readIndex == writeIndex){
            return false;
        }
        *value = data[readIndex];
        readIndex = (readIndex + 1)%N;
        return true;
    }
    Queue() :
    writeIndex(0),
    readIndex(0) {}
  private:
    size_t writeIndex;
    size_t readIndex;
    T data[N];
};

/** 
  * Simple in-place stack implementation.
  * Not thread safe.
  */ 
template <class T, size_t N>
class Stack {
  public:

    typedef T* iterator;

    /**
      * Pushes an element to the stack.
      * If there is no room in the stack, returns false instead.
      */
    bool push(const T& elt){
        if (size == N){
            return false;
        }
        data[size] = elt;
        size++;
        return true;
    }
    /**
      * Pops the top element off the stack.
      * If there is no element to pop, returns false.
      */
    bool pop(){
        if (size == 0){
            return false;
        }
        size--;
        return true;
    }

    /**
      * Returns true iff the stack is empty
      */
    bool empty(){
        return size == 0;
    }

    /** 
      * Returns reference to the last element in the container.
      * Calling back on an empty container is undefined. 
      */
    const T& back(){
        return data[size - 1];
    }

    /** 
      * Returns an iterator pointing to the bottom of the stack
      * Incrementing the iterator moves you towards the top of the stack
      */
    iterator begin(){
        return data;
    }
    /** 
      * Returns an iterator past-the-end of the stack
      */
    iterator end(){
        return data + size;
    }

    Stack() :
    size(0) {}
  private:
    size_t size;
    T data[N];
};

/** 
  * Simple SPSC in-place queue implementation.
  * Thread safe.
  */ 
template <class T, size_t N>
class SPSCQueue {
  public:
    /**
      * Pushes an element to the queue. Returns false if there is no 
      * space left in the queue.
      */ 
    bool push(const T& elt){
        size_t _writeIndex = writeIndex.load(std::memory_order_relaxed);
        size_t _readIndex = readIndex.load(std::memory_order_acquire);
        size_t nextWriteIndex =  (_writeIndex + 1) % N;
        if (nextWriteIndex == _readIndex) {
          return false;
        }
        data[_writeIndex] = elt;
        writeIndex.store(nextWriteIndex, std::memory_order_release);
        return true;
    }
    /**
      * Retrieves the next element off the queue, storing it in value.
      * If there is no element in the queue, returns false.
      */
    bool pop(T* value){
        size_t _readIndex = readIndex.load(std::memory_order_relaxed);
        size_t _writeIndex = writeIndex.load(std::memory_order_acquire);
        if (_readIndex == _writeIndex){
            return false;
        }
        *value = data[_readIndex];
        size_t nextReadIndex = (_readIndex + 1)%N;
        readIndex.store(nextReadIndex, std::memory_order_release);
        return true;
    }
    SPSCQueue() :
    writeIndex(0),
    readIndex(0) {}
  private:
    std::atomic<size_t> writeIndex;
    std::atomic<size_t> readIndex;
    T data[N];
};

#if 0

/** 
  * Simple SPSC in-place stack implementation.
  * Assumes that T is trivially copy assignable, and checks this via a static assertion
  * Thread safe.
  */ 
template <class T, size_t N>
class SPSCStack {
        /*
  static_assert(T::is_trivially_copyable::value,
  "SPSCStack can only take trivially copy assignable types.");
  */
  private:
    typedef uint64_t VersionNo;
  public:
    typedef VersionNo Reference;
    /**
      * The following methods are called by the producer
      */
    /**
      * Pushes an element to the stack.
      * If there is no room in the stack, returns false instead.
      */
    bool push(const T& elt){
        if (size == N){
            return false;
        }
        //Odd versionCounter => dirty
        versions[size].store(++versionCounter, std::memory_order_release);
        status[size] = true;
        data[size] = elt;
        //Even versionCounter => clean
        versions[size].store(++versionCounter, std::memory_order_release);
        size++;
        //printf("Push to %zd\n", size);
        return true;
    }
    /**
      * Pops the top element off the stack.
      * If there is no element to pop, returns false.
      */
    bool pop(){
        if (size == 0){
            return false;
        }
        //printf("Pop from %zd\n", size);
        status[size] = false;
        size--;
        return true;
    }

    /** 
      * Returns true if the stack is empty
      * NOTE: Only callable by the producer!
      */
    bool empty(){
      return size == 0;
    }
    /** 
      * Returns reference to the last element in the container.
      * Calling back on an empty container is undefined.
      * NOTE: Only callable by the producer!
      */
    const T& back(){
        return data[size - 1];
    }

    /**
      * Constructor is called by the producer
      */
    SPSCStack() :
    size(0),
    status{0}, 
    versionCounter(2),
    masks{0}
    {
      for(size_t i = 0; i < N; i++){
        versions[i].store(0, std::memory_order_relaxed);
      }
    }


    /**
      * The following methods are called by the consumer
      */

    /**
      * It is always allowed to call get() on any index in [0, MAX_SIZE-1]
      */
    size_t getMaxSize(){
        return N;
    }
    /**
      * Tries to get the ith index of the stack.
      * Returns 0 if the get failed
      *
      * Otherwise, the return value can be passed to mask to make sure
      * successive gets do not return the same value 
      */
    Reference get(size_t index, T* out){
        assert(index < N);
        //Just try to get the index'th item, ensuring that
        //versionCounter doesn't change
        VersionNo versionBefore = versions[index].load(std::memory_order_acquire);
        //Is this get masked?
        if (versionBefore == masks[index]){
           return 0; 
        }
        //This assignment won't have any bad side effects when
        //concurrently performed with modifications to data[index] if T
        //is trivially copy assignable.
        bool exists = status[index];
        if (!exists){
          return 0;
        }
        *out = data[index];
        VersionNo versionAfter = versions[index].load(std::memory_order_acquire);
        //We need versionBefore to == versionAfter and for both to be
        //even (clean)
        if (versionBefore != versionAfter){
          return 0;
        }
        if (versionBefore & 1){
          return 0;
        }
        return versionAfter;
    }

    /**
      * Given the return value of a successful get, 
      * prevents future gets to the same index from returning this same
      * value
      */
    void mask(size_t index, Reference ref){
        //version numbers are increasing
        masks[index] = std::max(masks[index], ref);
    }

  private:
    /**
      * The actual elements in the stack
      * Modified only by the producer
      */
    T data[N];
    /**
      * The number of actual elements in data
      */
    size_t size;
    /**
      * True in each index iff. the index contains an element
      * Modified only by the producer
      */
    bool status[N];
    /**
      * The version number of each  
      * Only modified by the producer
      */
    std::atomic<VersionNo> versions[N];
    /**
      * Version counter ticker (starts at 2 so that 0 is an invalid
      * version)
      */
    uint64_t versionCounter;
    /**
      * The version number of the last successful get to an index
      * Modified only by the consumer
      */
    VersionNo masks[N];
};

#endif

}

#endif
