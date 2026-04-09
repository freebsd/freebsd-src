#!/bin/sh
# ---- UOS MediaTek: Ethernet Test ----
# Tests RGMII Ethernet on Radxa NIO 12L (RTL8211F PHY via mtk_eth).
# On QEMU: tests VirtIO NIC (vtnet0).

VERBOSE="${1:-}"
PASS=0; FAIL=0
p() { printf "\033[0;32m  PASS\033[0m %s\n" "$*"; PASS=$((PASS+1)); }
f() { printf "\033[0;31m  FAIL\033[0m %s\n" "$*"; FAIL=$((FAIL+1)); }
i() { printf "\033[0;34m  .....\033[0m %s\n" "$*"; }
NOTE() { printf "\033[1;33m  NOTE\033[0m %s\n" "$*"; }

PING_TARGET="8.8.8.8"
PING_LOCAL="192.168.0.1"

i "Checking Ethernet controller in dmesg..."
if dmesg | grep -qiE "eth|gbe|vtnet|rgmii|phy[0-9]"; then
    p "Ethernet: controller found in dmesg"
    [ "$VERBOSE" = "--verbose" ] && dmesg | grep -iE "eth|gbe|vtnet|rgmii" | head -10
else
    f "Ethernet: no Ethernet controller in dmesg"
fi

i "Detecting network interfaces..."
IFACES=$(ifconfig -l 2>/dev/null | tr ' ' '\n' | grep -vE '^lo' | head -5)
if [ -n "$IFACES" ]; then
    p "Ethernet: interfaces found: $IFACES"
else
    f "Ethernet: no network interfaces found"
fi

i "Checking interface status..."
for iface in $IFACES; do
    STATUS=$(ifconfig "$iface" 2>/dev/null | grep -c "status: active" || true)
    if [ "$STATUS" -gt 0 ]; then
        p "Ethernet: $iface is active (link up)"
        ACTIVE_IFACE="$iface"
    else
        NOTE "Ethernet: $iface not active"
    fi
done

i "Checking IP address assignment (DHCP)..."
HAS_IP=0
for iface in $IFACES; do
    IP=$(ifconfig "$iface" 2>/dev/null | grep 'inet ' | awk '{print $2}' | head -1)
    if [ -n "$IP" ]; then
        p "Ethernet: $iface has IP: $IP"
        HAS_IP=1
    fi
done
[ "$HAS_IP" -eq 0 ] && NOTE "Ethernet: no IP addresses assigned yet (DHCP may be pending)"

i "Checking MTK RGMII TX delay (real HW: expected 2030ps)..."
if dmesg | grep -qi "tx-delay.*2030\|mediatek.*2030"; then
    p "Ethernet: MTK RGMII TX delay 2030ps confirmed"
elif dmesg | grep -qi "vtnet\|QEMU"; then
    NOTE "Ethernet: QEMU VirtIO - RGMII TX delay not applicable"
else
    NOTE "Ethernet: RGMII TX delay not confirmed in dmesg (check mtk_eth driver)"
fi

i "Testing ping to local gateway..."
GW=$(route -n get default 2>/dev/null | grep gateway | awk '{print $2}')
if [ -n "$GW" ]; then
    if ping -c 3 -W 2 "$GW" >/dev/null 2>&1; then
        p "Ethernet: ping to gateway $GW succeeded"
    else
        f "Ethernet: ping to gateway $GW failed"
    fi
else
    NOTE "Ethernet: no default gateway found - skipping ping test"
fi

i "Testing DNS resolution..."
if host freebsd.org >/dev/null 2>&1 || drill freebsd.org >/dev/null 2>&1; then
    p "Ethernet: DNS resolution working"
else
    NOTE "Ethernet: DNS not working (check /etc/resolv.conf)"
fi

i "Measuring network throughput (iperf3 if available)..."
if command -v iperf3 >/dev/null 2>&1; then
    NOTE "Ethernet: iperf3 available - run manually: iperf3 -c <server>"
else
    NOTE "Ethernet: install iperf3 for throughput testing: pkg install iperf3"
fi

echo "Ethernet: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
