#!/bin/bash
# Script to set up the environment for testing perf with OpenCSD
#
# See HOWTO.md for details on how these environment variables should be set and used.
#
# to use this script:-
#
# source perf-setup-env.bash
#

#------ User Edits Start -------
# Edit as required for user system.

# Root of the opencsd library project as cloned from github
export OPENCSD_ROOT=~/OpenCSD/opencsd-github/opencsd

# the opencsd build library directory to use.
export OCSD_LIB_DIR=lib/linux64/rel

# the root of the perf branch / perf dev-tree as checked out
export PERF_ROOT=~/work2/perf-opencsd/mp-4.7-rc4/coresight

# the arm x-compiler toolchain path
export XTOOLS_PATH=~/work2/toolchain-aarch64/gcc-linaro-4.9-2015.05-1-rc1-x86_64_aarch64-linux-gnu/bin/

#------ User Edits End -------

# path to source/include root dir - used by perf build to 
# include Opencsd decoder.
export CSTRACE_PATH=${OPENCSD_ROOT}/decoder

# add library to lib path
if [ "${LD_LIBRARY_PATH}" == ""  ]; then 
    export LD_LIBRARY_PATH=${CSTRACE_PATH}/${OCSD_LIB_DIR}
else
    export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${CSTRACE_PATH}/${OCSD_LIB_DIR}
fi

# perf script defines
export PERF_EXEC_PATH=${PERF_ROOT}/tools/perf
export PERF_SCRIPT_PATH=${PERF_EXEC_PATH}/scripts/python

