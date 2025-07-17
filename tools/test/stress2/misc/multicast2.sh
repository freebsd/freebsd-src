#!/bin/sh

# D19886 Fix numerous refcount bugs in multicast

# Page fault in in6_pcbpurgeif0+0xc8 seen.
# https://people.freebsd.org/~pho/stress/log/mmacy035.txt
# Test scenario by mmacy

# Fixed by r349507

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -x /usr/local/bin/mDNSResponderPosix ] ||
    { echo "mDNSResponder not installed"; exit 0; }

(cd ../testcases/swap; ./swap -t 2m -i 50 -v -h -l 100) &
sleep 2

service mdnsd onestart
ifconfig vtnet0 delete 2>/dev/null
ifconfig epair create
ifconfig epair0a 0/24 up
ifconfig epair0a destroy
timeout 2m service mdnsd onestop

while pkill swap; do :; done
wait
