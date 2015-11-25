#!/bin/sh
# $FreeBSD$

class="uzip"
base=`basename $0`
mntpoint=$(mktemp -d tmp.XXXXXX) || exit

uzip_test_cleanup()
{
	umount $mntpoint
	rmdir $mntpoint
	geom_test_cleanup
}
trap uzip_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh
