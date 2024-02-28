#!/bin/sh -
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 1990, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
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

export LC_CTYPE=C
export LC_COLLATE=C
set -e

usage() {
	echo "usage: lorder file ..." >&2
	exit 1
}

while getopts "" opt ; do
	case $opt in
	*)
		usage
		;;
	esac
done
shift $(($OPTIND - 1))
if [ $# -eq 0 ] ; then
	usage
fi

#
# Create temporary files.
#
N=$(mktemp -t _nm_)
R=$(mktemp -t _reference_)
S=$(mktemp -t _symbol_)
T=$(mktemp -t _temp_)
NM=${NM:-nm}

#
# Remove temporary files on termination.
#
trap "rm -f $N $R $S $T" EXIT 1 2 3 13 15

#
# A line matching " [RTDW] " indicates that the input defines a symbol
# with external linkage; put it in the symbol file.
#
# A line matching " U " indicates that the input references an
# undefined symbol; put it in the reference file.
#
${NM} ${NMFLAGS} -go "$@" >$N
sed -e "
	/ [RTDW] / {
		s/:.* [RTDW] / /
		w $S
		d
	}
	/ U / {
		s/:.* U / /
		w $R
	}
	d
" <$N

#
# Elide entries representing a reference to a symbol from within the
# library that defines it.
#
sort -u -o $S $S
sort -u -o $R $R
comm -23 $R $S >$T
mv $T $R

#
# Make sure that all inputs get into the output.
#
for i ; do
	echo "$i" "$i"
done

#
# Sort references and symbols on the second field (the symbol), join
# on that field, and print out the file names.
#
sort -k 2 -o $R $R
sort -k 2 -o $S $S
join -j 2 -o 1.1 2.1 $R $S
