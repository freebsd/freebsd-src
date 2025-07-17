#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Watch disk resources for a test program

unset LOAD
unset runLOAD
unset swapLOAD
unset rwLOAD
unset mkdirLOAD
unset creatLOAD
unset symlinkLOAD

[ ! -d $RUNDIR ] && mkdir $RUNDIR
imax=0
kmax=0
istart=`df -ik $RUNDIR | tail -1 | awk '{print $6}'`
kstart=`df -ik $RUNDIR | tail -1 | awk '{print $3}'`

"$@" &

while ps -p $! > /dev/null; do
   i=`df -ik $RUNDIR | tail -1 | awk '{print $6}'`
   k=`df -ik $RUNDIR | tail -1 | awk '{print $3}'`
   if [ $i -gt $imax -o $k -gt $kmax ]; then
      imax=$i
      kmax=$k
   fi
   sleep .2
done
printf "Disk usage: %d inodes and %dk\n" $((imax - istart)) $((kmax - kstart))
wait
