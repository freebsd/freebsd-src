#! /bin/sh

set -x
aclocal -I config
libtoolize --force --copy
autoheader
automake --foreign --add-missing --copy
autoconf
