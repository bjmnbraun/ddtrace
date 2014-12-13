#ifndef VECTORCLOCKUTILS_H
#define VECTORCLOCKUTILS_H

#include <vector>

//Yucky implementation, the C++ standard library really needs a multidimensional
//array class to avoid evil things like this. Boost has a solution, but hey.
typedef std::vector<std::vector<bool>> Adjacencies;
typedef Adjacencies Reachabilities;
typedef Adjacencies PartialOrderGraph;

//Fills empty out with any edges from in but removing any edge i,j
//if there is a path from i to j through at least one other node k in "in"
void makeClosestAdjacencies(const PartialOrderGraph* in, Adjacencies* out){
    assert(out->empty());
    int V = in->size();
    out->resize(V);
    for(int i = 0; i < V; i++){
        (*out)[i].resize(V);
    }
    for(int i = 0; i < V; i++){
        for(int j = 0; j < V; j++){
            if (j == i){
                continue;
            }
            if ((*in)[i][j]){
                bool hasOtherPath = false;
                for(int k = 0; k < V; k++){
                    if (k == i || k == j){
                        continue;
                    }
                    if ((*in)[i][k] && (*in)[k][j]){ //then we can get from i to j through k
                        hasOtherPath = true;
                        break;
                    }
                }
                (*out)[i][j] = !hasOtherPath;   
            }
        }
    }
}

//Enumerates the edges i,j in "in"  where j == target
void getIncomingEdges(const Adjacencies* in, int target, std::vector<int>* incoming){
    int V = in->size();
    for(int i = 0; i < V; i++){
        if ((*in)[i][target]){
            incoming->push_back(i);
        }
    }
}


//Fills empty Reachabilities out with the reachability of in
//Postcondition: (*out)[i][j] iff there is a path of adjacencies from i to j in
//"in"
/*
void makeReachability(const Adjacencies* in, Reachabilities* out){
    assert(out->empty());
    int V = in->size();
    out->resize(V);
    for(int i = 0; i < V; i++){
        (*in)[i].resize(V);
    }
    //Floyd Warshall algorithm
    for(int k = 0; i < V; i++){
        for(int i = 0; i < V; i++){
            for(int j = 0; j < V; j++){
                if (!(*out)[i][j]){
                    (*out)[i][j] = 
                        (*in)[i][k] && (*in)[k][j];
                }
            }
        }
    }
}
*/

/**
  * Compare two events by the vector clock. This is necessary when we are
  * aggregating files outputted by multiple servers which include shared
  * RpcId's.
  *
  * Returns True iff a.clock < b.clock
  * Returns False iff a and b are not comparable or b.clock < a.clock 
  *
  * NOTE: We assume that all inputs are actually the same RpcId
  */
bool vectorClockLessThan(const DDTrace::IntervalRecord& a, 
        const DDTrace::IntervalRecord& b) {
   auto& va = a.getClock();
   auto& vb = b.getClock();
  

   // If any part of the clock is greater than it cannot be less than
   bool canBeTrue = false;
   for (size_t i = 0; i < std::max(va.length, vb.length); i++) {
       if (va.entries[i].count > vb.entries[i].count) {
            return false;
       }
       else if (va.entries[i].count < vb.entries[i].count){
           canBeTrue = true;
       }
       if (i < va.length && i < vb.length &&
               va.entries[i].serverId != vb.entries[i].serverId) return false;
   }
   // Incomparable => not less than
   return canBeTrue;
}

#endif
