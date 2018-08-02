#include <assert.h>
#include <errno.h>
#include <net/if.h>
#if NET_RAW_USE_DEV_TUN
#  include <linux/if_tun.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include "cpe/pal/pal_string.h"
#include "cpe/pal/pal_unistd.h"
#include "cpe/utils/string_utils.h"
#include "net_address.h"
#include "net_raw_device_tun_i.h"
#include "net_raw_utils.h"

#if NET_RAW_USE_DEV_TUN

static void net_raw_device_tun_rw_cb(EV_P_ ev_io *w, int revents);

int net_raw_device_tun_init_tun(net_raw_driver_t driver, net_raw_device_tun_t device_tun, const char * name, uint16_t * mtu) {
    device_tun->m_dev_fd = -1;
    
#if CPE_OS_LINUX
    if ((device_tun->m_dev_fd = open("/dev/net/tun", O_RDWR)) < 0) {
        CPE_ERROR(driver->m_em, "raw: %s: open fail, %d %s", name, errno, strerror(errno));
        goto create_error;
    }
            
    struct ifreq ifr;
    bzero(&ifr, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);
                            
    if (ioctl(device_tun->m_dev_fd, TUNSETIFF, (void *) &ifr) < 0) {
        CPE_ERROR(driver->m_em, "raw: %s: ioctl fail, %d %s", name, errno, strerror(errno));
        goto create_error;
    }
    cpe_str_dup(device_tun->m_dev_name, sizeof(device_tun->m_dev_name), ifr.ifr_name);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        CPE_ERROR(driver->m_em, "raw: %s: socket fail, %d %s", device_tun->m_dev_name, errno, strerror(errno));
        goto create_error;
    }
            
    bzero(&ifr, sizeof(ifr));
    strcpy(ifr.ifr_name, device_tun->m_dev_name);

    /*mtu*/
    if (ioctl(sock, SIOCGIFMTU, (void *)&ifr) < 0) {
        CPE_ERROR(driver->m_em, "raw: %s: get mtu fail, %d %s", device_tun->m_dev_name, errno, strerror(errno));
        close(sock);
        goto create_error;
    }
    *mtu = ifr.ifr_mtu;

    /*address*/
    if (ioctl(sock, SIOCGIFADDR, (void *)&ifr) < 0) {
        CPE_ERROR(driver->m_em, "raw: %s: get addr fail, %d %s", device_tun->m_dev_name, errno, strerror(errno));
        close(sock);
        goto create_error;
    }
    device_tun->m_address = net_address_create_from_sockaddr(net_raw_driver_schedule(driver), (struct sockaddr *)(&ifr.ifr_addr), sizeof(ifr.ifr_addr));

    /*mask*/
    if (ioctl(sock, SIOCGIFNETMASK, (void *)&ifr) < 0) {
        CPE_ERROR(driver->m_em, "raw: %s: get mask fail, %d %s", device_tun->m_dev_name, errno, strerror(errno));
        close(sock);
        goto create_error;
    }
    device_tun->m_mask = net_address_create_from_sockaddr(net_raw_driver_schedule(driver), (struct sockaddr *)(&ifr.ifr_netmask), sizeof(ifr.ifr_netmask));
    
    close(sock);

#endif /*CPE_OS_LINUX*/

    if (fcntl(device_tun->m_dev_fd, F_SETFL, O_NONBLOCK) < 0) {
        CPE_ERROR(driver->m_em, "raw: %s: set nonblock fail, %d %s", name, errno, strerror(errno));
        goto create_error;
    }
    
    device_tun->m_watcher.data = device_tun;
    ev_io_init(&device_tun->m_watcher, net_raw_device_tun_rw_cb, device_tun->m_dev_fd, EV_READ);
    ev_io_start(driver->m_ev_loop, &device_tun->m_watcher);

    return 0;

create_error:
    if (device_tun->m_dev_fd != -1) {
        close(device_tun->m_dev_fd);
        device_tun->m_dev_fd = -1;
    }

    if (device_tun->m_address) {
        net_address_free(device_tun->m_address);
        device_tun->m_address = NULL;
    }

    if (device_tun->m_mask) {
        net_address_free(device_tun->m_mask);
        device_tun->m_mask = NULL;
    }

    return -1;
}

void net_raw_device_tun_fini_dev(net_raw_driver_t driver, net_raw_device_tun_t device_tun) {
    ev_io_stop(driver->m_ev_loop, &device_tun->m_watcher);
    close(device_tun->m_dev_fd);
}

static void net_raw_device_tun_rw_cb(EV_P_ ev_io *w, int revents) {
    net_raw_device_tun_t device_tun = w->data;
    net_raw_device_t device = &device_tun->m_device;
    net_raw_driver_t driver = device->m_driver;
    
    if (revents & EV_READ) {
        mem_buffer_clear_data(&driver->m_data_buffer);
        void * data = mem_buffer_alloc(&driver->m_data_buffer, device->m_frame_mtu);
        if (data == NULL) {
            CPE_ERROR(
                driver->m_em, "%s: rw: alloc data, size=%d fail",
                device->m_netif.name, device->m_frame_mtu);
            return;
        }
        
        do {
            int bytes = read(device_tun->m_dev_fd, data, device->m_frame_mtu);
            if (bytes <= 0) {
                if (bytes == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                else {
                    CPE_ERROR(
                        driver->m_em, "%s: rw: read data error, errno=%d %s",
                        device->m_netif.name, errno, strerror(errno));
                    break;
                }
            }
    
            assert(bytes <= device->m_frame_mtu);

            if (bytes > UINT16_MAX) {
                CPE_ERROR(driver->m_em, "%s: rw: packet too large", device->m_netif.name);
                break;
            }
            
            uint8_t * ethhead = NULL;
            uint8_t * iphead = data;  
            uint8_t * data = iphead + 20;

            if (driver->m_debug >= 2) {
                CPE_INFO(
                    driver->m_em, "%s: IN: %d |      %s",
                    device->m_netif.name, bytes,
                    net_raw_dump_raw_data(net_raw_driver_tmp_buffer(driver), ethhead, iphead, data));
            }
            
            struct pbuf *p = pbuf_alloc(PBUF_RAW, bytes, PBUF_POOL);
            if (!p) {
                CPE_ERROR(driver->m_em, "%s: rw: pbuf_alloc fail", device->m_netif.name);
                break;
            }

            err_t err = pbuf_take(p, iphead, bytes);
            if (err != ERR_OK) {
                CPE_ERROR(driver->m_em, "%s: rw: pbuf_take fail, error=%d (%s)", device->m_netif.name, err, lwip_strerr(err));
                pbuf_free(p);
                continue;
            }

            err = device->m_netif.input(p, &device->m_netif);
            if (err != ERR_OK) {
                CPE_ERROR(driver->m_em, "%s: rw: input fail, error=%d (%s)", device->m_netif.name, err, lwip_strerr(err));
                pbuf_free(p);
                continue;
            }
        } while(1);
    }
}
#endif