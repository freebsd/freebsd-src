/*
 * Bluetooth Manager - High-level Bluetooth management
 * BSD licensed
 */

#ifndef _MOBILE_BLUETOOTH_MGR_H_
#define _MOBILE_BLUETOOTH_MGR_H_

#include "bt_core.h"
#include "../services/networkd.h"

#define BT_MGR_MAX_DEVICES      32
#define BT_MGR_MAX_NAME_LEN     64
#define BT_MGR_MAX_UUID_LEN     64
#define BT_MGR_MAX_SERVICES     8

typedef enum {
    BT_DEVICE_TYPE_UNKNOWN    = 0,
    BT_DEVICE_TYPE_PHONE      = 1,
    BT_DEVICE_TYPE_HEADSET    = 2,
    BT_DEVICE_TYPE_SPKR       = 3,
    BT_DEVICE_TYPE_MOUSE      = 4,
    BT_DEVICE_TYPE_KEYBOARD   = 5,
    BT_DEVICE_TYPE_TABLET     = 6,
    BT_DEVICE_TYPE_PC         = 7,
    BT_DEVICE_TYPE_WATCH      = 8
} bt_device_type_t;

typedef enum {
    BT_DEVICE_STATE_DISCONNECTED = 0,
    BT_DEVICE_STATE_CONNECTING  = 1,
    BT_DEVICE_STATE_CONNECTED   = 2,
    BT_DEVICE_STATE_PAIRED      = 3,
    BT_DEVICE_STATE_ERROR       = 4
} bt_device_state_t;

typedef enum {
    BT_PROFILE_A2DP         = 0,
    BT_PROFILE_HFP            = 1,
    BT_PROFILE_AVRCP          = 2,
    BT_PROFILE_SPP            = 3,
    BT_PROFILE_HID            = 4,
    BT_PROFILE_PAN            = 5
} bt_profile_t;

typedef struct {
    uint8_t         address[6];
    char            name[BT_MGR_MAX_NAME_LEN + 1];
    bt_device_type_t type;
    bt_device_state_t state;
    int             rssi;
    int             paired;
    int             trusted;
    char            uuids[BT_MGR_MAX_UUID_LEN + 1];
} bt_device_t;

typedef struct {
    char            address[18];
    bt_profile_t    profiles[BT_MGR_MAX_SERVICES];
    int             profile_count;
} bt_connection_t;

typedef struct {
    bt_core_t      *core;
    bt_device_t    devices[BT_MGR_MAX_DEVICES];
    int            device_count;
    int            scanning;
    int            auto_connect_a2dp;
    int            auto_answer_hfp;
    char           adapter_name[BT_MGR_MAX_NAME_LEN + 1];
    int            discoverable;
} bt_mgr_context_t;

int bt_mgr_init(void);
void bt_mgr_shutdown(void);
int bt_mgr_get_devices(bt_device_t *devices, int *count);
int bt_mgr_pair(const uint8_t address[6]);
int bt_mgr_connect(const uint8_t address[6]);
int bt_mgr_disconnect(const uint8_t address[6]);
int bt_mgr_forget(const uint8_t address[6]);
int bt_mgr_set_name(const char *name);
int bt_mgr_set_power(int on);
int bt_mgr_start_scan(bt_scan_type_t type);
int bt_mgr_stop_scan(void);
int bt_mgr_enable_profile(bt_profile_t profile, int auto_enable);
int bt_mgr_ble_advertise(const char *name, const char *service_uuid, int enabled);
int bt_mgr_ble_start_scan(void);
int bt_mgr_ble_stop_scan(void);

#endif /* _MOBILE_BLUETOOTH_MGR_H_ */