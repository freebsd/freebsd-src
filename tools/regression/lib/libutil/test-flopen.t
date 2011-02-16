#!/bin/sh
#
# $FreeBSD: src/tools/regression/lib/libutil/test-flopen.t,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $
#

base=$(realpath $(dirname $0))
name=$(basename $0 .t)

set -e
cd $base
make -s $name >/dev/null
exec $base/$name
