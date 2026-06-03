/*
 * WiFi Management - WiFi stack implementation
 * BSD licensed
 */

#include "wifi.h"
#include "../services/networkd.h"
#include "../frameworks/ipc/ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

static wifi_context_t wifi_ctx;

int wifi_init(void)
{
    memset(&wifi_ctx, 0, sizeof(wifi_ctx));
    wifi_ctx.state = WIFI_STATE_DISCONNECTED;
    
    return wpa_ctrl_attach(WPA_CONTROL_PATH);
}

void wifi_shutdown(void)
{
    if (wifi_ctx.control_fd >= 0) {
        wpa_ctrl_detach(wifi_ctx.control_fd);
        close(wifi_ctx.control_fd);
        wifi_ctx.control_fd = -1;
    }
}

int wifi_scan(void)
{
    char reply[4096];
    int ret;
    
    wifi_ctx.state = WIFI_STATE_SCANNING;
    wifi_ctx.scan_count = 0;
    
    ret = wpa_ctrl_request(wifi_ctx.control_fd, "SCAN", reply, sizeof(reply));
    if (ret < 0)
        return -1;
    
    ret = wpa_ctrl_request(wifi_ctx.control_fd, "SCAN_RESULTS", reply, sizeof(reply));
    if (ret < 0)
        return -1;
    
    return 0;
}

