/*
 * Network Manager - Network interface and routing management
 * BSD licensed
 */

#ifndef _MOBILE_NETWORK_MGR_H_
#define _MOBILE_NETWORK_MGR_H_

#include <sys/types.h>
#include <netinet/in.h>

#include "../wifi.h"
#include "../dhcp.h"

#define NM_MAX_INTERFACES       16
#define NM_MAX_ROUTES           64
#define NM_MAX_DNS_SERVERS      8
#define CONNECTIVITY_CHECK_URL  "http://connectivitycheck.gstatic.com/generate_204"

typedef enum {
    NM_IFACE_TYPE_ETHERNET  = 0,
    NM_IFACE_TYPE_WIFI      = 1,
    NM_IFACE_TYPE_CELLULAR  = 2,
    NM_IFACE_TYPE_LOOPBACK  = 3,
    NM_IFACE_TYPE_VPN       = 4,
    NM_IFACE_TYPE_UNKNOWN   = 5
} nm_iface_type_t;

typedef enum {
    NM_IFACE_STATE_DOWN     = 0,
    NM_IFACE_STATE_UP       = 1,
    NM_IFACE_STATE_CONFIG   = 2,
    NM_IFACE_STATE_TESTING  = 3
} nm_iface_state_t;

typedef enum {
    NM_PROXY_MODE_NONE      = 0,
    NM_PROXY_MODE_MANUAL    = 1,
    NM_PROXY_MODE_AUTO      = 2
} nm_proxy_mode_t;

typedef struct {
    char            name[IFNAMSIZ];
    nm_iface_type_t type;
    nm_iface_state_t state;
    struct in_addr  ip_addr;
    struct in_addr  netmask;
    struct in_addr  gateway;
    char            mac_addr[18];
} net_if_t;

typedef struct {
    struct in_addr  address;
    struct in_addr  netmask;
    struct in_addr  gateway;
    struct in_addr  dns_servers[DHCP_MAX_DNS_SERVERS];
    int             mtu;
} ip_config_t;

typedef struct {
    struct in_addr  destination;
    int             prefix;
    struct in_addr  gateway;
    char            interface[IFNAMSIZ];
} route_t;

typedef struct {
    nm_proxy_mode_t mode;
    char            http_proxy[256];
    char            https_proxy[256];
    char            pac_url[256];
} proxy_config_t;

typedef struct {
    net_if_t        interfaces[NM_MAX_INTERFACES];
    int             iface_count;
    route_t         routes[NM_MAX_ROUTES];
    int             route_count;
    proxy_config_t  proxy;
    int             connectivity_checked;
    int             is_connected;
} nm_context_t;

int nm_init(void);
void nm_shutdown(void);
int nm_get_interfaces(net_if_t *interfaces, int *count);
int nm_get_ipv4_config(const char *if_name, ip_config_t *config);
int nm_set_dns(const char *if_name, struct in_addr *dns_servers, int count);
int nm_add_route(const char *dest, int prefix, const char *gateway, const char *iface);
int nm_remove_route(const char *dest, int prefix);
int nm_get_default_route(route_t *route);
int nm_set_proxy(nm_proxy_mode_t mode, const proxy_config_t *config);
int nm_get_proxy(proxy_config_t *config);
int nm_check_connectivity(void);
const char *nm_connectivity_url(void);

#endif /* _MOBILE_NETWORK_MGR_H_ */