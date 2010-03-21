#!/bin/sh
# $FreeBSD: src/tools/regression/execve/execve.t,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

cd `dirname $0`
cmd="./`basename $0 .t`"

make >/dev/null 2>&1

tests="test-empty test-nonexist test-nonexistshell \
	test-devnullscript test-badinterplen test-goodscript \
	test-scriptarg test-scriptarg-nospace test-goodaout \
	test-truncaout test-sparseaout"

n=0

echo "1..11"

for atest in ${tests}
do
	n=`expr ${n} + 1`
	if make ${atest}
	then
		echo "ok ${n} - ${atest}"
	else
		echo "not ok ${n} - ${atest}"
	fi
done
