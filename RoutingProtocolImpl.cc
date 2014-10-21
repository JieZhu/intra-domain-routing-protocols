#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"
#include "string.h"

const char RoutingProtocolImpl::PING_ALARM = 8;
const char RoutingProtocolImpl::LS_ALARM = 9;
const char RoutingProtocolImpl::DV_ALARM = 10;
const char RoutingProtocolImpl::CHECK_ALARM = 11;

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
    sys = n;
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
    /* release memory for ports */
    hash_map<unsigned short, Port*>::iterator port_iter = ports.begin();
    
    while (port_iter != ports.end()) {
        Port* port = port_iter->second;
        ports.erase(port_iter++);
        free(port);
    }
    
    /* release memory for dv_table */
    hash_map<unsigned short, DV_Entry*>::iterator dv_iter = dv_table.begin();
    
    while (dv_iter != dv_table.end()) {
        DV_Entry* dv_entry = dv_iter->second;
        dv_table.erase(dv_iter++);
        free(dv_entry);
    }
    
    /* release memory for ls_table */
    /*hash_map<unsigned short, LS_Entry*>::iterator ls_iter = ls_table.begin();
     
     while (ls_iter != ls_table.end()) {
     LS_Entry* ls_entry = ls_iter->second;
     ls_table.erase(ls_iter++);
     free(ls_entry);
     }*/
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
    this->num_ports = num_ports;
    this->router_id = router_id;
    this->protocol_type = protocol_type;
    
    /* first ping message sent at 0 second */
    sys->set_alarm(this, 0, (void*)&PING_ALARM);
    sys->set_alarm(this, CHECK_DURATION, (void*)&CHECK_ALARM);
    
    if (protocol_type == P_LS) {
        sequence_num = 0;
        linkSt = new vector<LS_Entry*>;
        for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
            Port* port = iter->second;
            LS_Entry *entry = (struct LS_Entry*)malloc(sizeof(struct LS_Entry));
            entry->neighbor_id = (unsigned short)htons(port->neighbor_id);
            entry->cost = (unsigned short)htons(port->cost);
            entry->time_to_expire = sys->time() + LS_TIMEOUT;
            linkSt->push_back(entry);
        }
     
        sys->set_alarm(this, LS_DURATION, (void*)&LS_ALARM);
        
    } else {
        sys->set_alarm(this, DV_DURATION, (void*)&DV_ALARM);
    }
    
}

void RoutingProtocolImpl::handle_alarm(void *data) {
    char alarm_type = *(char*)data;
    switch (alarm_type) {
        case PING_ALARM:
            handle_ping_alarm();
            break;
        case LS_ALARM:
            handle_ls_alarm();
            break;
        case DV_ALARM:
            handle_dv_alarm();
            break;
        case CHECK_ALARM:
            handle_check_alarm();
            break;
        default:
            break;
    }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
    if (!check_packet_size(packet, size)) {
        cerr << "[ERROR] The router " << router_id << " received a packet with a wrong packet size at time "
        << sys->time() / 1000.0 << endl;
        free(packet);
        return;
    }
    
    char packet_type = *(char*)packet;
    //  cout << "size: " << sizeof(ePacketType) << endl;
    //  cout << "type: " << (unsigned short)packet_type << endl;
    switch (packet_type) {
        case DATA:
            handle_data_packet();
            break;
        case PING:
            handle_ping_packet(port, packet, size);
            break;
        case PONG:
            handle_pong_packet(port, packet);
            break;
        case LS:
            handle_ls_packet(port, packet, size);
            break;
        case DV:
            handle_dv_packet();
            break;
        default:
            break;
    }
}

void RoutingProtocolImpl::handle_ping_alarm() {
    unsigned short packet_size = 12;
    
    /* send ping packet to all ports */
    for (int i = 0; i < num_ports; ++i) {
        char* packet = (char*)malloc(packet_size);
        *(ePacketType*)packet = PING;
        *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
        *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
        *(unsigned int*)(packet + 8) = (unsigned int)htonl(sys->time());
        
        sys->send(i, packet, packet_size);
    }
    
    sys->set_alarm(this, PING_DURATION, (void*)&PING_ALARM);
}

