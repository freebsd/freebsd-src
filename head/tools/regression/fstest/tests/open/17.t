#!/bin/sh
# $FreeBSD$

desc="open returns ENXIO when O_NONBLOCK is set, the named file is a fifo, O_WRONLY is set, and no process has the file open for reading"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

n0=`namegen`

expect 0 mkfifo ${n0} 0644
expect ENXIO open ${n0} O_WRONLY,O_NONBLOCK
expect 0 unlink ${n0}
