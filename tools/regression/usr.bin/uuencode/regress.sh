# $FreeBSD$

# Go into the regression test directory, handed to us by make(1)
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

for test in traditional base64; do
  echo "Running test $test"
  case "$test" in
  traditional)
    uuencode regress.in regress.in | diff -u regress.$test.out -
    ;;
  base64)
    uuencode -m regress.in regress.in | diff -u regress.$test.out -
    ;;
  esac
  if [ $? -eq 0 ]; then
    echo "Test $test detected no regression, output matches."
  else
    echo "Test $test failed: regression detected.  See above."
    exit 1
  fi
done
