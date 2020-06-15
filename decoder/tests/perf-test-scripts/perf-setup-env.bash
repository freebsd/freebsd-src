#!/bin/bash
# Script to set up the environment for testing perf with OpenCSD
#
# See HOWTO.md for details on how these environment variables should be set and used.
#
# to use this script:-
#
# 1) for perf exec env only
# source perf-setup-env.bash
#
# 2) for perf build and exec env
# source perf-setup-env.bash buildenv
#

#------ User Edits Start -------
# Edit as required for user system.

# Root of the opencsd library project as cloned from github
export OPENCSD_ROOT=~/work/opencsd-master

# the opencsd build library directory to use.
export OCSD_LIB_DIR=lib/builddir

# the root of the perf branch / perf dev-tree as checked out
export PERF_ROOT=~/work/kernel-dev

# the arm x-compiler toolchain path
export XTOOLS_PATH=~/work/gcc-x-aarch64-6.2/bin

#------ User Edits End -------

# path to source/include root dir - used by perf build to 
# include Opencsd decoder.

if [ "$1" == "buildenv" ]; then
   export CSTRACE_PATH=${OPENCSD_ROOT}/decoder
   export CSLIBS=${CSTRACE_PATH}/${OCSD_LIB_DIR}
   export CSINCLUDES=${CSTRACE_PATH}/include

   # add library to lib path
   if [ "${LD_LIBRARY_PATH}" == ""  ]; then 
       export LD_LIBRARY_PATH=${CSLIBS}
   else
       export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${CSLIBS}
   fi
fi

# perf script defines
export PERF_EXEC_PATH=${PERF_ROOT}/tools/perf
export PERF_SCRIPT_PATH=${PERF_EXEC_PATH}/scripts/python

