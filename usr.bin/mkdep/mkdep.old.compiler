#!/bin/sh -
#
# Copyright (c) 1991, 1993
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
#	@(#)mkdep.old.compiler	8.1 (Berkeley) 6/6/93
#

# This is a version of mkdep that works pretty well
# with compilers that don't have -M.
MAKE=Makefile			# default makefile name is "Makefile"

PATH=/bin:/usr/bin:/usr/ucb:/lib:/usr/lib

INCL=

while :
	do case "$1" in
		# -f allows you to select a makefile name
		-f)
			MAKE=$2
			shift; shift ;;

		# the -p flag produces "program: program.c" style dependencies
		# so .o's don't get produced
		-p)
			SED='s;\.o;;'
			shift ;;
		*)
			break ;;
	esac
done

if [ $# = 0 ] ; then
	echo 'usage: mkdep [-p] [-f makefile] [flags] file ...'
	exit 1
fi

if [ ! -w $MAKE ]; then
	echo "mkdep: no writeable file \"$MAKE\""
	exit 1
fi

TMP=/tmp/mkdep$$

trap 'rm -f $TMP ; exit 1' 1 2 3 13 15

cp $MAKE ${MAKE}.bak
sed -e '/DO NOT DELETE THIS LINE/,$d' < $MAKE > $TMP

cat << _EOF_ >> $TMP
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

_EOF_


for i do
	case "$i" in
	-c|-M|-O)
		;;
	-I*)
		INCL="$INCL $i";;
	-D*|-U*)
		FLAGS="$FLAGS $i";;
	*)
		# assume source file
		# put '$dep' in front of dependencies
		dep=`echo "$i" | sed -e 's,/,\\\\/,g' -e 's/\.c$/.o/'`

		# Find includes, remove leading numerics, remove ./,
		# remove double quotes, and remove trailing numerics.
		# Sort that, discarding duplicates, and add '$dep'.
		cpp $INCL $FLAGS "$i" | sed -e '
			/^#/!d
			s/# [0-9]* //
			s,"./,",
			s/"\(.*\)"/\1/
			s/ [ 0-9]*$//' |
		sort -u | sed -e "s/^/$dep: /";;
	esac
done |
sed "
	s; \./; ;g
	/\.c:$/d
	$SED" |
awk '{
	if ($1 != prev) {
		if (rec != "")
			print rec;
		rec = $0;
		prev = $1;
	}
	else {
		if (length(rec $2) > 78) {
			print rec;
			rec = $0;
		}
		else
			rec = rec " " $2
	}
}
END {
	print rec
}' >> $TMP

cat << _EOF_ >> $TMP

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
_EOF_

# copy to preserve permissions
cp $TMP $MAKE
rm -f $TMP
exit 0
