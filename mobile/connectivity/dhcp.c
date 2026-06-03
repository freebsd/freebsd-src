/*
 * DHCP Client - Dynamic Host Configuration Protocol client implementation
 * BSD licensed
 */

#include "dhcp.h"
#include "../services/networkd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

static dhcp_context_t dhcp_ctx;

#define DHCP_OP_REQUEST         1
#define DHCP_OP_REPLY            2
#define DHCP_HTYPE_ETHER        1
#define DHCP_HLEN_ETHER         6
#define DHCP_OPTION_MAGIC       0x63826363

#define DHCP_OPTION_PAD         0
#define DHCP_OPTION_SUBNET      1
#define DHCP_OPTION_ROUTER      3
#define DHCP_OPTION_DNS         6
#define DHCP_OPTION_LEASE_TIME   51
#define DHCP_OPTION_MSG_TYPE    53
#define DHCP_OPTION_SERVER_ID   54
#define DHCP_OPTION_REQUEST_IP  50
#define DHCP_OPTION_END        255

#define DHCP_MSG_DISCOVER       1
#define DHCP_MSG_REQUEST        3
#define DHCP_MSG_RELEASE        7

#pragma pack(push, 1)
struct dhcp_packet {
    uint8_t     op;
    uint8_t     htype;
    uint8_t     hlen;
    uint8_t     hops;
    uint32_t    xid;
    uint16_t    secs;
    uint16_t    flags;
    struct in_addr  ciaddr;
    struct in_addr  yiaddr;
    struct in_addr  siaddr;
    struct in_addr  giaddr;
    uint8_t     chaddr[16];
    char        sname[64];
    char        file[128];
    uint32_t    magic;
    uint8_t     options[308];
};
#pragma pack(pop)

static uint32_t dhcp_generate_xid(void)
{
    static uint32_t seed = 0;
    if (seed == 0)
        seed = (uint32_t)time(NULL) ^ getpid();
    return ++seed;
}

static int dhcp_send_packet(int type, const struct in_addr *server_ip,
                          const struct in_addr *req_ip, const char *server_hw)
{
    struct dhcp_packet pkt;
    struct sockaddr_in addr;
    
    memset(&pkt, 0, sizeof(pkt));
    pkt.op = DHCP_OP_REQUEST;
    pkt.htype = DHCP_HTYPE_ETHER;
    pkt.hlen = DHCP_HLEN_ETHER;
    pkt.xid = dhcp_ctx.transaction_id;
    pkt.magic = htonl(DHCP_OPTION_MAGIC);
    
    if (req_ip)
        pkt.ciaddr = *req_ip;
    
    if (type == DHCP_MSG_DISCOVER || (server_ip && strcmp(server_hw, "unicast") != 0)) {
        pkt.flags = htons(0x8000);
    }
    
    int opt_idx = 0;
    pkt.options[opt_idx++] = DHCP_OPTION_MSG_TYPE;
    pkt.options[opt_idx++] = 1;
    pkt.options[opt_idx++] = type;
    
    pkt.options[opt_idx++] = DHCP_OPTION_END;
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DHCP_SERVER_PORT);
    addr.sin_addr.s_addr = (type == DHCP_MSG_DISCOVER || !server_ip) ?
                         htonl(INADDR_BROADCAST) : server_ip->s_addr;
    
    return sendto(dhcp_ctx.sock_fd, &pkt, sizeof(pkt), 0,
                  (struct sockaddr *)&addr, sizeof(addr));
}

static int dhcp_process_options(const uint8_t *options, ssize_t len)
{
    const uint8_t *ptr = options;
    int dns_count = 0;
    
    while ((ptr - options) < len && *ptr != DHCP_OPTION_END) {
        uint8_t opt = *ptr++;
        
        if (opt == DHCP_OPTION_PAD)
            continue;
        
        uint8_t opt_len = *ptr++;
        
        switch (opt) {
        case DHCP_OPTION_SUBNET:
            if (opt_len >= 4)
                memcpy(&dhcp_ctx.lease.subnet_mask, ptr, 4);
            break;
        case DHCP_OPTION_ROUTER:
            if (opt_len >= 4)
                memcpy(&dhcp_ctx.lease.gateway, ptr, 4);
            break;
        case DHCP_OPTION_DNS:
            if (opt_len >= 4 && dns_count < DHCP_MAX_DNS_SERVERS) {
                int copy_count = opt_len / 4;
                if (copy_count > DHCP_MAX_DNS_SERVERS)
                    copy_count = DHCP_MAX_DNS_SERVERS;
                memcpy(dhcp_ctx.lease.dns_servers, ptr, copy_count * 4);
                dns_count += copy_count;
            }
            break;
        case DHCP_OPTION_LEASE_TIME:
            if (opt_len >= 4) {
                uint32_t lt = 0;
                for (int i = 0; i < opt_len && i < 4; i++)
                    lt = (lt << 8) | ptr[i];
                dhcp_ctx.lease.lease_time = lt;
                dhcp_ctx.lease.lease_expire = time(NULL) + lt;
            }
            break;
        case DHCP_OPTION_SERVER_ID:
            if (opt_len >= 4)
                memcpy(&dhcp_ctx.server_ip, ptr, 4);
            break;
        }
        ptr += opt_len;
    }
    
    return 0;
}

