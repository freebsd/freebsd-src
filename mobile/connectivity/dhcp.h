/*
 * DHCP Client - Dynamic Host Configuration Protocol client
 * BSD licensed
 */

#ifndef _MOBILE_DHCP_H_
#define _MOBILE_DHCP_H_

#include <sys/types.h>
#include <netinet/in.h>

#define DHCP_MAX_DNS_SERVERS   4
#define DHCP_CLIENT_PORT       68
#define DHCP_SERVER_PORT       67

typedef enum {
    DHCP_STATE_INIT           = 0,
    DHCP_STATE_SELECTING      = 1,
    DHCP_STATE_REQUESTING     = 2,
    DHCP_STATE_BOUND          = 3,
    DHCP_STATE_RENEWING       = 4,
    DHCP_STATE_REBINDING      = 5,
    DHCP_STATE_RELEASED       = 6
} dhcp_state_t;

typedef struct {
    struct in_addr  ip_address;
    struct in_addr  subnet_mask;
    struct in_addr  gateway;
    struct in_addr  dns_servers[DHCP_MAX_DNS_SERVERS];
    uint32_t        lease_time;
    uint32_t        renewal_time;
    uint32_t        rebinding_time;
    time_t          lease_start;
    time_t          lease_expire;
} dhcp_lease_t;

typedef void (*dhcp_event_cb_t)(const char *if_name, dhcp_state_t state, 
                                const dhcp_lease_t *lease, void *user_ctx);

typedef struct {
    int             sock_fd;
    dhcp_state_t    state;
    char            if_name[IFNAMSIZ];
    dhcp_lease_t    lease;
    dhcp_event_cb_t event_cb;
    void           *user_ctx;
    uint32_t        transaction_id;
    int             server_port;
    struct in_addr  server_ip;
    time_t          last_request;
} dhcp_context_t;

int dhcp_init(const char *if_name, dhcp_event_cb_t cb, void *user_ctx);
void dhcp_shutdown(void);
int dhcp_discover(void);
int dhcp_request(void);
int dhcp_release(void);
int dhcp_renew(void);
int dhcp_get_lease(dhcp_lease_t *lease);
int dhcp_set_option(const char *option, const void *value, size_t len);
const char *dhcp_state_string(dhcp_state_t state);

#endif /* _MOBILE_DHCP_H_ */