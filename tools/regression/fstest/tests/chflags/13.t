#!/bin/sh
# $FreeBSD$

desc="chflags returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..2"

expect EFAULT chflags NULL UF_NODUMP
expect EFAULT chflags DEADCODE UF_NODUMP
