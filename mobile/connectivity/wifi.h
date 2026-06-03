/*
 * WiFi Management - WiFi stack interface
 * BSD licensed
 */

#ifndef _MOBILE_WIFI_H_
#define _MOBILE_WIFI_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#define WIFI_MAX_SSID_LEN       32
#define WIFI_MAX_BSSID_LEN      18
#define WIFI_MAX_SCAN_RESULTS   64
#define WPA_CONTROL_PATH        "/var/run/wpa_supplicant/wlan0"

typedef enum {
    WIFI_SECURITY_OPEN        = 0,
    WIFI_SECURITY_WEP         = 1,
    WIFI_SECURITY_WPA_PSK     = 2,
    WIFI_SECURITY_WPA2_PSK    = 3,
    WIFI_SECURITY_WPA3_SAE    = 4,
    WIFI_SECURITY_WPA_ENTERPRISE = 5
} wifi_security_t;

typedef enum {
    WIFI_STATE_DISCONNECTED   = 0,
    WIFI_STATE_SCANNING       = 1,
    WIFI_STATE_CONNECTING     = 2,
    WIFI_STATE_CONNECTED      = 3,
    WIFI_STATE_ERROR          = 4
} wifi_state_t;

typedef enum {
    WPS_MODE_PBC              = 0,
    WPS_MODE_PIN              = 1,
    WPS_MODE_VIRTUAL_PIN      = 2
} wps_mode_t;

typedef struct {
    char            ssid[WIFI_MAX_SSID_LEN + 1];
    char            bssid[WIFI_MAX_BSSID_LEN];
    int             channel;
    int             signal_strength;
    wifi_security_t security;
} wifi_ap_t;

typedef struct {
    wifi_state_t    state;
    char            ssid[WIFI_MAX_SSID_LEN + 1];
    char            ip_addr[INET_ADDRSTRLEN];
    int             signal_strength;
    char            bssid[WIFI_MAX_BSSID_LEN];
} wifi_status_t;

typedef struct {
    wifi_ap_t       aps[WIFI_MAX_SCAN_RESULTS];
    int             count;
} wifi_scan_results_t;

typedef struct {
    char            network_name[WIFI_MAX_SSID_LEN + 1];
    char            password[64];
    wifi_security_t security;
    int             priority;
} wpa_network_config_t;

typedef struct {
    int             control_fd;
    wifi_state_t    state;
    int             scan_count;
    wifi_ap_t       scan_results[WIFI_MAX_SCAN_RESULTS];
} wifi_context_t;

int wifi_init(void);
void wifi_shutdown(void);
int wifi_scan(void);
int wifi_get_scan_results(wifi_scan_results_t *results);
int wifi_connect(const char *ssid, const char *password, wifi_security_t security);
int wifi_disconnect(void);
int wifi_get_status(wifi_status_t *status);
int wifi_reconnect(void);
int wifi_forget_network(const char *ssid);
int wifi_wps_push_button(void);
int wifi_wps_pin(const char *pin);
int wifi_save_config(void);

int wpa_ctrl_attach(const char *path);
int wpa_ctrl_detach(int fd);
int wpa_ctrl_request(int fd, const char *cmd, char *reply, size_t reply_len);
int wpa_generate_config(const wpa_network_config_t *configs, int count, const char *path);

#endif /* _MOBILE_WIFI_H_ */