static int dhcp_receive_packet(void)
{
    uint8_t buf[1024];
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    ssize_t n;
    
    n = recvfrom(dhcp_ctx.sock_fd, buf, sizeof(buf), 0,
                 (struct sockaddr *)&addr, &addrlen);
    if (n < 0)
        return -1;
    
    struct dhcp_packet *pkt = (struct dhcp_packet *)buf;
    
    if (pkt->xid != dhcp_ctx.transaction_id)
        return -1;
    
    uint8_t *opts = pkt->options;
    int msg_type = 0;
    
    for (int i = 0; i < n - (int)sizeof(struct dhcp_packet) + 308; ) {
        uint8_t opt = opts[i++];
        if (opt == DHCP_OPTION_END)
            break;
        if (opt == DHCP_OPTION_PAD)
            continue;
        uint8_t opt_len = opts[i++];
        if (opt == DHCP_OPTION_MSG_TYPE && opt_len >= 1)
            msg_type = opts[i];
        i += opt_len;
    }
    
    switch (msg_type) {
    case 2: /* DHCPOFFER */
        dhcp_ctx.state = DHCP_STATE_REQUESTING;
        memcpy(&dhcp_ctx.lease.ip_address, &pkt->yiaddr, 4);
        dhcp_process_options(opts, n - (int)sizeof(struct dhcp_packet));
        dhcp_ctx.last_request = time(NULL);
        break;
    case 5: /* DHCPACK */
        dhcp_ctx.state = DHCP_STATE_BOUND;
        memcpy(&dhcp_ctx.lease.ip_address, &pkt->yiaddr, 4);
        dhcp_process_options(opts, n - (int)sizeof(struct dhcp_packet));
        dhcp_ctx.last_request = time(NULL);
        break;
    }
    
    if (dhcp_ctx.event_cb)
        dhcp_ctx.event_cb(dhcp_ctx.if_name, dhcp_ctx.state, &dhcp_ctx.lease, dhcp_ctx.user_ctx);
    
    return 0;
}

int dhcp_init(const char *if_name, dhcp_event_cb_t cb, void *user_ctx)
{
    int broadcast = 1;
    
    memset(&dhcp_ctx, 0, sizeof(dhcp_ctx));
    strlcpy(dhcp_ctx.if_name, if_name, sizeof(dhcp_ctx.if_name));
    dhcp_ctx.event_cb = cb;
    dhcp_ctx.user_ctx = user_ctx;
    dhcp_ctx.state = DHCP_STATE_INIT;
    
    dhcp_ctx.sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (dhcp_ctx.sock_fd < 0)
        return -1;
    
    if (setsockopt(dhcp_ctx.sock_fd, SOL_SOCKET, SO_REUSEADDR,
                   &broadcast, sizeof(broadcast)) < 0) {
        close(dhcp_ctx.sock_fd);
        return -1;
    }
    
    return 0;
}

void dhcp_shutdown(void)
{
    if (dhcp_ctx.sock_fd >= 0) {
        close(dhcp_ctx.sock_fd);
        dhcp_ctx.sock_fd = -1;
    }
}

int dhcp_discover(void)
{
    dhcp_ctx.transaction_id = dhcp_generate_xid();
    dhcp_ctx.state = DHCP_STATE_SELECTING;
    dhcp_ctx.last_request = time(NULL);
    
    return dhcp_send_packet(DHCP_MSG_DISCOVER, NULL, NULL, "broadcast");
}

int dhcp_request(void)
{
    dhcp_ctx.transaction_id = dhcp_generate_xid();
    dhcp_ctx.state = DHCP_STATE_REQUESTING;
    dhcp_ctx.last_request = time(NULL);
    
    return dhcp_send_packet(DHCP_MSG_REQUEST, &dhcp_ctx.server_ip,
                          &dhcp_ctx.lease.ip_address, "broadcast");
}

int dhcp_release(void)
{
    uint8_t release_pkt[342];
    
    memset(release_pkt, 0, sizeof(release_pkt));
    release_pkt[0] = DHCP_OP_REQUEST;
    release_pkt[1] = DHCP_HTYPE_ETHER;
    release_pkt[2] = DHCP_HLEN_ETHER;
    *(uint32_t *)(release_pkt + 4) = dhcp_ctx.transaction_id;
    *(uint32_t *)(release_pkt + 28) = dhcp_ctx.lease.ip_address.s_addr;
    *(uint32_t *)(release_pkt + 12) = ntohl(DHCP_OPTION_MAGIC);
    release_pkt[236] = DHCP_OPTION_MSG_TYPE;
    release_pkt[238] = 1;
    release_pkt[239] = 7; /* DHCPRELEASE */
    
    dhcp_ctx.state = DHCP_STATE_RELEASED;
    
    return 0;
}

int dhcp_renew(void)
{
    dhcp_ctx.transaction_id = dhcp_generate_xid();
    dhcp_ctx.state = DHCP_STATE_RENEWING;
    dhcp_ctx.last_request = time(NULL);
    
    return dhcp_send_packet(DHCP_MSG_REQUEST, &dhcp_ctx.server_ip,
                          &dhcp_ctx.lease.ip_address, "unicast");
}

int dhcp_get_lease(dhcp_lease_t *lease)
{
    if (!lease)
        return -1;
    
    *lease = dhcp_ctx.lease;
    return 0;
}

int dhcp_set_option(const char *option, const void *value, size_t len)
{
    return 0;
}

const char *dhcp_state_string(dhcp_state_t state)
{
    switch (state) {
    case DHCP_STATE_INIT: return "INIT";
    case DHCP_STATE_SELECTING: return "SELECTING";
    case DHCP_STATE_REQUESTING: return "REQUESTING";
    case DHCP_STATE_BOUND: return "BOUND";
    case DHCP_STATE_RENEWING: return "RENEWING";
    case DHCP_STATE_REBINDING: return "REBINDING";
    case DHCP_STATE_RELEASED: return "RELEASED";
    default: return "UNKNOWN";
    }
}