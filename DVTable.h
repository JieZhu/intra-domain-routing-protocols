#ifndef DVTABLE_H
#define DVTABLE_H

#include "global.h"

struct DV_Entry {
  unsigned int time_to_expire;
  unsigned short cost;
};

class DVTable {
public:

  DVTable() {}
  ~DVTable();

  unsigned short dv_length() const { return table.size(); }
  void set_dv_packet(char* packet, unsigned short src_id, unsigned short dst_id, hash_map<unsigned short, unsigned short>& routing_table);
  bool update_by_pong(unsigned short src_id, unsigned short cost, unsigned short current_time, hash_map<unsigned short, unsigned short>& routing_table);
  bool update_by_dv(const char* packet, unsigned short size, unsigned int current_time, hash_map<unsigned short, unsigned short>& routing_table);
  void delete_dv(vector<unsigned short>& deleted_dst_ids, hash_map<unsigned short, unsigned short>& routing_table);
  bool check_dv_state(unsigned int current_time, hash_map<unsigned short, unsigned short>& routing_table);

private:

  static const unsigned int DV_TIMEOUT = 45000;

  /* destination ID and DV_Entry */
  hash_map<unsigned short, DV_Entry*> table;
};

#endif
