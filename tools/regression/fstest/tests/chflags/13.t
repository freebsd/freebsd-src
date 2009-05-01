#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chflags/13.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

desc="chflags returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..2"

expect EFAULT chflags NULL UF_IMMUTABLE
expect EFAULT chflags DEADCODE UF_IMMUTABLE
