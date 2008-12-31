#!/bin/sh
#
# $FreeBSD: src/tools/regression/lib/libutil/test-flopen.t,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
#

base=$(realpath $(dirname $0))
name=$(basename $0 .t)

set -e
cd $base
make -s $name >/dev/null
exec $base/$name
