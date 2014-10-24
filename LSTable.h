#ifdef LSTABLE_H
#define LSTABLE_H

#include "global.h"

struct LS_Entry {
    unsigned int time_to_expire;
    unsigned short neighbor_id;
    unsigned short cost;
};


class LSTable{

public:
    void getLinkState();
    
private:
    
    hash_map<unsigned short, vector<LS_Entry*>*> table;
    vector<LS_Entry*> *linkSt;
    
};

#endif