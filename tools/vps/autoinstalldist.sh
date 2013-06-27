#!/bin/sh

fail() {
        echo "FAILURE"
        exit 1
}

tar xzpf dist.tgz || fail
rm -rf /boot/kernel || fail
mv boot/kernel /boot/ || fail
mv usr/sbin/vpsctl /usr/sbin/vpsctl || fail
mv sbin/mount_vpsfs /sbin/mount_vpsfs || fail

echo "SUCCESS"
exit 0

# EOF
