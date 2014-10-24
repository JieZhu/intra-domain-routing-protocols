#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include "DVTable.h"
#include "LSTable.h"



// struct LS_Entry { 
//   unsigned int time_to_expire;
//   unsigned short neighbor_id;
//   unsigned short cost;
// };

//struct Forwarding_Table_Entry {
//  unsigned short dest_id;
//  unsigned short next_hop;
//};

//struct LS_Info{ //add by jz52@rice.edu
//    unsigned short destinatin_id;
//    unsigned short cost;
//    unsigned short next_hop_id;
//};

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

    /* 1-second check */
    static const unsigned int CHECK_DURATION = 1000;

    /* port ID and Port */
    hash_map<unsigned short, Port*> ports;
//    unsigned int sequence_num;

    /* destination ID and next_hop */
    hash_map<unsigned short, unsigned short> routing_table;
    DVTable dv_table;
    LSTable ls_table;
     // hash_map<unsigned short, vector<LS_Entry*>*> ls_table;
     // vector<LS_Entry*> *linkSt;


//    hash_map<unsigned short, unsigned int> ls_sequence_num; 
    Node *sys; // To store Node object; used to access GSR9999 interfaces
    unsigned short num_ports;
    unsigned short router_id;
    eProtocolType protocol_type;

    void handle_ping_alarm();
    void handle_ls_alarm();
    void handle_dv_alarm();
    void handle_check_alarm();

    bool check_port_state();
    bool check_ls_state();
    bool check_dv_state();

    void recv_data_packet(char* packet, unsigned short size);
    void recv_ping_packet(unsigned short port_id, char* packet, unsigned short size);
    void recv_pong_packet(unsigned short port_id, char* packet);
    void recv_ls_packet(unsigned short port_id, char* packet, unsigned short size);
    void recv_dv_packet(char* packet, unsigned short size);

    bool check_packet_size(char* packet, unsigned short size);
    bool check_dst_id(char* packet);

    void send_dv_packet();
    void send_ls_packet();

//    void compute_ls_forwarding_table();
//    bool check_lsp_sequence_num(void* packet);

};

#endif