void RoutingProtocolImpl::handle_ls_alarm() {
    unsigned short packet_size = 12 + 4 * ports.size();
    
    /* flood */
    for (hash_map<unsigned short, Port*>::iterator iter_i = ports.begin(); iter_i != ports.end(); ++iter_i) {
        char* packet = (char*)malloc(packet_size);
        *(char*)packet = LS;
        *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
        *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
        *(unsigned int*)(packet + 8) = (unsigned int)htonl(sequence_num);
        /* get neighbor ID and cost from ports */
        int count = 0;
        for (hash_map<unsigned short, Port*>::iterator iter_j = ports.begin(); iter_j != ports.end(); ++iter_j) {
            int offset = 12 + 4 * count;
            Port* port = iter_j->second;
            *(unsigned short*)((char*)packet + offset) = (unsigned short)htons(port->neighbor_id);
            *(unsigned short*)((char*)packet + offset + 4) = (unsigned short)htons(port->cost);
            count++;
        }
        
        sys->send(iter_i->first, packet, packet_size);
    }
    sequence_num++;
    sys->set_alarm(this, LS_DURATION, (void*)&LS_ALARM);
}

void RoutingProtocolImpl::handle_dv_alarm() {
    unsigned short packet_size = 8 + 4 * dv_table.size();
    
    /* send dv to neighbors */
    for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
        char* packet = (char*)malloc(packet_size);
        *(ePacketType*)packet = DV;
        *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
        *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
        *(unsigned short*)(packet + 6) = (unsigned short)htons(iter->second->neighbor_id);
        
        /* get router ID and cost from dv_table */
        int count = 0;
        for (hash_map<unsigned short, DV_Entry*>::iterator dv_iter = dv_table.begin(); dv_iter != dv_table.end(); ++dv_iter) {
            int offset = 8 + 4 * count;
            DV_Entry* entry = dv_iter->second;
            
            /* poison reverse */
            int cost = (iter->second->neighbor_id == entry->next_hop) ? INFINITY_COST : entry->cost;
            *(unsigned short*)(packet + offset) = (unsigned short)htons(dv_iter->first);
            *(unsigned short*)(packet + offset + 4) = (unsigned short)htons(cost);
        }
        sys->send(iter->first, packet, packet_size);
    }
    
    sys->set_alarm(this, DV_DURATION, (void*)&DV_ALARM);
}

void RoutingProtocolImpl::handle_check_alarm() {
    update_port_stat();
    
    switch (protocol_type) {
        case P_LS:
            update_ls_stat();
            break;
        case P_DV:
            update_dv_stat();
            break;
        default:
            break;
    }
    
    sys->set_alarm(this, CHECK_DURATION, (void*)&CHECK_ALARM);
}

void RoutingProtocolImpl::update_port_stat() {
    hash_map<unsigned short, Port*>::iterator iter = ports.begin();
    
    while (iter != ports.end()) {
        Port* port = iter->second;
        if (sys->time() > port->time_to_expire) {
            ports.erase(iter++);
            free(port);
        } else {
            ++iter;
        }
    }
}

void RoutingProtocolImpl::update_ls_stat() {
    
        linkSt->clear();
    for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
        Port* port = iter->second;
        LS_Entry *entry = (struct LS_Entry*)malloc(sizeof(struct LS_Entry));
        entry->neighbor_id = (unsigned short)htons(port->neighbor_id);
        entry->cost = (unsigned short)htons(port->cost);
        entry->time_to_expire = sys->time() + LS_TIMEOUT;
        linkSt->push_back(entry);
    }
    
    compute_ls_forwarding_table();
    
}

void RoutingProtocolImpl::update_dv_stat() {
    
}


void RoutingProtocolImpl::handle_data_packet() {
    
//    unsigned short dest_id = (unsigned short)ntohs( *(unsigned short*)((char*)packet + 6) );
//    unsigned short next_hop = forwarding_table[dest_id]->next_hop;
    
    
    
}

void RoutingProtocolImpl::handle_ping_packet(unsigned short port_id, void* packet, unsigned short size) {
    unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));
    
    *(char*)packet = (char)PONG;
    *(unsigned short*)((char*)packet + 4) = (unsigned short)htons(router_id);
    *(unsigned short*)((char*)packet + 6) = (unsigned short)htons(dest_id);
    
    sys->send(port_id, packet, size);
}

