#!/bin/sh -
#
# Copyright (c) 1990 The Regents of the University of California.
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
#	@(#)lorder.sh	5.3 (Berkeley) 4/12/91
#

PATH=/bin:/usr/bin
export PATH

# only one argument is a special case, just output the name twice
case $# in
	0)
		echo "usage: lorder file ...";
		exit ;;
	1)
		echo $1 $1;
		exit ;;
esac

# temporary files
R=/tmp/_reference_$$
S=/tmp/_symbol_$$

# remove temporary files on HUP, INT, QUIT, PIPE, TERM
trap "rm -f $R $S; exit 1" 1 2 3 13 15

# if the line ends in a colon, assume it's the first occurrence of a new
# object file.  Echo it twice, just to make sure it gets into the output.
#
# if the line has " T " or " D " it's a globally defined symbol, put it
# into the symbol file.
#
# if the line has " U " it's a globally undefined symbol, put it into
# the reference file.
nm -go $* | sed "
	/:$/ {
		s/://
		s/.*/& &/
		p
		d
	}
	/ [TD] / {
		s/:.* [TD]//
		w $S
		d
	}
	/ U / {
		s/:.* U//
		w $R
	}
	d
"

# sort symbols and references on the first field (the symbol)
# join on that field, and print out the file names.
sort +1 $R -o $R
sort +1 $S -o $S
join -j 2 -o 1.1 2.1 $R $S
rm -f $R $S
