/*
 * Connectivity Framework test program
 */

#include <stdio.h>
#include <string.h>
#include "wifi.h"
#include "dhcp.h"
#include "network_mgr.h"
#include "cellular.h"
#include "bluetooth_mgr.h"
#include "vpn.h"
#include "nfc.h"

int main(int argc, char **argv)
{
    wifi_status_t wifi_status;
    wifi_scan_results_t scan_results;
    dhcp_lease_t dhcp_lease;
    net_if_t interfaces[16];
    int iface_count, ret;
    cell_info_t cell_info;
    bt_device_t bt_devices[32];
    int bt_count;
    vpn_status_t vpn_status;
    nfc_tag_t nfc_tags[8];
    int nfc_count;
    
    printf("Mobile OS Connectivity Framework Test\n");
    printf("=====================================\n\n");
    
    /* WiFi tests */
    printf("WiFi:\n");
    if (wifi_init() == 0) {
        printf("  wifi_init() - OK\n");
        
        if (wifi_scan() == 0) {
            printf("  wifi_scan() - OK\n");
        }
        
        wifi_get_status(&wifi_status);
        printf("  wifi_get_status() - State: %d\n", wifi_status.state);
    } else {
        printf("  wifi_init() - FAILED (no wpa_supplicant)\n");
    }
    
    /* DHCP tests */
    printf("\nDHCP:\n");
    ret = dhcp_init("wlan0", NULL, NULL);
    printf("  dhcp_init() - %s\n", ret == 0 ? "OK" : "FAILED");
    
    /* Network Manager tests */
    printf("\nNetwork Manager:\n");
    nm_init();
    nm_get_interfaces(interfaces, &iface_count);
    printf("  nm_get_interfaces() - Found %d interfaces\n", iface_count);
    
    /* Cellular tests */
    printf("\nCellular:\n");
    ret = cell_init("/dev/ttyUSB2");
    printf("  cell_init() - %s\n", ret == 0 ? "OK" : "FAILED");
    
    /* Bluetooth Manager tests */
    printf("\nBluetooth:\n");
    if (bt_mgr_init() == 0) {
        printf("  bt_mgr_init() - OK\n");
        bt_mgr_get_devices(bt_devices, &bt_count);
        printf("  bt_mgr_get_devices() - Found %d devices\n", bt_count);
    } else {
        printf("  bt_mgr_init() - FAILED (no bluetooth)\n");
    }
    
    /* VPN tests */
    printf("\nVPN:\n");
    vpn_init();
    printf("  vpn_init() - OK\n");
    
    /* NFC tests */
    printf("\nNFC:\n");
    ret = nfc_init();
    printf("  nfc_init() - %s\n", ret == 0 ? "OK" : "FAILED");
    
    printf("\nAll tests completed.\n");
    return 0;
}