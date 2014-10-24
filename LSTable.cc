//#include "LSTable.h"
//
//LSTable::LSTable(){
//    linkSt = new vector<LS_Entry *>;
//}
//
//LSTable::~LSTable(){
//    
//}
//
//
//void LSTable::getLinkState(hash_map<unsigned short, Port*> ports) {
//    
//    linkSt->clear();
//    
//            for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
//                Port* port = iter->second;
//                LS_Entry *entry = (struct LS_Entry*)malloc(sizeof(struct LS_Entry));
//                entry->neighbor_id = (unsigned short)htons(port->neighbor_id);
//                entry->cost = (unsigned short)htons(port->cost);
//                entry->time_to_expire = sys->time() + LS_TIMEOUT;
//                linkSt->push_back(entry);
//            }
//}

