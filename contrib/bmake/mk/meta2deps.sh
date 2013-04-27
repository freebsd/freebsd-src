#!/bin/sh

# NAME:
#	meta2deps.sh - extract useful info from .meta files
#
# SYNOPSIS:
#	meta2deps.sh SB="SB" "meta" ...
#	
# DESCRIPTION:
#	This script looks each "meta" file and extracts the
#	information needed to deduce build and src dependencies.
#	
#	To do this, we extract the 'CWD' record as well as all the
#	syscall traces which describe 'R'ead, 'C'hdir and 'E'xec
#	syscalls.
#
#	The typical meta file looks like::
#.nf
#
#	# Meta data file "path"
#	CMD "command-line"
#	CWD "cwd"
#	TARGET "target"
#	-- command output --
#	-- filemon acquired metadata --
#	# buildmon version 2
#	V 2
#	E "pid" "path"
#	R "pid" "path"
#	C "pid" "cwd"
#	R "pid" "path"
#	X "pid" "status"
#.fi
#
#	The fact that all the syscall entry lines start with a single
#	character make these files quite easy to process using sed(1).
#
#	To simplify the logic the 'CWD' line is made to look like a
#	normal 'C'hdir entry, and "cwd" is remembered so that it can
#	be prefixed to any "path" which is not absolute.
#
#	If the "path" being read ends in '.srcrel' it is the content
#	of (actually the first line of) that file that we are
#	interested in.
#
#	Any "path" which lies outside of the sandbox "SB" is generally
#	not of interest and is ignored.
#
#	The output, is a set of absolute paths with "SB" like:
#.nf
#
#	$SB/obj-i386/junos/gnu/lib/csu
#	$SB/obj-i386/junos/gnu/lib/libgcc
#	$SB/obj-i386/bsd/include
#	$SB/obj-i386/bsd/lib/csu/i386-elf
#	$SB/obj-i386/bsd/lib/libc
#	$SB/src/bsd/include
#	$SB/src/bsd/sys/i386/include
#	$SB/src/bsd/sys/sys
#	$SB/src/pan-release/rtsock
#	$SB/src/pfe-shared/include/jnx
#.fi
#
#	Which can then be further processed by 'gendirdeps.mk'
#
#	If we are passed 'DPDEPS='"dpdeps", then for each src file
#	outside of "CURDIR" we read, we output a line like:
#.nf
#
#	DPDEPS_$path += $RELDIR
#.fi
#
#	with "$path" geting turned into reldir's, so that we can end
#	up with a list of all the directories which depend on each src
#	file in another directory.  This can allow for efficient yet
#	complete testing of changes.


# RCSid:
#	$Id: meta2deps.sh,v 1.5 2013/02/10 19:21:46 sjg Exp $

# Copyright (c) 2010-2012, Juniper Networks, Inc.
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
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

meta2src() {
    cat /dev/null "$@" |
    sed -n '/^R .*\.[chyl]$/s,^..[0-9]* ,,p' |
    sort -u
}
    
meta2dirs() {
    cat /dev/null "$@" |
    sed -n '/^R .*\/.*\.[a-z0-9][^\/]*$/s,^..[0-9]* \(.*\)/[^/]*$,\1,p' |
    sort -u
}

