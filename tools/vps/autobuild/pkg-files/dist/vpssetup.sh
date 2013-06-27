#!/bin/sh -x

kldload if_vps
kldload vps_dev
kldload vps_ddb
kldload vps_libdump
kldload vps_snapst
kldload vps_restore
kldload vps_suspend
kldload vps_account
kldload vpsfs
kldload sysvmsg
kldload sysvsem
kldload sysvshm

ifconfig vps0 create
ifconfig vps0 up

sysctl -w net.inet.ip.forwarding=1
sysctl -w net.inet6.ip6.forwarding=1

sysctl -w debug.vps_core_debug=0
sysctl -w debug.vps_if_debug=0
sysctl -w debug.vps_dev_debug=0
sysctl -w debug.vps_user_debug=0
sysctl -w debug.vps_snapst_debug=0
sysctl -w debug.vps_restore_debug=0
sysctl -w debug.vps_restore_ktrace=0
sysctl -w debug.vps_account_debug=0
sysctl -w debug.vps_vpsfs_debug=0

exit 0
