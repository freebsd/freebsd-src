# $FreeBSD$

define(`REGRESSION_START',
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

STATUS=0)

define(`REGRESSION_TEST',
echo "Running test $1"
$2 | diff -u regress.$1.out -
if [ $? -eq 0 ]; then
  echo "PASS: Test $1 detected no regression."
else
  STATUS=$?
  echo "FAIL: Test $1 failed: regression detected.  See above."
fi)

define(`REGRESSION_END',
exit $STATUS)
