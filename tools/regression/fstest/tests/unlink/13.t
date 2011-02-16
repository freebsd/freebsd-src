#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/13.t,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $

desc="unlink returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT unlink NULL
expect EFAULT unlink DEADCODE
