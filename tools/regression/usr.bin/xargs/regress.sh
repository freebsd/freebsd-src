# $FreeBSD$

# Go into the regression test directory, handed to us by make(1)
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

for test in normal I J L; do
  echo "Running test $test"
  case "$test" in
  normal)
    xargs echo The < regress.in | diff -u regress.$test.out -
    ;;
  I)
    xargs -I% echo The % % % %% % % < regress.in | diff -u regress.$test.out -
    ;;
  J)
    xargs -J% echo The % again. < regress.in | diff -u regress.$test.out -
    ;;
  L)
    xargs -L3 echo < regress.in | diff -u regress.$test.out -
    ;;
  esac
  if [ $? -eq 0 ]; then
    echo "Test $test detected no regression, output matches."
  else
    echo "Test $test failed: regression detected.  See above."
    exit 1
  fi
done
