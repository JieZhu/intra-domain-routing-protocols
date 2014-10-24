#include "RoutingProtocolImpl.h"
#include <arpa/inet.h>
#include "Node.h"
#include "DVTable.h"

const char RoutingProtocolImpl::PING_ALARM = 8;
const char RoutingProtocolImpl::LS_ALARM = 9;
const char RoutingProtocolImpl::DV_ALARM = 10;
const char RoutingProtocolImpl::CHECK_ALARM = 11;

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  /* release memory for ports */
  hash_map<unsigned short, Port*>::iterator iter = ports.begin();

  while (iter != ports.end()) {
    Port* port = iter->second;
    ports.erase(iter++);
    free(port);
  }
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
//    linkSt = new vector<LS_Entry*>;
//        for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
//            Port* port = iter->second;
//            LS_Entry *entry = (struct LS_Entry*)malloc(sizeof(struct LS_Entry));
//            entry->neighbor_id = (unsigned short)htons(port->neighbor_id);
//            entry->cost = (unsigned short)htons(port->cost);
//            entry->time_to_expire = sys->time() + LS_TIMEOUT;
//            linkSt->push_back(entry);
//        }
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
  if (!check_packet_size((char*)packet, size)) {
    cerr << "[ERROR] The router " << router_id << " received a packet with a wrong packet size at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  char packet_type = *(char*)packet;
  switch (packet_type) {
  case DATA:
    recv_data_packet();
    break;
  case PING:
    recv_ping_packet(port, (char*)packet, size);
    break;
  case PONG:
    recv_pong_packet(port, (char*)packet);
    break;
  case LS:
    recv_ls_packet();
    break;
  case DV:
    recv_dv_packet((char*)packet, size);
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
    *packet = (char)PING;
    *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
    *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
    *(unsigned int*)(packet + 8) = (unsigned int)htonl(sys->time());

    sys->send(i, packet, packet_size);
  }

  sys->set_alarm(this, PING_DURATION, (void*)&PING_ALARM);
}

void RoutingProtocolImpl::handle_ls_alarm() {
  send_ls_packet();
  sys->set_alarm(this, LS_DURATION, (void*)&LS_ALARM);
}

void RoutingProtocolImpl::handle_dv_alarm() {
  send_dv_packet();
  sys->set_alarm(this, DV_DURATION, (void*)&DV_ALARM);
}

void RoutingProtocolImpl::handle_check_alarm() {
  bool port_update = check_port_state();

  if (protocol_type == P_LS) {
    // bool ls_upate = ls_table.check_ls_state();
    //    bool ls_upate = false;

    if (port_update) {
      linkSt->clear();
        for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
          Port* port = iter->second;
          LS_Entry *entry = (struct LS_Entry*)malloc(sizeof(struct LS_Entry));
          entry->neighbor_id = (unsigned short)htons(port->neighbor_id);
          entry->cost = (unsigned short)htons(port->cost);
          entry->time_to_expire = sys->time() + LS_TIMEOUT;
          linkSt->push_back(entry);
      }
    // flooding port info to all other nodes;
    // handle_ls_alarm();
    }
    compute_ls_forwarding_table();

  } else {
    bool dv_update = dv_table.check_dv_state(sys->time(), routing_table);

    if (port_update || dv_update) {
      send_dv_packet();
    }
  }

  sys->set_alarm(this, CHECK_DURATION, (void*)&CHECK_ALARM);
}

bool RoutingProtocolImpl::check_port_state() {
  bool update = false;
  hash_map<unsigned short, Port*>::iterator iter = ports.begin();
  vector<unsigned short> deleted_dst_ids;

  while (iter != ports.end()) {
    Port* port = iter->second;
    if (sys->time() > port->time_to_expire) {
      update = true;
      deleted_dst_ids.push_back(port->neighbor_id);
      ports.erase(iter++);
      free(port);
    } else {
      ++iter;
    }
  }

  if (update) {
    if (protocol_type == P_LS) {

    } else {
      dv_table.delete_dv(deleted_dst_ids, routing_table);
    }
  }

  return update;
}