int wifi_get_scan_results(wifi_scan_results_t *results)
{
    char reply[4096];
    char *line, *saveptr;
    int ret;
    
    ret = wpa_ctrl_request(wifi_ctx.control_fd, "SCAN_RESULTS", reply, sizeof(reply));
    if (ret < 0)
        return -1;
    
    results->count = 0;
    line = strtok_r(reply, "\n", &saveptr);
    while (line && results->count < WIFI_MAX_SCAN_RESULTS) {
        unsigned int bssid[6], freq, level, flags;
        char ssid[33];
        int chan;
        
        if (sscanf(line, "%6x:%6x:%6x:%6x:%6x:%6x %d %d %32s %x",
                   &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5],
                   &chan, &level, ssid, &flags) == 9) {
            wifi_ap_t *ap = &results->aps[results->count];
            snprintf(ap->bssid, sizeof(ap->bssid), "%02x:%02x:%02x:%02x:%02x:%02x",
                     bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
            strlcpy(ap->ssid, ssid, sizeof(ap->ssid));
            ap->channel = chan;
            ap->signal_strength = level;
            ap->security = (flags & 0x10) ? WIFI_SECURITY_WPA2_PSK :
                           (flags & 0x02) ? WIFI_SECURITY_WEP : WIFI_SECURITY_OPEN;
            results->count++;
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    
    return 0;
}

int wifi_connect(const char *ssid, const char *password, wifi_security_t security)
{
    wpa_network_config_t config;
    char cmd[256];
    char reply[256];
    int ret;
    
    if (!ssid)
        return -1;
    
    wifi_ctx.state = WIFI_STATE_CONNECTING;
    strlcpy(config.network_name, ssid, sizeof(config.network_name));
    strlcpy(config.password, password ? password : "", sizeof(config.password));
    config.security = security;
    config.priority = 0;
    
    ret = wpa_generate_config(&config, 1, "/etc/wpa_supplicant.conf");
    if (ret < 0)
        return -1;
    
    snprintf(cmd, sizeof(cmd), "ADD_NETWORK");
    if (wpa_ctrl_request(wifi_ctx.control_fd, cmd, reply, sizeof(reply)) < 0)
        return -1;
    
    int network_id = atoi(reply);
    
    snprintf(cmd, sizeof(cmd), "SET_NETWORK %d ssid \"%s\"", network_id, ssid);
    ret = wpa_ctrl_request(wifi_ctx.control_fd, cmd, reply, sizeof(reply));
    if (ret < 0)
        return -1;
    
    if (security != WIFI_SECURITY_OPEN && password) {
        snprintf(cmd, sizeof(cmd), "SET_NETWORK %d psk \"%s\"", network_id, password);
        wpa_ctrl_request(wifi_ctx.control_fd, cmd, reply, sizeof(reply));
    }
    
    snprintf(cmd, sizeof(cmd), "SELECT_NETWORK %d", network_id);
    if (wpa_ctrl_request(wifi_ctx.control_fd, cmd, reply, sizeof(reply)) < 0)
        return -1;
    
    return 0;
}

int wifi_disconnect(void)
{
    char reply[256];
    wifi_ctx.state = WIFI_STATE_DISCONNECTED;
    return wpa_ctrl_request(wifi_ctx.control_fd, "DISCONNECT", reply, sizeof(reply));
}

int wifi_get_status(wifi_status_t *status)
{
    char reply[256];
    char *saveptr, *line;
    
    if (!status)
        return -1;
    
    wpa_ctrl_request(wifi_ctx.control_fd, "STATUS", reply, sizeof(reply));
    
    strlcpy(status->ssid, "", sizeof(status->ssid));
    status->state = wifi_ctx.state;
    status->signal_strength = 0;
    
    for (line = strtok_r(reply, "\n", &saveptr); line; line = strtok_r(NULL, "\n", &saveptr)) {
        if (strncmp(line, "ssid=", 5) == 0) {
            strlcpy(status->ssid, line + 5, sizeof(status->ssid));
        } else if (strncmp(line, "wpa_state=", 10) == 0) {
            const char *state = line + 10;
            if (strcmp(state, "COMPLETED") == 0)
                status->state = WIFI_STATE_CONNECTED;
            else if (strcmp(state, "SCANNING") == 0)
                status->state = WIFI_STATE_SCANNING;
            else if (strcmp(state, "ASSOCIATING") == 0)
                status->state = WIFI_STATE_CONNECTING;
        } else if (strncmp(line, "signal=", 7) == 0) {
            status->signal_strength = atoi(line + 7);
        } else if (strncmp(line, "bssid=", 6) == 0) {
            strlcpy(status->bssid, line + 6, sizeof(status->bssid));
        } else if (strncmp(line, "ip_address=", 11) == 0) {
            strlcpy(status->ip_addr, line + 11, sizeof(status->ip_addr));
        }
    }
    
    return 0;
}

int wifi_reconnect(void)
{
    char reply[256];
    return wpa_ctrl_request(wifi_ctx.control_fd, "RECONNECT", reply, sizeof(reply));
}

int wifi_forget_network(const char *ssid)
{
    char cmd[128], reply[256];
    snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK \"%s\"", ssid);
    return wpa_ctrl_request(wifi_ctx.control_fd, cmd, reply, sizeof(reply));
}

int wifi_wps_push_button(void)
{
    char reply[256];
    wifi_ctx.state = WIFI_STATE_CONNECTING;
    return wpa_ctrl_request(wifi_ctx.control_fd, "WPS_PBC", reply, sizeof(reply));
}

int wifi_wps_pin(const char *pin)
{
    char cmd[128], reply[256];
    if (!pin) {
        snprintf(cmd, sizeof(cmd), "WPS_PIN any");
    } else {
        snprintf(cmd, sizeof(cmd), "WPS_PIN %s", pin);
    }
    wifi_ctx.state = WIFI_STATE_CONNECTING;
    return wpa_ctrl_request(wifi_ctx.control_fd, cmd, reply, sizeof(reply));
}

int wifi_save_config(void)
{
    char reply[256];
    return wpa_ctrl_request(wifi_ctx.control_fd, "SAVE_CONFIG", reply, sizeof(reply));
}

int wpa_ctrl_attach(const char *path)
{
    int fd;
    
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
    
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    
    wifi_ctx.control_fd = fd;
    return 0;
}

int wpa_ctrl_detach(int fd)
{
    char reply[256];
    return wpa_ctrl_request(fd, "DETACH", reply, sizeof(reply));
}

int wpa_ctrl_request(int fd, const char *cmd, char *reply, size_t reply_len)
{
    ssize_t n;
    
    if (write(fd, cmd, strlen(cmd)) < 0)
        return -1;
    
    if (write(fd, "\n", 1) < 0)
        return -1;
    
    n = read(fd, reply, reply_len - 1);
    if (n < 0)
        return -1;
    
    reply[n] = '\0';
    return 0;
}

int wpa_generate_config(const wpa_network_config_t *configs, int count, const char *path)
{
    FILE *fp;
    int i;
    
    fp = fopen(path, "w");
    if (!fp)
        return -1;
    
    fprintf(fp, "# WPA Supplicant configuration - generated\n");
    fprintf(fp, "ctrl_interface=/var/run/wpa_supplicant\n");
    fprintf(fp, "ap_scan=1\n\n");
    
    for (i = 0; i < count; i++) {
        const wpa_network_config_t *c = &configs[i];
        fprintf(fp, "network={\n");
        fprintf(fp, "\tssid=\"%s\"\n", c->network_name);
        fprintf(fp, "\tpriority=%d\n", c->priority);
        
        switch (c->security) {
        case WIFI_SECURITY_OPEN:
            fprintf(fp, "\tkey_mgmt=NONE\n");
            break;
        case WIFI_SECURITY_WEP:
            fprintf(fp, "\tkey_mgmt=NONE\n");
            fprintf(fp, "\n%s\n", c->password);
            break;
        case WIFI_SECURITY_WPA_PSK:
        case WIFI_SECURITY_WPA2_PSK:
        case WIFI_SECURITY_WPA3_SAE:
            fprintf(fp, "\tpsk=\"%s\"\n", c->password);
            break;
        }
        
        fprintf(fp, "}\n\n");
    }
    
    fclose(fp);
    return 0;
}