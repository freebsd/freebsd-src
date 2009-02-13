#!/bin/sh
# $FreeBSD$

desc="link returns EPERM if the source file has its immutable or append-only flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..32"

n0=`namegen`
n1=`namegen`

expect 0 create ${n0} 0644

expect 0 link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 chflags ${n0} SF_IMMUTABLE
expect EPERM link ${n0} ${n1}
expect 0 chflags ${n0} none
expect 0 link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 chflags ${n0} UF_IMMUTABLE
expect EPERM link ${n0} ${n1}
expect 0 chflags ${n0} none
expect 0 link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 chflags ${n0} SF_APPEND
expect EPERM link ${n0} ${n1}
expect 0 chflags ${n0} none
expect 0 link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 chflags ${n0} UF_APPEND
expect EPERM link ${n0} ${n1}
expect 0 chflags ${n0} none
expect 0 link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 chflags ${n0} SF_NOUNLINK
expect 0 link ${n0} ${n1}
expect 0 chflags ${n0} none
expect 0 unlink ${n1}

expect 0 chflags ${n0} UF_NOUNLINK
expect 0 link ${n0} ${n1}
expect 0 chflags ${n0} none
expect 0 unlink ${n1}

expect 0 unlink ${n0}
