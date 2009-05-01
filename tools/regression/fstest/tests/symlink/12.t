#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/symlink/12.t,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

desc="symlink returns EFAULT if one of the pathnames specified is outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`

expect EFAULT symlink NULL ${n0}
expect EFAULT symlink DEADCODE ${n0}
expect EFAULT symlink test NULL
expect EFAULT symlink test DEADCODE
expect EFAULT symlink NULL DEADCODE
expect EFAULT symlink DEADCODE NULL
