#!/bin/sh

echo "Running tests.  For details see pngtest-log.txt"

echo "============ pngtest pngtest.png ==============" > pngtest-log.txt

echo "Running test-pngtest.sh"
if ./pngtest --strict ${srcdir}/pngtest.png >> pngtest-log.txt 2>&1
then
  echo "  PASS: pngtest --strict pngtest.png"
  err=0
else
  echo "  FAIL: pngtest --strict pngtest.png"
  err=1
fi
exit $err
