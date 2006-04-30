#!/bin/sh -ex
#
# $P4: //depot/projects/openpam/autogen.sh#2 $
#

libtoolize --copy --force
aclocal
autoheader
automake -a -c --foreign
autoconf
