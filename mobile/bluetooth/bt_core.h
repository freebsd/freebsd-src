/*
 * bt_core.h - Bluetooth Core API
 */

#ifndef _BT_CORE_H_
#define _BT_CORE_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BT_SCAN_BREDR = 0x01,
    BT_SCAN_LE   = 0x02,
    BT_SCAN_BOTH = 0x03,
} bt_scan_type_t;

typedef enum {
    BT_ADDR_PUBLIC  = 0x00,
    BT_ADDR_RANDOM  = 0x01,
    BT_ADDR_PUBLIC_ID = 0x02,
} bt_addr_type_t;

typedef struct bt_core bt_core_t;

bt_core_t* bt_init(void);
void bt_cleanup(bt_core_t *core);

int bt_enable(bt_core_t *core);
int bt_disable(bt_core_t *core);
int bt_scan_start(bt_core_t *core, bt_scan_type_t type);
int bt_scan_stop(bt_core_t *core);
int bt_connect(bt_core_t *core, bt_addr_type_t addr_type, const uint8_t addr[6]);
int bt_disconnect(bt_core_t *core, int handle);

int bt_get_adapter_address(bt_core_t *core, uint8_t addr[6]);
int bt_get_adapter_name(bt_core_t *core, char *name, size_t len);

#endif
