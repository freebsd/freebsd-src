/*
 * Network Daemon - Network management service
 * Handles interfaces, DHCP, WiFi, routes
 */

#ifndef _MOBILE_NETWORKD_H_
#define _MOBILE_NETWORKD_H_

#include <sys/types.h>

#define NETWORKD_SOCKET_PATH "/var/run/networkd.sock"

#define IFACE_STATE_DOWN     0
#define IFACE_STATE_UP       1
#define IFACE_STATE_CONFIG   2

#define DHCP_STATE_IDLE      0
#define DHCP_STATE_SELECTING 1
#define DHCP_STATE_REQUESTING 2
#define DHCP_STATE_BOUND     3
#define DHCP_STATE_RENEWING  4

#define WIFI_STATE_DISCONNECTED  0
#define WIFI_STATE_SCANNING    1
#define WIFI_STATE_CONNECTING  2
#define WIFI_STATE_CONNECTED   3

#define MAX_INTERFACES      16
#define MAX_SSID_LEN        32

struct iface_info {
    char name[16];
    int index;
    int state;
    char ip_addr[32];
    char netmask[32];
    char mac_addr[18];
};

struct dhcp_info {
    int state;
    char ip_addr[32];
    char server_ip[32];
    char lease_time[16];
};

struct wifi_network {
    char ssid[MAX_SSID_LEN + 1];
    int signal;
    int security;
};

int networkd_main(int argc, char **argv);
int networkd_init(void);
void networkd_shutdown(void);
int networkd_enum_interfaces(struct iface_info **ifaces, int *count);
int networkd_iface_up(const char *ifname);
int networkd_iface_down(const char *ifname);
int networkd_dhcp_request(const char *ifname);
int networkd_dns_set(const char *server);
int networkd_route_add(const char *destination, const char *gateway);
int networkd_wifi_scan(struct wifi_network **networks, int *count);
int networkd_wifi_connect(const char *ssid, const char *password);

#endif /* _MOBILE_NETWORKD_H_ */