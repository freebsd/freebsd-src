#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chown/08.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chown returns EPERM if the named file has its immutable or append-only flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..34"

n0=`namegen`

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_IMMUTABLE
expect EPERM chown ${n0} 65534 65534
expect 0 chflags ${n0} none
expect 0 chown ${n0} 65534 65534
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_IMMUTABLE
expect EPERM chown ${n0} 65534 65534
expect 0 chflags ${n0} none
expect 0 chown ${n0} 65534 65534
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_APPEND
expect EPERM chown ${n0} 65534 65534
expect 0 chflags ${n0} none
expect 0 chown ${n0} 65534 65534
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_APPEND
expect EPERM chown ${n0} 65534 65534
expect 0 chflags ${n0} none
expect 0 chown ${n0} 65534 65534
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_NOUNLINK
expect 0 chown ${n0} 65534 65534
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_NOUNLINK
expect 0 chown ${n0} 65534 65534
expect 0 chflags ${n0} none
expect 0 unlink ${n0}
