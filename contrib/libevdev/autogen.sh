#!/bin/sh

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`

cd "$srcdir"
autoreconf -fvi || exit $?

cd "$olddir"
test -n "$NOCONFIGURE" || $srcdir/configure "$@"
