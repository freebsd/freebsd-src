#!/bin/sh
#
# $Id: autogen.sh 814 2014-09-12 07:46:46Z des $
#

libtoolize --copy --force
aclocal -I m4
autoheader
automake --add-missing --copy --foreign
autoconf
