# Connectivity Framework

BSD licensed high-level connectivity management for Mobile OS.

## Components

### WiFi Management (`wifi.h`/`wifi.c`)
- `wifi_init()` - Initialize WiFi stack and connect to wpa_supplicant control interface
- `wifi_scan()` - Scan for available access points
- `wifi_connect(ssid, password, security)` - Connect to AP (WPA2-PSK, WPA3, open, WEP)
- `wifi_disconnect()` - Disconnect from current AP
- `wifi_get_status()` - Get current connection status
- `wifi_wps_push_button()` - WPS push-button pairing
- `wpa_generate_config()` - Generate wpa_supplicant.conf per network

### DHCP Client (`dhcp.h`/`dhcp.c`)
- `dhcp_init(if_name, cb)` - Initialize DHCP client with event callback
- `dhcp_discover()` - Send DHCPDISCOVER
- `dhcp_request()` - Send DHCPREQUEST
- `dhcp_release()` - Release DHCP lease
- `dhcp_renew()` - Renew DHCP lease
- `dhcp_get_lease()` - Get current lease information
- UDP socket-based implementation for DHCP state machine

### Network Manager (`network_mgr.h`/`network_mgr.c`)
- `nm_init()` - Initialize and enumerate interfaces (ethernet, wifi, cellular, loopback)
- `nm_get_interfaces()` - List all network interfaces
- `nm_get_ipv4_config()` - Get IPv4 configuration for interface
- `nm_set_dns()` - Set DNS servers for interface
- `nm_add_route()` - Add route entry
- `nm_get_default_route()` - Get default gateway route
- `nm_set_proxy()` - Configure proxy (none/manual/auto)
- `nm_check_connectivity()` - HTTP connectivity check

### Cellular Modem (`cellular.h`/`cellular.c`)
- `cell_init(device_path)` - Initialize modem via AT/QMI
- `cell_get_info()` - Get IMEI, IMSI, operator, signal, registration
- `cell_register_network()` - Manual network registration
- `cell_send_sms()` / `cell_get_sms()` - SMS operations
- `cell_data_connect(apn)` - PDP context activation
- AT command parser (OK, ERROR, +CME ERROR, +CMS ERROR)
- Modem power management

### Bluetooth Manager (`bluetooth_mgr.h`/`bluetooth_mgr.c`)
- `bt_mgr_init()` - Initialize and start scanning
- `bt_mgr_get_devices()` - List paired/nearby devices
- `bt_mgr_pair()` / `bt_mgr_connect()` - Pairing and connection
- `bt_mgr_forget()` - Unpair and remove device
- Profile management (A2DP auto-connect, HFP auto-answer)
- BLE advertising support

### VPN Framework (`vpn.h`/`vpn.c`)
- `vpn_init()` - Initialize VPN subsystem
- `vpn_create(type, config)` - Create WireGuard/OpenVPN/IKEv2 config
- `vpn_connect()` / `vpn_disconnect()` - VPN tunnel control
- `vpn_generate_wireguard_keys()` - Key generation
- Always-on VPN support

### NFC Framework (`nfc.h`/`nfc.c`)
- `nfc_init()` - Initialize NFC adapter
- `nfc_start_discovery()` - Poll for NFC-A/B/F/V, ISO 14443, ISO 15693 tags
- `nfc_read_ndef()` / `nfc_write_ndef()` - NDEF message handling
- Peer-to-peer mode support
- HCE (Host Card Emulation)

## Build

```bash
make            # Build static library (libconnectivity.a)
make test       # Build and run tests
make install    # Install to /usr/local
```

## Integration

Includes integration points with:
- `mobile/services/networkd.h` - Network daemon
- `mobile/frameworks/ipc/ipc.h` - IPC framework

Link against `libconnectivity.a` for all connectivity features.