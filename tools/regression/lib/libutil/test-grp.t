#!/bin/sh
#
# $FreeBSD: src/tools/regression/lib/libutil/test-grp.t,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

base=$(realpath $(dirname $0))
name=$(basename $0 .t)

set -e
cd $base
make -s $name >/dev/null
exec $base/$name
