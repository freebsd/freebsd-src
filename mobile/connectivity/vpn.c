/*
 * VPN Framework - Virtual Private Network management implementation
 * BSD licensed
 */

#include "vpn.h"
#include "../services/networkd.h"
#include "../network_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <errno.h>

static vpn_context_t vpn_ctx;
static vpn_status_t vpn_status_cache[16];

int vpn_init(void)
{
    memset(&vpn_ctx, 0, sizeof(vpn_ctx));
    memset(vpn_status_cache, 0, sizeof(vpn_status_cache));
    return 0;
}

void vpn_shutdown(void)
{
    for (int i = 0; i < 16; i++) {
        if (vpn_status_cache[i].state != VPN_STATE_DISCONNECTED) {
            vpn_disconnect(vpn_status_cache[i].vpn_id);
        }
    }
    memset(&vpn_ctx, 0, sizeof(vpn_ctx));
}

int vpn_create(const char *vpn_id, vpn_type_t type, const vpn_config_t *config)
{
    char path[256];
    FILE *fp;
    
    if (!vpn_id || !config)
        return -1;
    
    snprintf(path, sizeof(path), "/etc/vpn/%s.%s",
             vpn_id, type == VPN_TYPE_WIREGUARD ? "conf" : 
                   type == VPN_TYPE_OPENVPN ? "ovpn" : "conf");
    
    fp = fopen(path, "w");
    if (!fp)
        return -1;
    
    switch (type) {
    case VPN_TYPE_WIREGUARD:
        fprintf(fp, "[Interface]\n");
        fprintf(fp, "PrivateKey = %s\n", config->wg.private_key);
        fprintf(fp, "Address = %s\n", config->wg.allowed_ips);
        fprintf(fp, "DNS = %s\n", config->wg.dns_servers[0]);
        
        fprintf(fp, "[Peer]\n");
        fprintf(fp, "PublicKey = %s\n", config->wg.public_key);
        fprintf(fp, "Endpoint = %s\n", config->wg.endpoint);
        fprintf(fp, "AllowedIPs = %s\n", config->wg.allowed_ips);
        if (config->wg.keepalive > 0)
            fprintf(fp, "PersistentKeepalive = %d\n", config->wg.keepalive);
        break;
        
    case VPN_TYPE_OPENVPN:
        fprintf(fp, "client\n");
        fprintf(fp, "dev tun\n");
        fprintf(fp, "proto udp\n");
        fprintf(fp, "remote %s\n", config->ovpn.config_path);
        fprintf(fp, "ca %s\n", config->ovpn.ca_cert);
        fprintf(fp, "cert %s\n", config->ovpn.client_cert);
        fprintf(fp, "key %s\n", config->ovpn.client_key);
        break;
        
    case VPN_TYPE_IKEV2:
        fprintf(fp, "conn %s\n", vpn_id);
        fprintf(fp, "    keyexchange=ikev2\n");
        fprintf(fp, "    left=%s\n", config->ikev2.server);
        fprintf(fp, "    leftid=%s\n", config->ikev2.identity);
        break;
    }
    
    fclose(fp);
    
    strlcpy(vpn_status_cache[vpn_ctx.vpn_count].vpn_id, vpn_id,
            sizeof(vpn_status_cache[vpn_ctx.vpn_count].vpn_id));
    vpn_status_cache[vpn_ctx.vpn_count].type = type;
    vpn_status_cache[vpn_ctx.vpn_count].state = VPN_STATE_DISCONNECTED;
    snprintf(vpn_status_cache[vpn_ctx.vpn_count].interface,
             sizeof(vpn_status_cache[vpn_ctx.vpn_count].interface),
             "tun%s", vpn_id);
    
    vpn_ctx.vpn_count++;
    
    return 0;
}

int vpn_connect(const char *vpn_id)
{
    char cmd[512], path[256];
    FILE *fp;
    int ret = -1;
    
    for (int i = 0; i < vpn_ctx.vpn_count; i++) {
        if (strcmp(vpn_status_cache[i].vpn_id, vpn_id) == 0) {
            vpn_status_cache[i].state = VPN_STATE_CONNECTING;
            
            switch (vpn_status_cache[i].type) {
            case VPN_TYPE_WIREGUARD:
                snprintf(cmd, sizeof(cmd),
                         "wg-quick up %s 2>/dev/null || ip link set %s up",
                         vpn_id, vpn_status_cache[i].interface);
                ret = system(cmd);
                break;
                
            case VPN_TYPE_OPENVPN:
                snprintf(path, sizeof(path), "/etc/vpn/%s.ovpn", vpn_id);
                snprintf(cmd, sizeof(cmd), "openvpn --config %s --daemon", path);
                ret = system(cmd);
                break;
                
            case VPN_TYPE_IKEV2:
                snprintf(cmd, sizeof(cmd), "ipsec up %s", vpn_id);
                ret = system(cmd);
                break;
            }
            
            if (ret == 0) {
                vpn_status_cache[i].state = VPN_STATE_CONNECTED;
                vpn_ctx.always_on_vpn_active = 1;
            } else {
                vpn_status_cache[i].state = VPN_STATE_ERROR;
            }
            
            return ret;
        }
    }
    
    return -1;
}

