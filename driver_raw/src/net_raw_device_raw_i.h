#ifndef NET_RAW_DEVICE_RAW_I_H_INCLEDED
#define NET_RAW_DEVICE_RAW_I_H_INCLEDED
#include "net_raw_device_i.h"

struct net_raw_device_raw {
    struct net_raw_device m_device;
    uint8_t m_capture_all;
    net_raw_device_raw_capture_list_t m_captures;
    struct cpe_hash_table m_captures_by_source;
    struct cpe_hash_table m_captures_by_target;
    int m_fd;
    struct ev_io m_watcher;
};

#endif