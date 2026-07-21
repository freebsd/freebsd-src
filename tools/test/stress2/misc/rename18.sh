#!/bin/sh

# Test scenario from:
# Bug 295826 - kern_renameat() infinite loop
# D57453: Avoid infinite loop in renameat(2)

set -u
root=/root
cd $root
mkdir -p $root/test_loop/a
mkdir -p $root/test_loop/b
echo '123' > $root/test_loop/b/file
touch $root/test_loop/a/file
mount_nullfs -o rw -o nocache -o noatime -o noexec -o nosuid $root/test_loop/b/file $root/test_loop/a/file
echo '123' > /tmp/file

# The "mv" would spin
mv /tmp/file $root/test_loop/a/file

umount $root/test_loop/a/file
rm -rf /tmp/file $root/test_loop
exit
