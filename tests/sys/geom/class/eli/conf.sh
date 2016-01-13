#!/bin/sh
# $FreeBSD$

class="eli"
base=`basename $0`

# We need to use linear probing in order to detect the first available md(4)
# device instead of using mdconfig -a -t, because geli(8) attachs md(4) devices
no=0
while [ -c /dev/md$no ]; do
	: $(( no += 1 ))
done

geli_test_cleanup()
{
	[ -c /dev/md${no}.eli ] && geli detach md${no}.eli
	mdconfig -d -u $no
}
trap geli_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh
