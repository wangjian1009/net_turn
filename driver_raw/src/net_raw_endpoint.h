#ifndef NET_RAW_ENDPOINT_H_INCLEDED
#define NET_RAW_ENDPOINT_H_INCLEDED
#include "net_raw_driver_i.h"

struct net_raw_endpoint {
    struct tcp_pcb * m_pcb;
};

int net_raw_endpoint_init(net_endpoint_t base_endpoint);
void net_raw_endpoint_fini(net_endpoint_t base_endpoint);
int net_raw_endpoint_connect(net_endpoint_t base_endpoint);
void net_raw_endpoint_close(net_endpoint_t base_endpoint);
int net_raw_endpoint_on_output(net_endpoint_t base_endpoint);

void net_raw_endpoint_set_pcb(struct net_raw_endpoint * endpoint, struct tcp_pcb * pcb);
    
#endif
