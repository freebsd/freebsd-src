#! /bin/sh

. $(dirname $0)/../../common.sh

# Description
DESC="Archive parsing (modified BSD4.4 format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
