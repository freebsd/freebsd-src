# $FreeBSD$

# Go into the regression test directory, handed to us by make(1)
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

STATUS=0

# Note that currently the uuencode(1) program provides no facility to
# include the file mode explicitly based on an argument passed to it,
# so the regress.in file must be mode 644, or the test will say that,
# incorrectly, regression has occurred based on the header.

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
    STATUS=$?
    echo "Test $test failed: regression detected.  See above."
  fi
done

exit $STATUS
