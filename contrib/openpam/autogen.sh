#!/bin/sh
#
# $Id: autogen.sh 815 2014-09-12 07:47:27Z des $
#

aclocal -I m4
libtoolize --copy --force
autoheader
automake --add-missing --copy --foreign
autoconf
