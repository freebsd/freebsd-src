#!/bin/sh
#
# Copyright (c) 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
#       systags.sh				17-Aug-98
#
# script to make hypertext of kernel source.
# support OS: FreeBSD, NetBSD, OpenBSD, Linux, GNUmach, GNUhurd
#
while case $1 in
	-*)
		if grep '[^-cfgn]' <<! >/dev/null 2>&1; then
$1
!
			echo "usage: systags [-c][-f][-g][-n][dir]"
			exit 1
		fi
		case $1 in
		*c*)	cflag=c;;
		esac
		case $1 in
		*f*)	fflag=f;;
		esac
		case $1 in
		*g*)	gflag=g;;
		esac
		case $1 in
		*n*)	nflag=n;;
		esac
		;;
	*)	false;;
	esac
do
	shift
done
case $1 in
"")	dir=.;;
*)	dir=$1;;
esac
if [ ! -d $dir -o ! -w $dir ]; then
	echo "systags: '$dir' is not a directory or not writable."
	exit 1
fi
tmpdir=$dir/systags_tmpdir_$$
#
# get release number from source tree.
#
if [ -f conf/newvers.sh ]; then
	# (Free|Net|Open)BSD?
	if ! mkdir $tmpdir 2>/dev/null; then
		echo "systags: '$dir' is not writable."
		exit 1
	fi
	cwd=`pwd`
	(cd $tmpdir; sh $cwd/conf/newvers.sh)
	os=`awk -F\" '/char[ \t]+ostype\[\][ \t]*=[ \t]*\"[^"]+\"/ {print $2}' < $tmpdir/vers.c`;
	release=`awk -F\" '/char[ \t]+osrelease\[\][ \t]*=[ \t]*\"[^"]+\"/ {print $2}' < $tmpdir/vers.c`;
	rm -rf $tmpdir
elif [ -f Makefile ] && grep '^vmlinux:' Makefile >/dev/null; then
	# Linux?
	os=Linux
	version=`awk -F= '/^VERSION *=/ {print $2}' < Makefile`
	patchlevel=`awk -F= '/^PATCHLEVEL *=/ {print $2}' < Makefile`
	sublevel=`awk -F= '/^SUBLEVEL *=/ {print $2}' < Makefile`
	release=`echo "$version.$patchlevel.$sublevel" | tr -d ' '`
elif [ -f version.c ]; then
	# GNU mach?
	version=`awk -F\" '/char[ \t]+version\[\][ \t]*=[ \t]*\"[^"]+\"/ {print $2}' < version.c`
	os=`echo $version | awk '{print $1}'` 
	release=`echo $version | awk '{print $2}'` 
elif [ -f version.h ]; then
	# GNU hurd?
	release=`awk -F\" '/^#define[ \t]+HURD_VERSION[ \t]+"[^"]+\"/ {print $2}' < version.h`
	if [ ${release}X != X ]; then
		os=GNUhurd
	fi
fi
#
# remove old files
#
files=
for f in htags.log gtags.log GTAGS GRTAGS GSYMS GPATH HTML; do
	files="$files $dir/$f";
