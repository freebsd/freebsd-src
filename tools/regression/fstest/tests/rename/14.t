#!/bin/sh
# $FreeBSD$

desc="rename returns EISDIR when the 'to' argument is a directory, but 'from' is not a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..17"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755

expect 0 create ${n1} 0644
expect EISDIR rename ${n1} ${n0}
expect dir lstat ${n0} type
expect regular lstat ${n1} type
expect 0 unlink ${n1}

expect 0 mkfifo ${n1} 0644
expect EISDIR rename ${n1} ${n0}
expect dir lstat ${n0} type
expect fifo lstat ${n1} type
expect 0 unlink ${n1}

expect 0 symlink test ${n1}
expect EISDIR rename ${n1} ${n0}
expect dir lstat ${n0} type
expect symlink lstat ${n1} type
expect 0 unlink ${n1}

expect 0 rmdir ${n0}
