#!/bin/sh
# $FreeBSD$

desc="mknod returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

expect 0 mknod ${name255} f 0644 0 0
expect 0 unlink ${name255}
expect ENAMETOOLONG mknod ${name256} f 0644 0 0
