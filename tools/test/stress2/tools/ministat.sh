#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# Report excessive runtime using /tmp/stress2.d/elapsed

[ $# -lt 2 ] && { echo "Usage: $0 <logfile> <test> [debug]"; exit 1; }
log=$1
[ -f $log ] || { echo "No such $log"; exit 1; }

export LANG=C
data=/tmp/ministat.log
trap "rm -f $data" EXIT INT
test=`basename $2`
grep " $test" $log | awk '$NF > 2 {print $NF}' | sed '$d' > $data
[ `wc -l < $data` -lt 5 ] && exit 0

max=`ministat -n < $data | tail -1 | \
    awk '{print int($6 + $7 * 2 + 0.5)}'` # avg + 2 * stddev
run=`grep $test $log | awk '{print $NF}' | tail -1`
[ $run -lt 3 ] && exit 0
if [ $run -gt $max -o $# -eq 3 ]; then
	echo "Note: $test's runtime was $run seconds."
	ministat -n < $data | tail -2
	exit 1
fi
exit 0
