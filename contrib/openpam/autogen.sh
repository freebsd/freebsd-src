#!/bin/sh -ex

libtoolize --copy --force
aclocal
autoheader
automake -a -c --foreign
autoconf
