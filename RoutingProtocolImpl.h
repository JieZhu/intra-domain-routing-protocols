#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"

struct Port {
  unsigned short cost;
  unsigned int time_to_expire;
  unsigned short neighbor_id;
};

struct Forwarding_Table_Entry {
  unsigned short dest_id;
  unsigned short next_hop;
};

struct LS_Entry {
  unsigned int time_to_expire;
  unsigned short neighbor_id;
  unsigned short cost;
};

struct DV_Entry {
  unsigned int cost;
  unsigned int time_to_expire;
  unsigned short next_hop;
};

struct LS_Info{
    unsigned short destinatin_id;
    unsigned short cost;
    unsigned short next_hop_id;
};

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from
    // a neighbor router.

 private:

    /* alarm type */
    static const char PING_ALARM;
    static const char LS_ALARM;
    static const char DV_ALARM;
    static const char CHECK_ALARM;

    /* PING messages are generated every 10 seconds */
    static const unsigned int PING_DURATION = 10000;
    static const unsigned int PONG_TIMEOUT = 15000;

    /* LS updates every 30 seconds */
    static const unsigned int LS_DURATION = 30000;
    static const unsigned int LS_TIMEOUT = 45000;

    /* DV updates every 30 secondes */
    static const unsigned int DV_DURATION = 30000;
    static const unsigned int DV_TIMEOUT = 45000;

    /* 1-second check */
    static const unsigned int CHECK_DURATION = 1000;

    /* port ID and Port */
    hash_map<unsigned short, Port*> ports;
    /* router ID and neighbor LS_Entry */
    //hash_map<unsigned short, vector<LS_Entry*>*> ls_table;
    unsigned int sequence_num;

    /* router ID and DV_Entry */
    hash_map<unsigned short, DV_Entry*> dv_table;
    /* destination id and forwarding_table_entry */
    hash_map<unsigned short, Forwarding_Table_Entry*> forwarding_table;
    /* router id and neighbor ls_entry */
    hash_map<unsigned short, vector<LS_Entry*>*> ls_table;
    
    vector<LS_Entry*> *linkSt;

    hash_map<unsigned short, unsigned int> ls_sequence_num;
    Node *sys; // To store Node object; used to access GSR9999 interfaces
    unsigned short num_ports;
    unsigned short router_id;
    eProtocolType protocol_type;

    void handle_ping_alarm();
    void handle_ls_alarm();
    void handle_dv_alarm();
    void handle_check_alarm();


    void handle_data_packet();
    void handle_ping_packet(unsigned short port_id, void* packet, unsigned short size);
    void handle_pong_packet(unsigned short port_id, void* packet);
    void handle_ls_packet(unsigned short port_id, void* packet, unsigned short size);
    void handle_dv_packet();

    bool check_packet_size(void* packet, unsigned short size);
    bool check_dest_id(void* packet);
    bool check_lsp_sequence_num(void* packet);

    void update_port_stat();
    void update_ls_stat();
    void update_dv_stat();

    void compute_ls_forwarding_table();
};

#endif



