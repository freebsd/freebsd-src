#!/bin/sh
#
# $Id: autogen.sh 709 2013-08-18 14:47:20Z des $
#

aclocal -I m4
libtoolize --copy --force
autoheader
automake -a -c --foreign
autoconf
