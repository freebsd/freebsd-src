#!/bin/sh
# $FreeBSD$

desc="chmod returns EPERM if the named file has its immutable or append-only flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..40"

n0=`namegen`

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_IMMUTABLE
expect EPERM chmod ${n0} 0600
expect 0644 stat ${n0} mode
expect 0 chflags ${n0} none
expect 0 chmod ${n0} 0600
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_IMMUTABLE
expect EPERM chmod ${n0} 0600
expect 0644 stat ${n0} mode
expect 0 chflags ${n0} none
expect 0 chmod ${n0} 0600
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_APPEND
expect EPERM chmod ${n0} 0600
expect 0644 stat ${n0} mode
expect 0 chflags ${n0} none
expect 0 chmod ${n0} 0600
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_APPEND
expect EPERM chmod ${n0} 0600
expect 0644 stat ${n0} mode
expect 0 chflags ${n0} none
expect 0 chmod ${n0} 0600
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_NOUNLINK
expect 0 chmod ${n0} 0600
expect 0600 stat ${n0} mode
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_NOUNLINK
expect 0 chmod ${n0} 0600
expect 0600 stat ${n0} mode
expect 0 chflags ${n0} none
expect 0 unlink ${n0}
