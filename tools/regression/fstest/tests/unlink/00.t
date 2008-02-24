#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/00.t,v 1.1 2007/01/17 01:42:12 pjd Exp $

desc="unlink removes regular files, symbolic links, fifos and sockets"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..55"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

expect 0 create ${n0} 0644
expect regular lstat ${n0} type
expect 0 unlink ${n0}
expect ENOENT lstat ${n0} type

expect 0 symlink ${n1} ${n0}
expect symlink lstat ${n0} type
expect 0 unlink ${n0}
expect ENOENT lstat ${n0} type

expect 0 mkfifo ${n0} 0644
expect fifo lstat ${n0} type
expect 0 unlink ${n0}
expect ENOENT lstat ${n0} type

# TODO: sockets removal

# successful unlink(2) updates ctime.
expect 0 create ${n0} 0644
expect 0 link ${n0} ${n1}
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 unlink ${n1}
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 unlink ${n0}

expect 0 mkfifo ${n0} 0644
expect 0 link ${n0} ${n1}
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 unlink ${n1}
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -lt $ctime2
expect 0 unlink ${n0}

# unsuccessful unlink(2) does not update ctime.
expect 0 create ${n0} 0644
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect EACCES -u 65534 unlink ${n0}
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -eq $ctime2
expect 0 unlink ${n0}

expect 0 mkfifo ${n0} 0644
ctime1=`${fstest} stat ${n0} ctime`
sleep 1
expect EACCES -u 65534 unlink ${n0}
ctime2=`${fstest} stat ${n0} ctime`
test_check $ctime1 -eq $ctime2
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
expect 0 create ${n0}/${n1} 0644
time=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 unlink ${n0}/${n1}
mtime=`${fstest} stat ${n0} mtime`
test_check $time -lt $mtime
ctime=`${fstest} stat ${n0} ctime`
test_check $time -lt $ctime
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 mkfifo ${n0}/${n1} 0644
time=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 unlink ${n0}/${n1}
mtime=`${fstest} stat ${n0} mtime`
test_check $time -lt $mtime
ctime=`${fstest} stat ${n0} ctime`
test_check $time -lt $ctime
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 symlink test ${n0}/${n1}
time=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 unlink ${n0}/${n1}
mtime=`${fstest} stat ${n0} mtime`
test_check $time -lt $mtime
ctime=`${fstest} stat ${n0} ctime`
test_check $time -lt $ctime
expect 0 rmdir ${n0}

expect 0 create ${n0} 0644
expect 0 link ${n0} ${n1}
time=`${fstest} stat ${n0} ctime`
sleep 1
expect 0 unlink ${n1}
ctime=`${fstest} stat ${n0} ctime`
test_check $time -lt $ctime
expect 0 unlink ${n0}

cd ${cdir}
expect 0 rmdir ${n2}
