#!/bin/sh
#
# $FreeBSD$
#

set -e

ldns=$(dirname $(realpath $0))
cd $ldns

libtoolize --copy
autoheader
autoconf
./configure --prefix= --exec-prefix=/usr --disable-dane-ta-usage

cd $ldns/drill
autoheader
autoconf
./configure --prefix= --exec-prefix=/usr
