#!/bin/sh
# $FreeBSD$

set -e

cd $(dirname $0)

TMPDIR=$(mktemp -d /tmp/tmp.XXXXXXXX)
TEST_DIR="$TMPDIR/test"
trap "cd /; rm -Rf $TMPDIR" EXIT INT TERM

# Setup the environment for run_test.sh
mkdir -p "$TEST_DIR"
cp -Rf * "$TEST_DIR/."
echo > "$TMPDIR/config.h"
ln /usr/bin/yacc $TMPDIR/yacc

log=$TMPDIR/run_test.log
(cd $TEST_DIR && ./run_test 2>&1 && : > run_test.ok) | tee $log
if [ -f run_test.ok ] && ! egrep "^...(diff|not found)[^\*]+$" $log; then
	exit 0
else
	exit 1
fi
