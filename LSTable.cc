#include "LSTable.h"


LSTable::LSTable(){
    
}

LSTable::~LSTable(){
    
}

void LSTable::init(unsigned short router_id){
    sequence_num = 0;
    vector<LS_Entry*>* linkst = new std::vector<LS_Entry*>;
    insert(router_id, linkst);
    this->router_id = router_id;
}

void LSTable::insert(unsigned short router_id, vector<LS_Entry*>* vec){
    table[router_id] = vec;
}

bool LSTable::check_ls_state(unsigned int current_time){
    
    bool update = false;
    
    hash_map<unsigned short, vector<LS_Entry*>*>::iterator it= find(router_id);
    vector<LS_Entry*>* linkst = it->second;
    
    hash_map<unsigned short, vector<LS_Entry*>*>::iterator iter = table.begin();
    
    while (iter != table.end()) {
        vector<LS_Entry*>* entry = iter->second;
        //not finish
        
        
        
        
    }
    
    return update;
    
}

hash_map<unsigned short, vector<LS_Entry*>*>::iterator LSTable::find(unsigned short router_id){
    return table.find(router_id);
}

void LSTable::delete_ls(hash_map<unsigned short, Port*> ports, unsigned int current_time){
    hash_map<unsigned short, vector<LS_Entry*>*>::iterator it= find(router_id);
    vector<LS_Entry*>* linkst = it->second;
    for (vector<LS_Entry*>::iterator iter = linkst->begin(); iter!=linkst->end(); iter++) {
        if((*iter)->time_to_expire < current_time){
            free(*iter);
            linkst->erase(iter);
        }
    }
    
}

void LSTable::update_ls_package(unsigned short port_id, char* packet, unsigned short size){
    /*if from self, just ignore the packet and free the packet*/
    if(port_id == 0xffff){
        free(packet);
        cout<<"from self, ignore"<<endl;
        return;
    }
    if(!check_lsp_sequence_num(packet)){
        free(packet);
        return;
    }
    unsigned short source_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));
    unsigned short pointor=12;
    vector<LS_Entry*> *ls_vec=new vector<LS_Entry*>;
    while(pointor<size){
        
        unsigned short neighbor_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + pointor));
        unsigned short cost = (unsigned short)ntohs(*(unsigned short*)((char*)packet + pointor+2));
        
        
        struct LS_Entry* vec=(struct LS_Entry*)malloc(sizeof(struct LS_Entry));
//      vec->time_to_expire=sys->time()+LS_TIMEOUT;
        vec->neighbor_id=neighbor_id;
        vec->cost=cost;
        
        ls_vec->push_back(vec);
        pointor=pointor+4;
    }
    //if ls_table has this entry??????
    table[source_id]=ls_vec;
    
}

bool LSTable::check_lsp_sequence_num(void* packet) {
    unsigned short source_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));
    unsigned int sequence_num = (unsigned int)ntohl(*(unsigned int*)((char*)packet + 8));
    
    if((ls_sequence_num.find(source_id)==ls_sequence_num.end())||ls_sequence_num[source_id]<sequence_num){
        ls_sequence_num[source_id]=sequence_num;
        return true;
    }
    else
        return false;
}

void LSTable::compute_ls_routing_table(hash_map<unsigned short, unsigned short>& routing_table){
    
    //destination id and LS_info
    hash_map<unsigned short, LS_Info*> tentative;
    //forwarding_table.clear();
    routing_table.clear();
    
    hash_map<unsigned short, vector<LS_Entry*>*>::iterator iter= table.find(router_id);
    vector<LS_Entry*>* linkst = iter->second;
    if(!linkst->empty()){
        for (vector<LS_Entry*>::iterator it=linkst->begin(); it != linkst->end(); it++) {
            struct LS_Info *lin = (struct LS_Info*)malloc(sizeof(struct LS_Info));
            lin->destinatin_id = (*it)->neighbor_id;
            lin->cost = (*it)->cost;
            lin->next_hop_id = (*it)->neighbor_id;
            tentative[lin->destinatin_id] = lin;
        }
    }
    while (!tentative.empty()) {
        // get the min cost link and confirm it into routing table
        struct LS_Info *min = (struct LS_Info*) malloc(sizeof(struct LS_Info));
        min->cost = 9999;
        for (hash_map<unsigned short, LS_Info*>::iterator it = tentative.begin(); it != tentative.end(); it++) {
            
            if (it->second->cost < min->cost) {
                min = it->second;
            }
        }
        
        unsigned short dest = min->destinatin_id;
        unsigned short cost = min->cost;
        unsigned short next = min->next_hop_id;
        // erase it form tentative
        tentative.erase(min->destinatin_id);
        //put in routing table
        routing_table[dest] = next;
        
        // update tentative table;
        hash_map<unsigned short, vector<LS_Entry*>*>::iterator iter = table.find(dest);
        if (iter != table.end()) {
            if (!iter->second->empty()) {
                
                for (vector<LS_Entry*>::iterator it = iter->second->begin(); it != iter->second->end(); it++){
                    unsigned short cost_thru_next = cost + (*it)->cost;
                    
                    LS_Info *temp = (struct LS_Info*) malloc(sizeof(struct LS_Info));
                    temp->destinatin_id = (*it)->neighbor_id;
                    temp->next_hop_id = min->destinatin_id;
                    temp->cost = cost_thru_next;
                    
                    if (tentative[(*it)->neighbor_id]){
                        if(tentative[(*it)->neighbor_id] && (cost_thru_next < tentative[(*it)->neighbor_id]->cost)){
                            
                            tentative.erase((*it)->neighbor_id);
                            tentative[(*it)->neighbor_id] = temp;
                        }
                    }else{
                        if((!routing_table[(*it)->neighbor_id]) && (router_id != (*it)->neighbor_id) )
                            tentative[(*it)->neighbor_id] = temp;
                        
                    }
                }
            }
        }
    }
    
    // free tentative;
    hash_map<unsigned short, LS_Info*>::iterator ls_iter = tentative.begin();
    
    while (ls_iter != tentative.end()) {
        LS_Info* ls_info = ls_iter->second;
        tentative.erase(ls_iter++);
        free(ls_info);
    }
    
}

void LSTable::set_ls_package(char* packet, unsigned short packet_size, hash_map<unsigned short, Port*> ports){
            *(char*)packet = LS;
            *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
            *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
            *(unsigned int*)(packet + 8) = (unsigned int)htonl(sequence_num);
            /* get neighbor ID and cost from ports */
            int count = 0;
            for (hash_map<unsigned short, Port*>::iterator iter_j = ports.begin(); iter_j != ports.end(); ++iter_j) {
                int offset = 12 + (count<<2);
                Port* port = iter_j->second;
                *(unsigned short*)((char*)packet + offset) = (unsigned short)htons(port->neighbor_id);
                *(unsigned short*)((char*)packet + offset + 2) = (unsigned short)htons(port->cost);
                count++;
            }

}

void LSTable::increase_seq(){
    sequence_num++;
}
