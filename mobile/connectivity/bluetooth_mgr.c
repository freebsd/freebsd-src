/*
 * Bluetooth Manager - High-level Bluetooth management implementation
 * BSD licensed
 */

#include "bluetooth_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static bt_mgr_context_t bt_mgr_ctx;

static void bt_mgr_device_callback(const char *line, void *user)
{
    /* Process device discovery events */
}

int bt_mgr_init(void)
{
    memset(&bt_mgr_ctx, 0, sizeof(bt_mgr_ctx));
    
    bt_mgr_ctx.core = bt_init();
    if (!bt_mgr_ctx.core)
        return -1;
    
    bt_enable(bt_mgr_ctx.core);
    
    strlcpy(bt_mgr_ctx.adapter_name, "MobileOS-BT", sizeof(bt_mgr_ctx.adapter_name));
    
    return 0;
}

void bt_mgr_shutdown(void)
{
    if (bt_mgr_ctx.core) {
        bt_scan_stop(bt_mgr_ctx.core);
        bt_disable(bt_mgr_ctx.core);
        bt_cleanup(bt_mgr_ctx.core);
        bt_mgr_ctx.core = NULL;
    }
}

int bt_mgr_get_devices(bt_device_t *devices, int *count)
{
    int fd, ret, i;
    char buf[4096], *line, *saveptr;
    
    if (!devices || !count)
        return -1;
    
    fd = open("/var/lib/bluetooth/devices", O_RDONLY);
    if (fd < 0) {
        *count = 0;
        return 0;
    }
    
    ret = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    
    if (ret < 0) {
        *count = 0;
        return -1;
    }
    buf[ret] = '\0';
    
    *count = 0;
    
    for (line = strtok_r(buf, "\n", &saveptr); line && *count < BT_MGR_MAX_DEVICES;
         line = strtok_r(NULL, "\n", &saveptr)) {
        uint8_t addr[6];
        char name[BT_MGR_MAX_NAME_LEN + 1];
        
        if (sscanf(line, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx %64s",
                   &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5],
                   name) == 7) {
            memcpy(devices[*count].address, addr, 6);
            strlcpy(devices[*count].name, name, sizeof(devices[*count].name));
            devices[*count].paired = 1;
            devices[*count].state = BT_DEVICE_STATE_PAIRED;
            devices[*count].trusted = 1;
            (*count)++;
        }
    }
    
    return 0;
}

int bt_mgr_pair(const uint8_t address[6])
{
    char cmd[128];
    
    if (!address)
        return -1;
    
    for (int i = 0; i < BT_MGR_MAX_DEVICES; i++) {
        if (memcmp(bt_mgr_ctx.devices[i].address, address, 6) == 0) {
            bt_mgr_ctx.devices[i].state = BT_DEVICE_STATE_PAIRED;
            bt_mgr_ctx.devices[i].paired = 1;
            return 0;
        }
    }
    
    snprintf(cmd, sizeof(cmd), "BT_PAIR_%02x:%02x:%02x:%02x:%02x:%02x",
             address[0], address[1], address[2], address[3], address[4], address[5]);
    
    return 0;
}

int bt_mgr_connect(const uint8_t address[6])
{
    int handle;
    
    if (!address || !bt_mgr_ctx.core)
        return -1;
    
    handle = bt_connect(bt_mgr_ctx.core, ADDR_TYPE_PUBLIC, address);
    if (handle < 0)
        return -1;
    
    for (int i = 0; i < BT_MGR_MAX_DEVICES; i++) {
        if (memcmp(bt_mgr_ctx.devices[i].address, address, 6) == 0) {
            bt_mgr_ctx.devices[i].state = BT_DEVICE_STATE_CONNECTED;
            break;
        }
    }
    
    return 0;
}

int bt_mgr_disconnect(const uint8_t address[6])
{
    int handle;
    
    if (!address)
        return -1;
    
    for (int i = 0; i < BT_MGR_MAX_DEVICES; i++) {
        if (memcmp(bt_mgr_ctx.devices[i].address, address, 6) == 0) {
            if (bt_mgr_ctx.devices[i].state == BT_DEVICE_STATE_CONNECTED) {
                bt_disconnect(bt_mgr_ctx.core, handle);
                bt_mgr_ctx.devices[i].state = BT_DEVICE_STATE_PAIRED;
            }
            return 0;
        }
    }
    
    return -1;
}

int bt_mgr_forget(const uint8_t address[6])
{
    char path[128];
    
    if (!address)
        return -1;
    
    snprintf(path, sizeof(path), "/var/lib/bluetooth/linkkeys/%02x%02x%02x%02x%02x%02x",
             address[0], address[1], address[2], address[3], address[4], address[5]);
    
    unlink(path);
    
    for (int i = 0; i < BT_MGR_MAX_DEVICES; i++) {
        if (memcmp(bt_mgr_ctx.devices[i].address, address, 6) == 0) {
            memset(&bt_mgr_ctx.devices[i], 0, sizeof(bt_mgr_ctx.devices[i]));
            bt_mgr_ctx.device_count--;
            break;
        }
    }
    
    return 0;
}

int bt_mgr_set_name(const char *name)
{
    if (!name)
        return -1;
    
    strlcpy(bt_mgr_ctx.adapter_name, name, sizeof(bt_mgr_ctx.adapter_name));
    
    return 0;
}

int bt_mgr_set_power(int on)
{
    if (!bt_mgr_ctx.core)
        return -1;
    
    if (on)
        return bt_enable(bt_mgr_ctx.core);
    else
        return bt_disable(bt_mgr_ctx.core);
}

int bt_mgr_start_scan(bt_scan_type_t type)
{
    if (!bt_mgr_ctx.core)
        return -1;
    
    bt_mgr_ctx.scanning = 1;
    return bt_scan_start(bt_mgr_ctx.core, type);
}

int bt_mgr_stop_scan(void)
{
    if (!bt_mgr_ctx.core)
        return -1;
    
    bt_mgr_ctx.scanning = 0;
    return bt_scan_stop(bt_mgr_ctx.core);
}

int bt_mgr_enable_profile(bt_profile_t profile, int auto_enable)
{
    switch (profile) {
    case BT_PROFILE_A2DP:
        bt_mgr_ctx.auto_connect_a2dp = auto_enable;
        break;
    case BT_PROFILE_HFP:
        bt_mgr_ctx.auto_answer_hfp = auto_enable;
        break;
    default:
        return -1;
    }
    
    return 0;
}

int bt_mgr_ble_advertise(const char *name, const char *service_uuid, int enabled)
{
    if (!bt_mgr_ctx.core)
        return -1;
    
    if (!enabled)
        return 0;
    
    if (!name && !service_uuid)
        return -1;
    
    return 0;
}

int bt_mgr_ble_start_scan(void)
{
    return bt_mgr_start_scan(SCAN_TYPE_LE);
}

int bt_mgr_ble_stop_scan(void)
{
    return bt_mgr_stop_scan();
}