void RoutingProtocolImpl::handle_pong_packet(unsigned short port_id, void* packet) {
    if (!check_dest_id(packet)) {
        cerr << "[ERROR] The router " << router_id << " received a PONG packet with a wrong destination ID at time "
        << sys->time() / 1000.0 << endl;
        free(packet);
        return;
    }
    
    unsigned int timestamp = (unsigned int)ntohl(*(unsigned int*)((char*)packet + 8));
    unsigned short neighbor_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));
    
    Port* port = (Port*)malloc(sizeof(Port));
    /* impossible to overflow because PONG_TIMEOUT is only 15 seconds */
    port->cost = (short)(sys->time() - timestamp);
    port->time_to_expire = sys->time() + PONG_TIMEOUT;
    port->neighbor_id = neighbor_id;
    ports[port_id] = port;
    
    free(packet);
}

void RoutingProtocolImpl::handle_ls_packet(unsigned short port_id, void* packet, unsigned short size) {
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
        vec->time_to_expire=sys->time()+LS_TIMEOUT;
        vec->neighbor_id=neighbor_id;
        vec->cost=cost;
        
        ls_vec->push_back(vec);
        pointor=pointor+4;
        
    }
    
    ls_table[source_id]=ls_vec;
    
    for (hash_map<unsigned short, Port*>::iterator iter_j = ports.begin(); iter_j != ports.end(); ++iter_j) {
        if (port_id != iter_j->first){
            char* packet_f = (char*)malloc(size);
            memcpy((char*)packet_f,(char*)packet,size);
            sys->send(iter_j->first, packet_f, size);
        }
    }
    
    free(packet);
    //update~=
    compute_ls_forwarding_table();
}


void RoutingProtocolImpl::handle_dv_packet() {
    
}

bool RoutingProtocolImpl::check_packet_size(void* packet, unsigned short size) {
    unsigned short packet_size = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 2));
    return (size == packet_size) ? true : false;
}

bool RoutingProtocolImpl::check_dest_id(void* packet) {
    unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 6));
    return (router_id == dest_id) ? true : false;
}

bool RoutingProtocolImpl::check_lsp_sequence_num(void* packet) {
    unsigned short source_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));
    unsigned int sequence_num = (unsigned int)ntohl(*(unsigned int*)((char*)packet + 8));
    
    if((ls_sequence_num.find(source_id)==ls_sequence_num.end())||ls_sequence_num[source_id]<sequence_num){
        ls_sequence_num[source_id]=sequence_num;
        return true;
    }
    else
        return false;
}

void RoutingProtocolImpl::compute_ls_forwarding_table(){

    //destination id and LS_info
    hash_map<unsigned short, LS_Info*> tentative;

    forwarding_table.clear();
    if(!linkSt->empty()){
        for (vector<LS_Entry*>::iterator it=linkSt->begin(); it != linkSt->end(); it++) {
            struct LS_Info *lin = (struct LS_Info*)malloc(sizeof(struct LS_Info));
            lin->destinatin_id = (*it)->neighbor_id;
            lin->cost = (*it)->cost;
            lin->next_hop_id = (*it)->neighbor_id;
            tentative[lin->destinatin_id] = lin;
        }
    }
    while (!tentative.empty()) {
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

        tentative.erase(min->destinatin_id);

        struct Forwarding_Table_Entry *f_entry = (struct Forwarding_Table_Entry*) malloc(sizeof(struct Forwarding_Table_Entry));
        f_entry->dest_id = dest;
        f_entry->next_hop = next;
        forwarding_table[dest] = f_entry;

        if(!ls_table[dest]->empty())
        for (vector<LS_Entry*>::iterator it = ls_table[dest]->begin(); it != ls_table[dest]->end(); it++) {
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
                if((!forwarding_table[(*it)->neighbor_id]) && (router_id != (*it)->neighbor_id) )
                    tentative[(*it)->neighbor_id] = temp;
                
            }
               
        }

        
    }
    
    //
    hash_map<unsigned short, LS_Info*>::iterator ls_iter = tentative.begin();
    
    while (ls_iter != tentative.end()) {
        LS_Info* ls_info = ls_iter->second;
        tentative.erase(ls_iter++);
        free(ls_info);
    }
    
    
}