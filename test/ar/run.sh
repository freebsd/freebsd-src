#!/bin/sh
# $Id: run.sh 2092 2011-10-28 08:23:57Z jkoshy $
#
# Run all the tests.

test_log=test.log

# setup cleanup trap
trap 'rm -rf /tmp/bsdar-*; exit' 0 2 3 15

# load functions.
. ./func.sh

# global initialization.
init

exec >${test_log} 2>&1
echo @TEST-RUN: `date`

# run tests.
for f in tc/*; do
    if [ -d $f ]; then
	. $f/`basename $f`.sh
    fi
done

# show statistics.
echo @RESULT: `statistic`
