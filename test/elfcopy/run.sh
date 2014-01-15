#!/bin/sh
#
# $Id: run.sh 2091 2011-10-28 08:15:16Z jkoshy $
#
# Run all the tests.

test_log=test.log

# setup cleanup trap
trap 'rm -rf /tmp/elfcopy-*; rm -rf /tmp/strip-*; exit' 0 2 3 15

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
