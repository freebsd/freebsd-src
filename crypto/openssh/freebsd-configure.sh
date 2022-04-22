#!/bin/sh
#

configure_args="
    --prefix=/usr
    --sysconfdir=/etc/ssh
    --with-pam
    --with-ssl-dir=/usr
    --without-tcp-wrappers
    --with-libedit
    --with-ssl-engine
    --without-xauth
"

set -e

openssh=$(dirname $(realpath $0))
cd $openssh

# Run autotools before we drop LOCALBASE out of PATH
(cd $openssh && libtoolize --copy && autoheader && autoconf)

# Ensure we use the correct toolchain and clean our environment
export CC=$(echo ".include <bsd.lib.mk>" | make -f /dev/stdin -VCC)
export CPP=$(echo ".include <bsd.lib.mk>" | make -f /dev/stdin -VCPP)
unset CFLAGS CPPFLAGS LDFLAGS LD_LIBRARY_PATH LIBS
export PATH=/bin:/sbin:/usr/bin:/usr/sbin

# Generate config.h with krb5 and stash it
sh configure $configure_args --with-kerberos5=/usr
mv config.log config.log.kerberos5
mv config.h config.h.kerberos5

# Generate config.h with built-in security key support
#
# We install libcbor and libfido2 as PRIVATELIB, so the headers are not
# available for configure - add their paths via CFLAGS as a slight hack.
# configure.ac is also patched to specify -lprivatecbor and -lprivatefido2
# rather than -lcbor and -lfido2.
export CFLAGS="-I$openssh/../../contrib/libcbor/src -I$openssh/../../contrib/libfido2/src"
sh configure $configure_args --with-security-key-builtin
unset CFLAGS
mv config.log config.log.sk-builtin
mv config.h config.h.sk-builtin

# Generate config.h without krb5 or SK support
sh configure $configure_args --without-kerberos5 --without-security-key-builtin

# Extract the difference
diff -u config.h.kerberos5 config.h |
	sed -n '/^-#define/s/^-//p' |
	grep -Ff /dev/stdin config.h.kerberos5 > krb5_config.h

# Extract the difference - SK
diff -u config.h.sk-builtin config.h |
    sed -n '/^-#define/s/^-//p' |
    grep -Ff /dev/stdin config.h.sk-builtin > sk_config.h
