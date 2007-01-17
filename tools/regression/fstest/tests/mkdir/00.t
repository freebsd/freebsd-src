#!/bin/sh
# $FreeBSD$

desc="mkdir creates directories"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..36"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n1} 0755
cdir=`pwd`
cd ${n1}

expect 0 mkdir ${n0} 0755
expect dir,0755 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 mkdir ${n0} 0151
expect dir,0151 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 -U 077 mkdir ${n0} 0151
expect dir,0100 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 -U 070 mkdir ${n0} 0345
expect dir,0305 lstat ${n0} type,mode
expect 0 rmdir ${n0}
expect 0 -U 0501 mkdir ${n0} 0345
expect dir,0244 lstat ${n0} type,mode
expect 0 rmdir ${n0}

expect 0 chown . 65535 65535
expect 0 -u 65535 -g 65535 mkdir ${n0} 0755
expect 65535,65535 lstat ${n0} uid,gid
expect 0 rmdir ${n0}
expect 0 -u 65535 -g 65534 mkdir ${n0} 0755
expect 65535,65535 lstat ${n0} uid,gid
expect 0 rmdir ${n0}
expect 0 chmod . 0777
expect 0 -u 65534 -g 65533 mkdir ${n0} 0755
expect 65534,65535 lstat ${n0} uid,gid
expect 0 rmdir ${n0}

expect 0 chown . 0 0
time=`${fstest} stat . ctime`
sleep 1
expect 0 mkdir ${n0} 0755
atime=`${fstest} stat ${n0} atime`
test_check $time -lt $atime
mtime=`${fstest} stat ${n0} mtime`
test_check $time -lt $mtime
ctime=`${fstest} stat ${n0} ctime`
test_check $time -lt $ctime
mtime=`${fstest} stat . mtime`
test_check $time -lt $mtime
ctime=`${fstest} stat . ctime`
test_check $time -lt $ctime
expect 0 rmdir ${n0}

cd ${cdir}
expect 0 rmdir ${n1}
