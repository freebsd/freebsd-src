#! /bin/sh

. $(dirname $0)/../../common.sh

# Description
DESC="Test '+command' execution with -n -jX"

# Run
TEST_N=1
TEST_1=

eval_cmd $*
