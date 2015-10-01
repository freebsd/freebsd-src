#!/bin/sh
#
# $FreeBSD$
#

set -e

error() {
	echo "$@" >&2
	exit 1
}

unbound=$(dirname $(realpath $0))
cd $unbound

ldnssrc=$(realpath $unbound/../ldns)
[ -f $ldnssrc/ldns/ldns.h ] || error "can't find LDNS sources"
export CFLAGS="-I$ldnssrc"

ldnsbld=$(realpath $unbound/../../lib/libldns)
[ -f $ldnsbld/Makefile ] || error "can't find LDNS build directory"

ldnsobj=$(realpath $(make -C$ldnsbld -V.OBJDIR))
[ -f $ldnsobj/libprivateldns.a ] || error "can't find LDNS object directory"
export LDFLAGS="-L$ldnsobj"

autoconf
autoheader
./configure \
	--prefix= --exec-prefix=/usr \
	--with-conf-file=/var/unbound/unbound.conf \
	--with-run-dir=/var/unbound \
	--with-username=unbound

# Don't try to provide bogus memory usage statistics based on sbrk(2).
sed -n -i.orig -e '/HAVE_SBRK/!p' config.status
./config.status config.h
