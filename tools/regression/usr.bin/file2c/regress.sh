# $FreeBSD$

# Go into the regression test directory, handed to us by make(1)
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

file2c 'const char data[] = {' ', 0};' < regress.in | diff -u regress.out -
if [ $? -eq 0 ]; then
  echo "PASS: Test detected no regression, output matches."
else
  echo "FAIL: Test failed: regression detected.  See above."
  exit 1
fi
