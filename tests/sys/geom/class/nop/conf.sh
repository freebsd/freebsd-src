#!/bin/sh
# $FreeBSD$

class="nop"
base=`basename $0`

gnop_test_cleanup()
{
	[ -c /dev/${us}.nop ] && gnop destroy ${us}.nop
	geom_test_cleanup
}
trap gnop_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh
