#! /bin/sh
# Regression test for GNU e?grep.
# Usage: regress.sh [dir-containing-egrep]

builddir=${1-..}

failures=0

# The Khadafy test is brought to you by Scott Anderson . . .
$builddir/egrep -f khadafy.regexp khadafy.lines > khadafy.out
if cmp khadafy.lines khadafy.out
then
	rm khadafy.out
else
	echo Khadafy test failed -- output left on khadafy.out
	failures=1
fi

# . . . and the following by Henry Spencer.

awk -F: -f scriptgen.awk spencer.tests > tmp.script

if sh tmp.script $builddir
then
	rm tmp.script
	exit $failures
else
	rm tmp.script
	exit 1
fi