void RoutingProtocolImpl::recv_data_packet() {

}

void RoutingProtocolImpl::recv_ping_packet(unsigned short port_id, char* packet, unsigned short size) {
  unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)((char*)packet + 4));

  *packet = (char)PONG;
  *(unsigned short*)(packet + 4) = (unsigned short)htons(router_id);
  *(unsigned short*)(packet + 6) = (unsigned short)htons(dest_id);

  sys->send(port_id, packet, size);
}

void RoutingProtocolImpl::recv_pong_packet(unsigned short port_id, char* packet) {
  if (!check_dst_id(packet)) {
    cerr << "[ERROR] The router " << router_id << " received a PONG packet with a wrong destination ID at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  unsigned int timestamp = (unsigned int)ntohl(*(unsigned int*)(packet + 8));
  unsigned short src_id = (unsigned short)ntohs(*(unsigned short*)(packet + 4));
  unsigned short cost = (short)(sys->time() - timestamp);
  unsigned int time_to_expire = sys->time() + PONG_TIMEOUT;
  free(packet);

  hash_map<unsigned short, Port*>::iterator iter = ports.find(port_id);

  if (iter != ports.end()) {
    Port* port = iter->second;
    port->time_to_expire = time_to_expire;
  } else {
    Port* port = (Port*)malloc(sizeof(Port));
    port->time_to_expire = time_to_expire;
    port->neighbor_id = src_id;
    ports[port_id] = port;
  }

  if (protocol_type == P_LS) {
    // update linkst;

  } else {
    if (dv_table.update_by_pong(src_id, cost, sys->time(), routing_table)) {
      send_dv_packet();
    }
  }
}

void RoutingProtocolImpl::recv_ls_packet() {
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

void RoutingProtocolImpl::recv_dv_packet(char* packet, unsigned short size) {
  if (!check_dst_id(packet)) {
    cerr << "[ERROR] The router " << router_id << " received a DV packet with a wrong destination ID at time "
         << sys->time() / 1000.0 << endl;
    free(packet);
    return;
  }

  if (dv_table.update_by_dv(packet, size, sys->time(), routing_table)) {
    send_dv_packet();
  }

  free(packet);
}

bool RoutingProtocolImpl::check_packet_size(char* packet, unsigned short size) {
  unsigned short packet_size = (unsigned short)ntohs(*(unsigned short*)(packet + 2));
  return (size == packet_size) ? true : false;
}

bool RoutingProtocolImpl::check_dst_id(char* packet) {
  unsigned short dest_id = (unsigned short)ntohs(*(unsigned short*)(packet + 6));
  return (router_id == dest_id) ? true : false;
}


void RoutingProtocolImpl::send_ls_packet() {
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
            *(unsigned short*)((char*)packet + offset + 2) = (unsigned short)htons(port->cost);
            count++;
        }
        
        sys->send(iter_i->first, packet, packet_size);
    }
    sequence_num++;
}

void RoutingProtocolImpl::send_dv_packet() {
  unsigned short packet_size = 8 + (dv_table.dv_length() << 2);

  for (hash_map<unsigned short, Port*>::iterator iter = ports.begin(); iter != ports.end(); ++iter) {
    char* packet = (char*)malloc(packet_size);
    Port* port = iter->second;
    dv_table.set_dv_packet(packet, router_id, port->neighbor_id, routing_table);

    sys->send(iter->first, packet, packet_size);
  }
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

        hash_map<unsigned short, vector<LS_Entry*>*>::iterator iter = ls_table.find(dest);

        if (iter != ls_table.end()) {
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
                        if((!forwarding_table[(*it)->neighbor_id]) && (router_id != (*it)->neighbor_id) )
                            tentative[(*it)->neighbor_id] = temp;
                        
                    }
                }
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