int vpn_disconnect(const char *vpn_id)
{
    char cmd[256];
    
    for (int i = 0; i < vpn_ctx.vpn_count; i++) {
        if (strcmp(vpn_status_cache[i].vpn_id, vpn_id) == 0) {
            switch (vpn_status_cache[i].type) {
            case VPN_TYPE_WIREGUARD:
                snprintf(cmd, sizeof(cmd), "wg-quick down %s", vpn_id);
                break;
            case VPN_TYPE_OPENVPN:
                snprintf(cmd, sizeof(cmd), "pkill -f \"openvpn.*%s\"", vpn_id);
                break;
            case VPN_TYPE_IKEV2:
                snprintf(cmd, sizeof(cmd), "ipsec down %s", vpn_id);
                break;
            }
            
            system(cmd);
            vpn_status_cache[i].state = VPN_STATE_DISCONNECTED;
            memset(&vpn_status_cache[i].local_ip, 0, sizeof(vpn_status_cache[i].local_ip));
            vpn_ctx.always_on_vpn_active = 0;
            
            return 0;
        }
    }
    
    return -1;
}

int vpn_get_status(const char *vpn_id, vpn_status_t *status)
{
    if (!vpn_id || !status)
        return -1;
    
    for (int i = 0; i < vpn_ctx.vpn_count; i++) {
        if (strcmp(vpn_status_cache[i].vpn_id, vpn_id) == 0) {
            *status = vpn_status_cache[i];
            
            char path[128];
            snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", status->interface);
            FILE *fp = fopen(path, "r");
            if (fp) {
                char state[16];
                if (fgets(state, sizeof(state), fp)) {
                    if (strncmp(state, "down", 4) == 0)
                        status->state = VPN_STATE_DISCONNECTED;
                }
                fclose(fp);
            }
            
            return 0;
        }
    }
    
    return -1;
}

int vpn_list_status(vpn_status_t *status, int *count)
{
    if (!status || !count)
        return -1;
    
    int copy_count = *count > vpn_ctx.vpn_count ? vpn_ctx.vpn_count : *count;
    
    for (int i = 0; i < copy_count; i++) {
        status[i] = vpn_status_cache[i];
    }
    
    *count = copy_count;
    return 0;
}

int vpn_delete(const char *vpn_id)
{
    char path[256];
    int idx = -1;
    
    for (int i = 0; i < vpn_ctx.vpn_count; i++) {
        if (strcmp(vpn_status_cache[i].vpn_id, vpn_id) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0)
        return -1;
    
    vpn_disconnect(vpn_id);
    
    snprintf(path, sizeof(path), "/etc/vpn/%s.conf", vpn_id);
    unlink(path);
    snprintf(path, sizeof(path), "/etc/vpn/%s.ovpn", vpn_id);
    unlink(path);
    
    for (int i = idx; i < vpn_ctx.vpn_count - 1; i++) {
        vpn_status_cache[i] = vpn_status_cache[i + 1];
    }
    vpn_ctx.vpn_count--;
    
    return 0;
}

int vpn_set_always_on(const char *vpn_id, int enabled)
{
    for (int i = 0; i < vpn_ctx.vpn_count; i++) {
        if (strcmp(vpn_status_cache[i].vpn_id, vpn_id) == 0) {
            vpn_status_cache[i].always_on = enabled;
            return 0;
        }
    }
    return -1;
}

int vpn_generate_wireguard_keys(char *private_key, char *public_key, size_t key_len)
{
    FILE *fp;
    char cmd[256];
    
    snprintf(cmd, sizeof(cmd), "wg genkey | tee /dev/stdout | wg pubkey");
    
    fp = popen(cmd, "r");
    if (!fp)
        return -1;
    
    if (fread(private_key, 1, key_len, fp) < 0) {
        pclose(fp);
        return -1;
    }
    
    pclose(fp);
    
    snprintf(cmd, sizeof(cmd), "wg pubkey");
    fp = popen(cmd, "w");
    if (fp) {
        fprintf(fp, "%s\n", private_key);
        fflush(fp);
        fread(public_key, 1, key_len, fp);
        pclose(fp);
    }
    
    return 0;
}

int vpn_bring_interface_up(const char *if_name, const char *ip, int mtu)
{
    char cmd[256];
    
    snprintf(cmd, sizeof(cmd), "ip link set %s up", if_name);
    system(cmd);
    
    if (ip) {
        snprintf(cmd, sizeof(cmd), "ip address add %s dev %s", ip, if_name);
        system(cmd);
    }
    
    if (mtu > 0) {
        snprintf(cmd, sizeof(cmd), "ip link set mtu %d dev %s", mtu, if_name);
        system(cmd);
    }
    
    return 0;
}

int vpn_bring_interface_down(const char *if_name)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip link set %s down", if_name);
    return system(cmd);
}