#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rmdir/09.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="rmdir returns EPERM if the named file has its immutable, undeletable or append-only flag set"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..30"

n0=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 chflags ${n0} SF_IMMUTABLE
expect EPERM rmdir ${n0}
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 chflags ${n0} UF_IMMUTABLE
expect EPERM rmdir ${n0}
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 chflags ${n0} SF_NOUNLINK
expect EPERM rmdir ${n0}
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 chflags ${n0} UF_NOUNLINK
expect EPERM rmdir ${n0}
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 chflags ${n0} SF_APPEND
expect EPERM rmdir ${n0}
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 chflags ${n0} UF_APPEND
expect EPERM rmdir ${n0}
expect 0 chflags ${n0} none
expect 0 rmdir ${n0}
