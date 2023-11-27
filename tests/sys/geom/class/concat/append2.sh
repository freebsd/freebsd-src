#!/bin/sh

# A basic regression test for gconcat append using "gconcat label",
# i.e., automatic mode.

gconcat_check_size()
{
    local actual expected name

    name=$1
    expected=$2

    actual=$(diskinfo /dev/concat/${name} | awk '{print $3}')
    if [ $actual -eq $expected ]; then
        echo "ok - Size is ${actual}"
    else
        echo "not ok - Size is ${actual}"
    fi
}

. `dirname $0`/conf.sh

echo '1..4'

ss=512

f1=$(mktemp) || exit 1
truncate -s $((1024 * 1024 + $ss)) $f1
f2=$(mktemp) || exit 1
truncate -s $((1024 * 1024 + $ss)) $f2
f3=$(mktemp) || exit 1
truncate -s $((1024 * 1024 + $ss)) $f3

attach_md us0 -f $f1 -S $ss || exit 1
attach_md us1 -f $f2 -S $ss || exit 1
attach_md us2 -f $f3 -S $ss || exit 1

gconcat label $name /dev/$us0 /dev/$us1 || exit 1
devwait

# We should have a 2MB device.  Add another disk and verify that the
# reported size of the concat device grows accordingly.  A sector from
# each disk is reserved for the metadata sector.
gconcat_check_size "${name}" $((2 * 1024 * 1024))
gconcat append $name /dev/$us2 || exit 1
gconcat_check_size "${name}" $((3 * 1024 * 1024))

copy=$(mktemp) || exit 1
dd if=/dev/random of=$copy bs=1M count=3 || exit 1
dd if=$copy of=/dev/concat/${name} || exit 1

# Stop the concat device and destroy the backing providers.
gconcat stop ${name} || exit 1
detach_md $us0
detach_md $us1
detach_md $us2

# Re-create the providers and verify that the concat device comes
# back and that the data is still there.
attach_md us0 -f $f1 -S $ss || exit 1
attach_md us1 -f $f2 -S $ss || exit 1
attach_md us2 -f $f3 -S $ss || exit 1

devwait

# Make sure that the
if [ -c /dev/concat/${name} ]; then
    echo "ok - concat device was instantiated"
else
    echo "not ok - concat device was instantiated"
fi

if cmp -s $copy /dev/concat/${name}; then
    echo "ok - Data was persisted across gconcat stop"
else
    echo "not ok - Data was persisted across gconcat stop"
fi
