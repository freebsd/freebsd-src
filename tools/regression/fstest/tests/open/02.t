#!/bin/sh
# $FreeBSD$

desc="open returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

expect 0 open ${name255} O_CREAT 0620
expect 0620 stat ${name255} mode
expect 0 unlink ${name255}
expect ENAMETOOLONG open ${name256} O_CREAT 0620
