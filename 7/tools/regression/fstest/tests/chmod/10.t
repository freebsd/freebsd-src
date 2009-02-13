#!/bin/sh
# $FreeBSD$

desc="chmod returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT chmod NULL 0644
expect EFAULT chmod DEADCODE 0644
