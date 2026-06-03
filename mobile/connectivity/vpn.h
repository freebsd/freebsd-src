/*
 * VPN Framework - Virtual Private Network management
 * BSD licensed
 */

#ifndef _MOBILE_VPN_H_
#define _MOBILE_VPN_H_

#include <sys/types.h>
#include <netinet/in.h>

#define VPN_MAX_CONFIG_LEN      1024
#define VPN_MAX_PROXIES       8
#define VPN_MAX_APPS           16

typedef enum {
    VPN_TYPE_WIREGUARD       = 0,
    VPN_TYPE_OPENVPN          = 1,
    VPN_TYPE_IKEV2            = 2
} vpn_type_t;

typedef enum {
    VPN_STATE_DISCONNECTED    = 0,
    VPN_STATE_CONNECTING      = 1,
    VPN_STATE_CONNECTED       = 2,
    VPN_STATE_ERROR           = 3
} vpn_state_t;

typedef enum {
    VPN_PROXY_MODE_NONE       = 0,
    VPN_PROXY_MODE_URL        = 1,
    VPN_PROXY_MODE_BYPASS     = 2
} vpn_proxy_mode_t;

typedef struct {
    char            server[256];
    char            endpoint[256];
    char            public_key[64];
    char            private_key[64];
    char            pre_shared_key[128];
    char            username[64];
    char            password[64];
    int             mtu;
    int             keepalive;
    char            allowed_ips[256];
    char            dns_servers[VPN_MAX_PROXIES][INET_ADDRSTRLEN];
    int             dns_count;
} wireguard_config_t;

typedef struct {
    char            config_path[256];
    char            ca_cert[256];
    char            client_cert[256];
    char            client_key[256];
    int             verify_cert;
} openvpn_config_t;

typedef struct {
    char            server[256];
    char            identity[64];
    char            ca_cert[256];
    char            client_cert[256];
    char            client_key[256];
} ikev2_config_t;

typedef union {
    wireguard_config_t      wg;
    openvpn_config_t        ovpn;
    ikev2_config_t          ikev2;
} vpn_config_t;

typedef struct {
    char            vpn_id[64];
    vpn_type_t      type;
    vpn_state_t     state;
    char            interface[16];
    struct in_addr  local_ip;
    struct in_addr  remote_ip;
    char            apps[VPN_MAX_APPS][64];
    int             app_count;
    vpn_proxy_mode_t proxy_mode;
    int             always_on;
} vpn_status_t;

typedef struct {
    int             initialized;
    int             vpn_count;
    int             always_on_vpn_active;
} vpn_context_t;

int vpn_init(void);
void vpn_shutdown(void);
int vpn_create(const char *vpn_id, vpn_type_t type, const vpn_config_t *config);
int vpn_connect(const char *vpn_id);
int vpn_disconnect(const char *vpn_id);
int vpn_get_status(const char *vpn_id, vpn_status_t *status);
int vpn_list_status(vpn_status_t *status, int *count);
int vpn_delete(const char *vpn_id);
int vpn_set_always_on(const char *vpn_id, int enabled);
int vpn_generate_wireguard_keys(char *private_key, char *public_key, size_t key_len);
int vpn_bring_interface_up(const char *if_name, const char *ip, int mtu);
int vpn_bring_interface_down(const char *if_name);

#endif /* _MOBILE_VPN_H_ */