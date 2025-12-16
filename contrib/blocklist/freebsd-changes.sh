#!/bin/sh

#
# FreeBSD-specific changes from upstream
#

# Remove Debian port
rm -fr port/debian

# /libexec -> /usr/libexec
sed -i "" -e 's| /libexec| /usr/libexec|g' bin/blocklistd.8
sed -i "" -e 's|"/libexec|"/usr/libexec|g' bin/internal.h

# NetBSD: RT_ROUNDUP -> FreeBSD: SA_SIZE (from net/route.h)
sed -i "" -e 's/RT_ROUNDUP/SA_SIZE/g' bin/conf.c

# npfctl(8) -> ipf(8), ipfw(8), pfctl(8)
sed -i "" -e 's/npfctl 8 ,/ipf 8 ,\n.Xr ipfw 8 ,\n.Xr pfctl 8 ,/g' bin/blocklistd.8
