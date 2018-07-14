#ifndef NET_RAW_DEVICE_TUN_LISTENER_I_H_INCLEDED
#define NET_RAW_DEVICE_TUN_LISTENER_I_H_INCLEDED
#include "net_raw_device_tun_listener.h"
#include "net_raw_device_tun_i.h"

struct net_raw_device_tun_listener {
    net_raw_device_tun_t m_device;
    union {
        struct cpe_hash_entry m_hh;
        TAILQ_ENTRY(net_raw_device_tun_listener) m_next;
    };
    net_address_t m_address;
    net_protocol_t m_protocol;
};

void net_raw_device_tun_listener_free_all(net_raw_device_tun_t device_tun);

void net_raw_device_tun_listener_real_free(net_raw_device_tun_listener_t tun_listener);

uint32_t net_raw_device_tun_listener_hash(net_raw_device_tun_listener_t capture, void * user_data);
int net_raw_device_tun_listener_eq(net_raw_device_tun_listener_t l, net_raw_device_tun_listener_t r, void * user_data);

#endif
