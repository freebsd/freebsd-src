#!/bin/sh
unset LC_ALL LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC LC_TIME
export LANG=C
export BC_TEST_OUTPUT_DIR=${PWD}
exec "$(dirname "$(realpath "$0")")"/tests/all.sh -n dc 1 1 0 0 dc
