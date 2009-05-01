#!/bin/sh
#
# $FreeBSD: src/tools/regression/lib/libutil/test-flopen.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $
#

base=$(realpath $(dirname $0))
name=$(basename $0 .t)

set -e
cd $base
make -s $name >/dev/null
exec $base/$name
