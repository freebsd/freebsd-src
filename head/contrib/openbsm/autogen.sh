#!/bin/sh
#
# $P4: //depot/projects/trustedbsd/openbsm/autogen.sh#1 $
#

libtoolize --copy --force
aclocal
autoheader
automake -a -c --foreign
autoconf
