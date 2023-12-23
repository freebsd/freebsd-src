#!/bin/sh

# A basic regression test for gconcat append using "gconcat create",
# i.e., manual mode.

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

echo '1..3'

attach_md us0 -t malloc -s 1M || exit 1
attach_md us1 -t malloc -s 1M || exit 1
attach_md us2 -t malloc -s 1M || exit 1

gconcat create $name /dev/$us0 /dev/$us1 || exit 1
devwait

# We should have a 2MB device.  Add another disk and verify that the
# reported size of the concat device grows accordingly.
gconcat_check_size "${name}" $((2 * 1024 * 1024))
gconcat append $name /dev/$us2 || exit 1
gconcat_check_size "${name}" $((3 * 1024 * 1024))

# Write some data and make sure that we can read it back.
tmpfile=$(mktemp) || exit 1
dd if=/dev/random of=$tmpfile bs=1M count=3 || exit 1
dd if=$tmpfile of=/dev/concat/${name} || exit 1
if cmp -s $tmpfile /dev/concat/${name}; then
    echo "ok - Data matches what was written"
else
    echo "not ok - Data matches what was written"
fi
