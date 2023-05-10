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

# Sort tests by increasing run time.

log=/tmp/stress2.d/elapsed
lst=/tmp/elapsed.lst
lst2=/tmp/elapsed.order
tim=/tmp/elapsed.tim
[ ! -f $log ] && { echo "$0: No such file: $log" 1>&2; exit 1; }
[ ! -s $log ] && { echo "$0: Empty file: $log" 1>&2; exit 1; }
[ `wc -l < $log` -lt 100 ] && exit 1

export LANG=C
cd ../misc
ls *.sh | grep -E -v "all.sh|cleanup.sh" > $lst
rm -f $lst2
cat $lst | while read l; do
	[ -f ./$l ] || continue
	grep -w $l $log | sed 's/.* //' > $tim
	if [ `wc -l < $tim` -gt 2 ]; then
		# Get the average value
		maxtime=`ministat -n < $tim | tail -1 | awk '{print int($6 + 0.5)}'`
	else
		maxtime=`sed 's/.* //' < $tim | sort -n | tail -1`
	fi
	[ -z "$maxtime" ] && maxtime=999
	echo "$maxtime $l" >> $lst2
done
sort -n < $lst2 | awk '{print $2}' > $lst
cat $lst
rm -f $lst2 $tim
exit 0
