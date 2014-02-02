#!/bin/sh
#
# $FreeBSD$
#

configure_args="
    --prefix=/usr
    --sysconfdir=/etc/ssh
    --with-pam
    --with-tcp-wrappers
    --with-libedit
    --with-ssl-engine
    --without-xauth
" 

set -e

# generate config.h with krb5 and stash it
sh configure $configure_args --with-kerberos5
mv config.log config.log.orig
mv config.h config.h.orig

# generate config.h without krb5
sh configure $configure_args --without-kerberos5

# extract the difference
echo '/* $Free''BSD$ */' > krb5_config.h
diff -u config.h.orig config.h |
	sed -n '/^-#define/s/^-//p' |
	grep -Ff /dev/stdin config.h.orig >> krb5_config.h
