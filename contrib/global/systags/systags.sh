#!/bin/sh
#
# Copyright (c) 1997 Shigio Yamaguchi. All rights reserved.
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
#       This product includes software developed by Shigio Yamaguchi.
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
#       systags.sh               7-Jul-97
#
# script to make hypertext of kernel source.
# supporting FreeBSD and Linux.
#
case $1 in
-n)	nflag=1; shift;;
esac
case $1 in
"")	dir=.;;
*)	dir=$1;;
esac
#
# get release number from source tree.
#
if [ -f conf/newvers.sh ]; then
	os=FreeBSD
	release=`awk -F= '/^RELEASE=/ {print $2}' < conf/newvers.sh`
elif [ -f Makefile ] && grep '^vmlinux:' Makefile >/dev/null; then
	os=Linux
	version=`awk -F= '/^VERSION *=/ {print $2}' < Makefile`
	patchlevel=`awk -F= '/^PATCHLEVEL *=/ {print $2}' < Makefile`
	sublevel=`awk -F= '/^SUBLEVEL *=/ {print $2}' < Makefile`
	release=`echo "$version.$patchlevel.$sublevel" | tr -d ' '`
fi
#
# remove old files
#
case $nflag in
1)	echo "rm -rf $dir/htags.log $dir/gtags.log $dir/GTAGS $dir/GRTAGS $dir/GSYMS $dir/HTML";;
*)	rm -rf $dir/htags.log $dir/gtags.log $dir/GTAGS $dir/GRTAGS $dir/GSYMS $dir/HTML;;
esac
#
# make global database(GTAGS, GRTAGS, GSYMS).
#
case $nflag in
1)	echo "gtags -v $dir > $dir/gtags.log 2>&1";;
*)	gtags -v $dir > $dir/gtags.log 2>&1;;
esac
case $? in
0)	;;
*)	exit 1;;
esac
#
# make hypertext.
# (please replace this title with a suitable one.) 
#
case $os$release in
"")	program=`/bin/pwd | sed 's/.*\///'`
	title="Welcome to $program source tour!";;
*)	title="Welcome to $os $release kernel source tour!";;
esac
case $nflag in
1)	echo "htags -fnvat '$title' -d $dir $dir > $dir/htags.log 2>&1";;
*)	htags -fnvat "$title" -d $dir $dir> $dir/htags.log 2>&1;;
esac
