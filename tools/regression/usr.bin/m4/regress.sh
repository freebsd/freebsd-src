# $FreeBSD$

REGRESSION_START($1)

for test in GNU/changecom changecom; do
  echo "Running test $test"
  case "$test" in
  GNU/*)
    M4="m4 -g"
    GNU="g"
    test=`basename $test`
    ;;
  *)
    M4="m4"
    GNU=""
    ;;
  esac
  case "$test" in
  changecom)
    $M4 < regress.$test.in | diff -u regress.$GNU$test.out -
    ;;
  esac
  if [ $? -eq 0 ]; then
    echo "PASS: Test $test detected no regression, output matches."
  else
    STATUS=$?
    echo "FAIL: Test $test failed: regression detected.  See above."
  fi
done

REGRESSION_END()
