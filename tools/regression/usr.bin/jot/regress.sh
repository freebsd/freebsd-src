# $FreeBSD$

# Go into the regression test directory, handed to us by make(1)
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

jot -w '%X' -s ',' 100 1 200 | diff -u regress.out -
if [ $? -eq 0 ]; then
  echo "Test detected no regression, output matches."
else
  echo "Test failed: regression detected.  See above."
  exit 1
fi
