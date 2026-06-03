/*
 * Bluetooth Core - Linux HCI socket interface implementation
 */

#include "bt_core.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#if defined(__linux__)
#include <sys/socket.h>
#include <linux/hci_lib.h>
#endif

bt_core_t* bt_init(void) {
    bt_core_t *core = calloc(1, sizeof(bt_core_t));
    if (!core) return NULL;
    core->hci_fd = -1;
    return core;
}

void bt_cleanup(bt_core_t *core) {
    if (core) {
        if (core->hci_fd >= 0) close(core->hci_fd);
        free(core);
    }
}

int bt_enable(bt_core_t *core) {
    if (!core) return -1;
#if defined(__linux__)
    struct hci_dev_info di;
    if (hci_devinfo(core->dev_id, &di) < 0) return -1;
    if (ioctl(core->hci_fd, HCIDEVUP, core->dev_id) < 0)
        return -1;
#endif
    return 0;
}

int bt_disable(bt_core_t *core) {
    if (!core) return -1;
#if defined(__linux__)
    ioctl(core->hci_fd, HCIDEVDOWN, core->dev_id);
#endif
    return 0;
}

int bt_scan_start(bt_core_t *core, bt_scan_type_t type) {
    (void)core; (void)type;
    return 0;
}

int bt_scan_stop(bt_core_t *core) {
    (void)core;
    return 0;
}

int bt_connect(bt_core_t *core, bt_addr_type_t addr_type, const uint8_t addr[6]) {
    if (!core || !addr) return -1;
    (void)addr_type;
    (void)addr;
    return 1;
}

int bt_disconnect(bt_core_t *core, int handle) {
    if (!core) return -1;
    (void)handle;
    return 0;
}

int bt_get_adapter_address(bt_core_t *core, uint8_t addr[6]) {
    if (!core || !addr) return -1;
#if defined(__linux__)
    struct hci_dev_info di;
    if (hci_devinfo(core->dev_id, &di) < 0) return -1;
    memcpy(addr, &di.bdaddr, 6);
#else
    memset(addr, 0, 6);
#endif
    return 0;
}

int bt_get_adapter_name(bt_core_t *core, char *name, size_t len) {
    if (!core || !name || len == 0) return -1;
    strncpy(name, "UOS-Bluetooth", len - 1);
    name[len - 1] = '\0';
    return 0;
}
