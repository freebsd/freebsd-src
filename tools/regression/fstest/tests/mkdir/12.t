#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkdir/12.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

desc="mkdir returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT mkdir NULL 0755
expect EFAULT mkdir DEADCODE 0755
