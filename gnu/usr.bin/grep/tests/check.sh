#! /bin/sh
# Regression test for GNU grep.
# Usage: regress.sh [testdir]

testdir=${1-tests}

failures=0

# The Khadafy test is brought to you by Scott Anderson . . .
./grep -E -f $testdir/khadafy.regexp $testdir/khadafy.lines > khadafy.out
if cmp $testdir/khadafy.lines khadafy.out
then
	:
else
	echo Khadafy test failed -- output left on khadafy.out
	failures=1
fi

# . . . and the following by Henry Spencer.

${AWK-awk} -F: -f $testdir/scriptgen.awk $testdir/spencer.tests > tmp.script

sh tmp.script && exit $failures
exit 1
