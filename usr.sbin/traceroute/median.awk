#!/bin/awk -f
#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# This code is derived from software contributed to Berkeley by
# Van Jacobson.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)median.awk	5.2 (Berkeley) 4/28/90
#
/^ *[0-9]/	{
	# print out the median time to each hop along a route.
	tottime = 0; n = 0;
	for (f = 5; f <= NF; ++f) {
		if ($f == "ms") {
			++n
			time[n] = $(f - 1)
		}
	}
	if (n > 0) {
		# insertion sort the times to find the median
		for (i = 2; i <= n; ++i) {
			v = time[i]; j = i - 1;
			while (time[j] > v) {
				time[j+1] = time[j];
				j = j - 1;
				if (j < 0)
					break;
			}
			time[j+1] = v;
		}
		if (n > 1 && (n % 2) == 0)
			median = (time[n/2] + time[(n/2) + 1]) / 2
		else
			median = time[(n+1)/2]

		print $1, median
	}
}
