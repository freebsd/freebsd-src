#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chown/10.t,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

desc="chown returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT chown NULL 65534 65534
expect EFAULT chown DEADCODE 65534 65534
