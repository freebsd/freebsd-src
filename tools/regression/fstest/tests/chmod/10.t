#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chmod/10.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

desc="chmod returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT chmod NULL 0644
expect EFAULT chmod DEADCODE 0644
