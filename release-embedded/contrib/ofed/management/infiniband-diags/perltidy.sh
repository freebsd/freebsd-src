#!/bin/bash
#
# Copyright (c) 2006 The Regents of the University of California.
#
# Produced at Lawrence Livermore National Laboratory.
# Written by Ira Weiny <weiny2@llnl.gov>.
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# OpenIB.org BSD license below:
#
#     Redistribution and use in source and binary forms, with or
#     without modification, are permitted provided that the following
#     conditions are met:
#
#      - Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      - Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

tidy_cmd="perltidy -pt=2 -sbt=2 -bt=2 -nsfs -b -t -nola -ce -sbl -nbbc"

argv0=`basename $0`
scripts_dir=`dirname $0`/scripts

if [ "$1" == "-h" ]; then
   echo "$argv0 [-h]"
   echo "   Run perltidy on all perl scripts and modules in the scripts directory"
   exit 1
fi

cd $scripts_dir

for file in *.pl ; do
   echo "tidy : $scripts_dir/$file"
   $tidy_cmd $file
done

for file in *.pm ; do
   echo "tidy : $scripts_dir/$file"
   $tidy_cmd $file
done

exit 0
