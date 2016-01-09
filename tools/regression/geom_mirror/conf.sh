#!/bin/sh
# $FreeBSD$

name="$(mktemp -u mirror.XXXXXX)"
class="mirror"
base=`basename $0`

gmirror_test_cleanup()
{
	[ -c /dev/$class/$name ] && gmirror destroy $name
	geom_test_cleanup
}
trap gmirror_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh
