# $FreeBSD$

# Go into the regression test directory, handed to us by make(1)
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

for test in traditional base64; do
  echo "Running test $test"
  uudecode -p < regress.$test.in | cmp regress.out -
  if [ $? -eq 0 ]; then
    echo "Test $test detected no regression, output matches."
  else
    echo "Test $test failed: regression detected.  See above."
    exit 1
  fi
done
