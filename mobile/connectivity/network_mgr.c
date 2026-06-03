/*
 * Network Manager - Network interface and routing management implementation
 * BSD licensed
 */

#include "network_mgr.h"
#include "../services/networkd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <errno.h>

static nm_context_t nm_ctx;

int nm_init(void)
{
    memset(&nm_ctx, 0, sizeof(nm_ctx));
    nm_ctx.proxy.mode = NM_PROXY_MODE_NONE;
    
    return networkd_init();
}

void nm_shutdown(void)
{
    networkd_shutdown();
}

int nm_get_interfaces(net_if_t *interfaces, int *count)
{
    struct iface_info *ifaces = NULL;
    int if_count = 0;
    int ret;
    
    ret = networkd_enum_interfaces(&ifaces, &if_count);
    if (ret < 0)
        return -1;
    
    *count = if_count > NM_MAX_INTERFACES ? NM_MAX_INTERFACES : if_count;
    
    for (int i = 0; i < *count; i++) {
        interfaces[i].type = NM_IFACE_TYPE_UNKNOWN;
        strlcpy(interfaces[i].name, ifaces[i].name, sizeof(interfaces[i].name));
        interfaces[i].ip_addr.s_addr = inet_addr(ifaces[i].ip_addr);
        strlcpy(interfaces[i].mac_addr, ifaces[i].mac_addr,
                sizeof(interfaces[i].mac_addr));
        
        if (strncmp(interfaces[i].name, "eth", 3) == 0)
            interfaces[i].type = NM_IFACE_TYPE_ETHERNET;
        else if (strncmp(interfaces[i].name, "wlan", 4) == 0)
            interfaces[i].type = NM_IFACE_TYPE_WIFI;
        else if (strncmp(interfaces[i].name, "ppp", 3) == 0 ||
                 strncmp(interfaces[i].name, "wwan", 4) == 0)
            interfaces[i].type = NM_IFACE_TYPE_CELLULAR;
        else if (strncmp(interfaces[i].name, "lo", 2) == 0)
            interfaces[i].type = NM_IFACE_TYPE_LOOPBACK;
        else if (strncmp(interfaces[i].name, "tun", 3) == 0 ||
                 strncmp(interfaces[i].name, "wg", 2) == 0)
            interfaces[i].type = NM_IFACE_TYPE_VPN;
        
        interfaces[i].state = ifaces[i].state == IFACE_STATE_UP ?
                              NM_IFACE_STATE_UP : NM_IFACE_STATE_DOWN;
    }
    
    free(ifaces);
    return 0;
}

int nm_get_ipv4_config(const char *if_name, ip_config_t *config)
{
    struct iface_info *ifaces = NULL;
    int if_count = 0;
    int ret;
    
    ret = networkd_enum_interfaces(&ifaces, &if_count);
    if (ret < 0)
        return -1;
    
    for (int i = 0; i < if_count; i++) {
        if (strncmp(ifaces[i].name, if_name, IFNAMSIZ) == 0) {
            config->address.s_addr = inet_addr(ifaces[i].ip_addr);
            config->netmask.s_addr = inet_addr(ifaces[i].netmask);
            strlcpy(config->gateway, ifaces[i].gateway, sizeof(config->gateway));
            config->mtu = 1500;
            
            free(ifaces);
            return 0;
        }
    }
    
    free(ifaces);
    return -1;
}

int nm_set_dns(const char *if_name, struct in_addr *dns_servers, int count)
{
    char dns_str[256] = "";
    int i;
    
    for (i = 0; i < count && i < NM_MAX_DNS_SERVERS; i++) {
        char tmp[INET_ADDRSTRLEN + 2];
        snprintf(tmp, sizeof(tmp), "%s%s%s", dns_str, 
                 i > 0 ? " " : "", inet_ntoa(dns_servers[i]));
        strlcpy(dns_str, tmp, sizeof(dns_str));
    }
    
    return networkd_dns_set(dns_str);
}

int nm_add_route(const char *dest, int prefix, const char *gateway, const char *iface)
{
    char dest_str[INET_ADDRSTRLEN];
    
    if (!dest) {
        dest_str[0] = '\0';
    } else {
        strlcpy(dest_str, dest, sizeof(dest_str));
    }
    
    return networkd_route_add(dest_str[0] ? dest_str : "default", gateway);
}

int nm_remove_route(const char *dest, int prefix)
{
    net_if_t interfaces[NM_MAX_INTERFACES];
    int count = 0;
    int ret;
    
    ret = nm_get_interfaces(interfaces, &count);
    if (ret < 0)
        return -1;
    
    return 0;
}

int nm_get_default_route(route_t *route)
{
    struct iface_info *ifaces = NULL;
    int if_count = 0;
    int ret;
    
    ret = networkd_enum_interfaces(&ifaces, &if_count);
    if (ret < 0)
        return -1;
    
    for (int i = 0; i < if_count; i++) {
        if (ifaces[i].state == IFACE_STATE_UP &&
            strncmp(ifaces[i].name, "lo", 2) != 0) {
            route->destination.s_addr = 0;
            route->prefix = 0;
            route->gateway.s_addr = inet_addr(ifaces[i].gateway);
            strlcpy(route->interface, ifaces[i].name, sizeof(route->interface));
            
            free(ifaces);
            return 0;
        }
    }
    
    free(ifaces);
    return -1;
}

int nm_set_proxy(nm_proxy_mode_t mode, const proxy_config_t *config)
{
    if (mode != NM_PROXY_MODE_NONE && !config)
        return -1;
    
    nm_ctx.proxy.mode = mode;
    
    if (config) {
        strlcpy(nm_ctx.proxy.http_proxy, config->http_proxy,
                sizeof(nm_ctx.proxy.http_proxy));
        strlcpy(nm_ctx.proxy.https_proxy, config->https_proxy,
                sizeof(nm_ctx.proxy.https_proxy));
        strlcpy(nm_ctx.proxy.pac_url, config->pac_url,
                sizeof(nm_ctx.proxy.pac_url));
    }
    
    return 0;
}

int nm_get_proxy(proxy_config_t *config)
{
    if (!config)
        return -1;
    
    *config = nm_ctx.proxy;
    return 0;
}

int nm_check_connectivity(void)
{
    int sock, ret;
    struct sockaddr_in addr;
    char request[256], response[1024];
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;
    
    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        close(sock);
        return -1;
    }
    
    snprintf(request, sizeof(request),
             "GET /generate_204 HTTP/1.1\r\n"
             "Host: connectivitycheck.gstatic.com\r\n"
             "Connection: close\r\n\r\n");
    
    send(sock, request, strlen(request), 0);
    ret = recv(sock, response, sizeof(response) - 1, 0);
    close(sock);
    
    if (ret > 0) {
        response[ret] = '\0';
        if (strstr(response, "204") || strstr(response, "200")) {
            nm_ctx.is_connected = 1;
            return 0;
        }
    }
    
    nm_ctx.is_connected = 0;
    return -1;
}

const char *nm_connectivity_url(void)
{
    return CONNECTIVITY_CHECK_URL;
}