done
com="rm -rf $files"
case $nflag in
n)	echo $com;;
*)	eval $com;;
esac
#
# FreeBSD System macros.
#
#	These macros with argument are used out of function.
#	gctags(1) knows these are not function by '.notfunction' list.
#
# kernel.h	MAKE_SET,TEXT_SET,DATA_SET,BSS_SET,ABS_SET,
#		SYSINIT,SYSINIT_KT,SYSINIT_KP,PSEUDO_SET
# sysctl.h	SYSCTL_OID,SYSCTL_NODE,SYSCTL_STRING,SYSCTL_INT,SYSCTL_OPAQUE,
#		SYSCTL_STRUCT,SYSCTL_PROC
# domain.h	DOMAIN_SET
# mount.h	VFS_SET
# lkm.h		MOD_DECL,MOD_SYSCALL,MOD_VFS,MOD_DEV,MOD_EXEC,MOD_MISC
# vnode.h	VNODEOP_SET
# spl.h		GENSPL
# queue.h	SLIST_HEAD,SLIST_ENTRY,SLIST_INIT,SLIST_INSERT_AFTER,
#		SLIST_INSERT_HEAD,SLIST_REMOVE_HEAD,SLIST_REMOVE,STAILQ_HEAD,
#		STAILQ_ENTRY,STAILQ_INIT,STAILQ_INSERT_HEAD,STAILQ_INSERT_TAIL,
#		STAILQ_INSERT_AFTER,STAILQ_REMOVE_HEAD,STAILQ_REMOVE,
#		LIST_HEAD,LIST_ENTRY,LIST_INIT,LIST_INSERT_AFTER,LIST_INSERT_BEFORE,
#		LIST_INSERT_HEAD,LIST_REMOVE,TAILQ_HEAD,TAILQ_ENTRY,
#		TAILQ_EMPTY,TAILQ_FIRST,TAILQ_LAST,TAILQ_NEXT,TAILQ_PREV,
#		TAILQ_INIT,TAILQ_INSERT_HEAD,TAILQ_INSERT_TAIL,TAILQ_INSERT_AFTER,
#		TAILQ_INSERT_BEFORE,TAILQ_REMOVE,CIRCLEQ_HEAD,CIRCLEQ_ENTRY,
#		CIRCLEQ_INIT,CIRCLEQ_INSERT_AFTER,CIRCLEQ_INSERT_BEFORE,
#		CIRCLEQ_INSERT_HEAD,CIRCLEQ_INSERT_TAIL,CIRCLEQ_REMOVE
#
case $os in
FreeBSD)
	cat <<-! >.notfunction
	MAKE_SET
	TEXT_SET
	DATA_SET
	BSS_SET
	ABS_SET
	SYSINIT
	SYSINIT_KT
	SYSINIT_KP
	PSEUDO_SET
	SYSCTL_OID
	SYSCTL_NODE
	SYSCTL_STRING
	SYSCTL_INT
	SYSCTL_OPAQUE
	SYSCTL_STRUCT
	SYSCTL_PROC
	DOMAIN_SET
	VFS_SET
	MOD_DECL
	MOD_SYSCALL
	MOD_VFS
	MOD_DEV
	MOD_EXEC
	MOD_MISC
	VNODEOP_SET
	GENSPL
	SLIST_HEAD
	SLIST_ENTRY
	SLIST_INIT
	SLIST_INSERT_AFTER
	SLIST_INSERT_HEAD
	SLIST_REMOVE_HEAD
	SLIST_REMOVE
	STAILQ_HEAD
	STAILQ_ENTRY
	STAILQ_INIT
	STAILQ_INSERT_HEAD
	STAILQ_INSERT_TAIL
	STAILQ_INSERT_AFTER
	STAILQ_REMOVE_HEAD
	STAILQ_REMOVE
	LIST_HEAD
	LIST_ENTRY
	LIST_INIT
	LIST_INSERT_AFTER
	LIST_INSERT_BEFORE
	LIST_INSERT_HEAD
	LIST_REMOVE
	TAILQ_HEAD
	TAILQ_ENTRY
	TAILQ_EMPTY
	TAILQ_FIRST
	TAILQ_LAST
	TAILQ_NEXT
	TAILQ_PREV
	TAILQ_INIT
	TAILQ_INSERT_HEAD
	TAILQ_INSERT_TAIL
	TAILQ_INSERT_AFTER
	TAILQ_INSERT_BEFORE
	TAILQ_REMOVE
	CIRCLEQ_HEAD
	CIRCLEQ_ENTRY
	CIRCLEQ_INIT
	CIRCLEQ_INSERT_AFTER
	CIRCLEQ_INSERT_BEFORE
	CIRCLEQ_INSERT_HEAD
	CIRCLEQ_INSERT_TAIL
	CIRCLEQ_REMOVE
!
esac
#
# make global database(GTAGS, GRTAGS).
#
com="gtags -owv $dir > $dir/gtags.log 2>&1"
case $nflag in
n)	echo $com;;
*)	eval $com;;
esac
case $? in
0)	;;
*)	exit 1;;
esac
case $gflag in
g)	exit 0;;
esac
#
# make hypertext.
# (please replace this title with a suitable one.) 
#
if [ ${os}X != X -a ${release}X != X ]; then
	title="Welcome to $os $release kernel source tour!"
else
	program=`/bin/pwd | sed 's/.*\///'`
	title="Welcome to $program source tour!"
fi
com="htags -${cflag}${fflag}lhnvat '$title' -d $dir $dir > $dir/htags.log 2>&1"
case $nflag in
n)	echo $com;;
*)	eval $com;;
esac
