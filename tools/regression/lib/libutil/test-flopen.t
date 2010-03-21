#!/bin/sh
#
# $FreeBSD: src/tools/regression/lib/libutil/test-flopen.t,v 1.1.12.1 2010/02/10 00:26:20 kensmith Exp $
#

base=$(realpath $(dirname $0))
name=$(basename $0 .t)

set -e
cd $base
make -s $name >/dev/null
exec $base/$name