meta2deps() {
    DPDEPS=
    while :
    do
	case "$1" in
	*=*) eval export "$1"; shift;;
	*) break;;
	esac
    done

    [ -z "$RELDIR" ] && unset DPDEPS
    tf=/tmp/m2d$$-$USER
    rm -f $tf.*
    trap 'rm -f $tf.*; trap 0' 0

    > $tf.dirdep
    > $tf.qual
    > $tf.srcdep
    > $tf.srcrel
    > $tf.dpdeps

    seenit=
    seensrc=
    lpid=
    cat /dev/null "$@" |
    sed -e 's,^CWD,C C,;/^[CREFL] /!d' -e "s,',,g" |
    while read op pid path junk
    do
	: op=$op pid=$pid path=$path
	# we track cwd and ldir (of interest) per pid
	# CWD is bmake's cwd
	case "$lpid,$pid" in
	,C) CWD=$path cwd=$path ldir=$path
	    if [ -z "$SB" ]; then
		SB=`echo $CWD | sed 's,/obj.*,,'`
	    fi
	    SRCTOP=${SRCTOP:-$SB/src}
	    continue
	    ;;
	$pid,$pid) ;;
	*)
	    case "$lpid" in
	    "") ;;
	    *) eval ldir_$lpid=$ldir cwd_$lpid=$cwd;;
	    esac
	    eval ldir=\${ldir_$pid:-$CWD} cwd=\${cwd_$pid:-$CWD}
	    lpid=$pid
	    ;;
	esac

	case "$op,$path" in
	*.dirdep)  continue;;
	W,*srcrel) continue;;
	C,*)
	    case "$path" in
	    /*) cwd=$path;;
	    *) cwd=`cd $cwd/$path 2> /dev/null && /bin/pwd`;;
	    esac
	    # watch out for temp dirs that no longer exist
	    test -d ${cwd:-/dev/null/no/such} || cwd=$CWD
	    continue
	    ;;
	F,*)  eval cwd_$path=$cwd ldir_$path=$ldir
	    continue
	    ;;	  
	*)  dir=${path%/*}
	    case "$path" in
	    $SB/*|${SB_BACKING_SB:-$SB}/*) ;;
	    $SB_OBJROOT*) ;;
	    /*/stage/*) ;;
	    /*) continue;;
	    *)	for path in $ldir/$path $cwd/$path
		do
			test -e $path && break
		done
		dir=${path%/*}
		;;
	    esac
	    ;;
	esac
	# avoid repeating ourselves...
	case "$DPDEPS,$seensrc," in
	,*)
	    case ",$seenit," in
	    *,$dir,*) continue;;
	    esac
	    ;;
	*,$path,*) continue;;
	esac
	# canonicalize if needed
	case "/$dir/" in
	*/../*|*/./*)
	    rdir=$dir
	    dir=`cd $dir 2> /dev/null && /bin/pwd`
	    seen="$rdir,$dir"
	    ;;
	*)  seen=$dir;;
	esac
	case "$dir" in
	${CURDIR:-.}|${CURDIR:-.}/*|"") continue;;
	$SRCTOP/*|${SB_BACKING_SB:-$SB}/src/*)
	    # avoid repeating ourselves...
	    case "$DPDEPS,$seensrc," in
	    ,*)
		case ",$seenit," in
		*,$dir,*) continue;;
		esac
		;;
	    esac
	    ;;
	*)
	    case ",$seenit," in
	    *,$dir,*) continue;;
	    esac
	    ;;
	esac
	if [ -d $path ]; then
	    case "$path" in
	    */..) ldir=${dir%/*};;
	    *) ldir=$path;;
	    esac
	    continue
	fi
	[ -f $path ] || continue
	case "$dir" in
	$CWD) continue;;		# ignore
	$SRCTOP/*|${SB_BACKING_SB:-$SB}/src/*)
	    seenit="$seenit,$seen"
	    echo $dir >> $tf.srcdep
	    case "$DPDEPS,$reldir,$seensrc," in
	    ,*) ;;
	    *)	seensrc="$seensrc,$path"
		echo "DPDEPS_$dir/${path##*/} += $RELDIR" >> $tf.dpdeps
		;;
	    esac
	    continue
	    ;;
	esac
	# if there is a .dirdep we cannot skip
	# just because we've seen the dir before.
	if [ -s $path.dirdep ]; then
	    # this file contains:
	    # '# ${RELDIR}.<machine>'
	    echo $path.dirdep >> $tf.qual
	    continue
	elif [ -s $dir.dirdep ]; then
	    echo $dir.dirdep >> $tf.qual
	    seenit="$seenit,$seen"
	    continue
	fi
	seenit="$seenit,$seen"
	case "$dir" in
	$SB/*|${SB_OBJROOT:-$SB/}*|${SB_BACKING_SB:-$SB}/*)
	    echo $dir;;
	esac
    done > $tf.dirdep
    _nl=echo
    for f in $tf.dirdep $tf.qual $tf.srcdep
    do
	[ -s $f ] || continue
	case $f in
	*qual) # a list of .dirdep files
	    # we can prefix everthing with $OBJTOP to
	    # tell gendirdeps.mk that these are
	    # DIRDEP entries, since they are already
	    # qualified with .<machine> as needed.
	    # We strip .$MACHINE though
	    xargs cat < $f | sort -u |
	    sed "s,^# ,,;s,^,$OBJTOP/,;s,\.$MACHINE\$,,"
	    ;;
	*)  sort -u $f;;
	esac
	_nl=:
    done
    if [ -s $tf.dpdeps ]; then
	case "$DPDEPS" in
	*/*) ;;
	*) echo > $DPDEPS;;		# the echo is needed!
	esac
	sort -u $tf.dpdeps |
	sed "s,${SRCTOP}/,,;s,${SB_BACKING_SB:-$SB}/src/,," >> $DPDEPS
    fi
    # ensure we produce _something_ else egrep -v gets upset
    $_nl
}

case /$0 in
*/meta2dep*) meta2deps "$@";;
*/meta2dirs*) meta2dirs "$@";;
*/meta2src*) meta2src "$@";;
esac
