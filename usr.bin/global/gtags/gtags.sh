#!/bin/sh 
#
# Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
#	This product includes software developed by Shigio Yamaguchi.
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
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
#	gtags.sh	21-Apr-97
#
com=`echo $0 | sed 's/.*\///'`		# command name
usage="usage: $com [-e][-s][dbpath]"
#
# ctags flag
#
eflag=
sflag=
while :; do
	case $1 in
	-*)
		if echo $1 | grep '[^-es]' >/dev/null; then
			echo $usage >/dev/tty; exit 1
		fi
		case $1 in
		-*e*)   eflag=e;;
		esac
		case $1 in
		-*s*)   sflag=1;;
		esac
		shift;;
	*)
		break;;
	esac
done
export eflag sflag
case $1 in
"")	dbpath=".";;
*)	dbpath=$1;;
esac
if [ -f $dbpath/GTAGS -a -f $dbpath/GRTAGS ]; then
	if [ ! -w $dbpath/GTAGS ]; then
		echo "$com: cannot write to GTAGS."
		exit 1
	elif [ ! -w $dbpath/GRTAGS ]; then
		echo "$com: cannot write to GRTAGS."
	        exit 1
	fi
elif [ ! -w $dbpath ]; then
	echo "$com: cannot write to the directory $dbpath."
	exit 1
fi
#
# make global database
#
for db in GTAGS GRTAGS; do
	# currently only *.c *.h *.y are supported.
	# *.s *.S is valid only when -s option specified.
	find . -type f -name "*.[chysS]" -print | while read f; do
		case $f in
		*y.tab.c|*y.tab.h)
			continue;;
		*.s|*.S)
			[ ${sflag}x = x -o $db = GRTAGS ] && continue
			perl -ne '($nouse, $tag) = /^(ENTRY|ALTENTRY)\((\w+)\)/;
			if ($tag) {printf("%-16s%4d %-16s %s", $tag, $., $ARGV, $_)} ' $f
			continue;;
		esac
		case $db in
		GRTAGS)	flag=${eflag}Dxr;;
		GTAGS)	flag=${eflag}Dx;;
		esac
		GTAGDBPATH=$dbpath gctags -$flag $f || exit 1
	done | btreeop -C $dbpath/$db
done
exit 0
