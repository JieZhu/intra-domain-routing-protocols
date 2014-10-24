#include "DVTable.h"
#include <arpa/inet.h>


DVTable::~DVTable() {
  hash_map<unsigned short, DV_Entry*>::iterator iter = table.begin();

  while (iter != table.end()) {
    DV_Entry* entry = iter->second;
    table.erase(iter++);
    free(entry);
  }
}

void DVTable::set_dv_packet(char* packet, unsigned short src_id, unsigned short dst_id, hash_map<unsigned short, unsigned short>& routing_table) {
  *packet = (char)DV;
  unsigned short packet_size = 8 + (((unsigned short)table.size()) << 2);
  *(unsigned short*)(packet + 2) = (unsigned short)htons(packet_size);
  *(unsigned short*)(packet + 4) = (unsigned short)htons(src_id);
  *(unsigned short*)(packet + 6) = (unsigned short)htons(dst_id);

  unsigned short count = 0;
  for (hash_map<unsigned short, DV_Entry*>::iterator iter = table.begin(); iter != table.end(); ++iter) {
    unsigned short offset = 8 + (count << 2);

    /* poison reverse */
    unsigned short cost = (routing_table[iter->first] == dst_id) ? INFINITY_COST : iter->second->cost;
    *(unsigned short*)(packet + offset) = (unsigned short)htons(iter->first);
    *(unsigned short*)(packet + offset + 2) = (unsigned short)htons(cost);

    ++count;
  }
}

bool DVTable::update_by_pong(unsigned short src_id, unsigned short cost, unsigned short current_time, hash_map<unsigned short, unsigned short>& routing_table) {
  bool update = false;
  hash_map<unsigned short, DV_Entry*>::iterator iter = table.find(src_id);

  if (iter != table.end()) {
    DV_Entry* entry = iter->second;
    entry->time_to_expire = current_time + DV_TIMEOUT;

    if (cost != entry->cost) {
      if (routing_table[src_id] == src_id) {
        update = true;
        entry->cost = cost;
      } else if (cost < entry->cost) {
        update = true;
        entry->cost = cost;
        routing_table[src_id] = src_id;
      }
    }
  } else {
    update = true;
    DV_Entry* entry = (DV_Entry*)malloc(sizeof(DV_Entry));
    entry->cost = cost;
    entry->time_to_expire = current_time + DV_TIMEOUT;
    table[src_id] = entry;
    routing_table[src_id] = src_id;
  }

  return update;
}

bool DVTable::update_by_dv(const char* packet, unsigned short size, unsigned int current_time, hash_map<unsigned short, unsigned short>& routing_table) {
  bool update = false;
  unsigned short src_id = (unsigned short)ntohs(*(unsigned short*)(packet + 4));
  unsigned int count = (size - 8) >> 2;

  for (unsigned int i = 0; i < count; ++i) {
    unsigned int offset = 8 + (i << 2);
    unsigned short dst_id = (unsigned short)ntohs(*(unsigned short*)(packet + offset));
    unsigned short cost = (unsigned short)ntohs(*(unsigned short*)(packet + offset + 2));

    hash_map<unsigned short, DV_Entry*>::iterator iter = table.find(dst_id);

    /* if there is a DV entry in DV table */
    if (iter != table.end()) {
      DV_Entry* entry = iter->second;
      entry->time_to_expire = current_time + DV_TIMEOUT;
      iter = table.find(src_id);

      /* if there is a path to this neighbor in DV table */
      /* in most cases, src id should exists in routing table */
      if (iter != table.end()) {
        /* poison reverse */
        if (cost != INFINITY_COST) {
          cost += table[src_id]->cost;
        }

        if (cost < entry->cost) {
          update = true;
          entry-> cost = cost;
          routing_table[dst_id] = src_id;
        }
      }
    } else {
      update = true;
      DV_Entry* entry = (DV_Entry*)malloc(sizeof(DV_Entry*));
      entry->cost = cost;
      entry->time_to_expire = current_time + DV_TIMEOUT;
      table[dst_id] = entry;
      routing_table[dst_id] = src_id;
    }
  }

  return update;
}

void DVTable::delete_dv(vector<unsigned short>& deleted_dst_ids, hash_map<unsigned short, unsigned short>& routing_table) {
  for (vector<unsigned short>::iterator iter = deleted_dst_ids.begin(); iter != deleted_dst_ids.end(); ++iter) {
    hash_map<unsigned short, unsigned short>::iterator route_iter = routing_table.begin();

    while (route_iter != routing_table.end()) {
      if (route_iter->second == *iter) {
        routing_table.erase(route_iter++);

        DV_Entry* entry = table[*iter];
        table.erase(*iter);
        free(entry);
      } else {
        ++route_iter;
      }
    }
  }
}

bool DVTable::check_dv_state(unsigned int current_time, hash_map<unsigned short, unsigned short>& routing_table) {
  bool update = false;
  hash_map<unsigned short, DV_Entry*>::iterator iter = table.begin();

  while (iter != table.end()) {
    DV_Entry* entry = iter->second;

    if (entry->time_to_expire < current_time) {
      update = true;
      routing_table.erase(iter->first);
      table.erase(iter++);
      free(entry);
    } else {
      ++iter;
    }
  }

  return update;
}
