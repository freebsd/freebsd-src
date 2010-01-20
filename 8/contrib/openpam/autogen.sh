#!/bin/sh
#
# $Id: autogen.sh 396 2007-10-24 09:58:18Z des $
#

aclocal
libtoolize --copy --force
autoheader
automake -a -c --foreign
autoconf
