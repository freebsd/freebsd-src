#!/bin/bash
#
# Script to run perf report 
#
# Uses environment set up by perf-setup-env.bash.
# See HOWTO.md for further details.
#
# run from directory containing perf.data file.
#

${PERF_EXEC_PATH}/perf --exec-path=${PERF_EXEC_PATH} script --script=python:${PERF_SCRIPT_PATH}/cs-trace-disasm.py -- -d ${XTOOLS_PATH}/aarch64-linux-gnu-objdump $*
