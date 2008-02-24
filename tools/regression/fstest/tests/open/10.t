#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/10.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="open returns EPERM when the named file has its immutable flag set and the file is to be modified"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..28"

n0=`namegen`

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_IMMUTABLE
expect EPERM open ${n0} O_WRONLY
expect EPERM open ${n0} O_RDWR
expect EPERM open ${n0} O_RDONLY,O_TRUNC
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_IMMUTABLE
expect EPERM open ${n0} O_WRONLY
expect EPERM open ${n0} O_RDWR
expect EPERM open ${n0} O_RDONLY,O_TRUNC
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} SF_NOUNLINK
expect 0 open ${n0} O_WRONLY
expect 0 open ${n0} O_RDWR
expect 0 open ${n0} O_RDONLY,O_TRUNC
expect 0 chflags ${n0} none
expect 0 unlink ${n0}

expect 0 create ${n0} 0644
expect 0 chflags ${n0} UF_NOUNLINK
expect 0 open ${n0} O_WRONLY
expect 0 open ${n0} O_RDWR
expect 0 open ${n0} O_RDONLY,O_TRUNC
expect 0 chflags ${n0} none
expect 0 unlink ${n0}
