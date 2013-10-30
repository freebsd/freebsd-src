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
[ -f $ldnsobj/libldns.a ] || error "can't find LDNS object directory"
export LDFLAGS="-L$ldnsobj"

autoconf
autoheader
./configure \
	--prefix= --exec-prefix=/usr \
	--with-conf-file=/var/unbound/unbound.conf \
	--with-run-dir=/var/unbound \
	--with-username=unbound

# Regenerate the configuration parser
{
cat <<EOF
#include "config.h"
#include "util/configyyrename.h"
EOF
/usr/bin/flex -L -t util/configlexer.lex
} >util/configlexer.c

/usr/bin/yacc -d -o util/configparser.c util/configparser.y
