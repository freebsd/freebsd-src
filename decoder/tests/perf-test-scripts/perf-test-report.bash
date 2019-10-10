#!/bin/bash
#
# Script to run perf report 
#
# Uses environment set up by perf-setup-env.bash.
# See HOWTO.md for further details.
# 
# run from directory containing perf.data file.
#
#

${PERF_EXEC_PATH}/perf report --stdio $*


