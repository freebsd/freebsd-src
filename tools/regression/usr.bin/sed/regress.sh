# $FreeBSD$

# Go into the regression test directory, handed to us by make(1)
TESTDIR=$1
if [ -z "$TESTDIR" ]; then
  TESTDIR=.
fi
cd $TESTDIR

STATUS=0

for test in G P psl; do
  echo "Running test $test"
  case "$test" in
  G)
    sed G < regress.in | diff -u regress.$test.out -
    ;;
  P)
    sed P < regress.in | diff -u regress.$test.out -
    ;;
  psl)	
    sed '$!g; P; D' < regress.in | diff -u regress.$test.out -
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
