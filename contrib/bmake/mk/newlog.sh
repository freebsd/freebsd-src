#!/bin/sh

# NAME:
#	newlog - rotate log files
#
# SYNOPSIS:
#	newlog.sh [options] "log"[:"num"] ...
#
# DESCRIPTION:
#	This script saves multiple generations of each "log".
#	The "logs" are kept compressed except for the current and
#	previous ones.
#
#	Options:
#
#	-C "compress"
#		Compact old logs (other than .0) with "compress"
#		(default is 'gzip' or 'compress' if no 'gzip').
#
#	-E "ext"
#		If "compress" produces a file extention other than
#		'.Z' or '.gz' we need to know.
#
#	-G "gens"
#		"gens" is a comma separated list of "log":"num" pairs
#		that allows certain logs to handled differently.
#
#	-N	Don't actually do anything, just show us.
#
#	-R	Rotate rather than save logs by default.
#		This is the default anyway.
#
#	-S	Save rather than rotate logs by default.
#		Each log is saved to a unique name that remains
#		unchanged.  This results in far less churn.
#
#	-f "fmt"
#		Format ('%Y%m%d.%H%M%S') for suffix added to "log" to
#		uniquely name it when using the '-S' option.
#		If a "log" is saved more than once per second we add
#		an extra suffix of our process-id.
#
#	-d	The "log" to be rotated/saved is a directory.
#		We leave the mode of old directories alone.
#
#	-e	Normally logs are only cycled if non-empty, this
#		option forces empty logs to be cycled as well.
#
#	-g "group"
#		Set the group of "log" to "group".
#
#	-m "mode"
#		Set the mode of "log".
#
#	-M "mode"
#		Set the mode of old logs (default 444).
#
#	-n "num"
#		Keep "num" generations of "log".
#
#	-o "owner"
#		Set the owner of "log".
#
#	Regardless of whether '-R' or '-S' is provided, we attempt to
#	choose the correct behavior based on observation of "log.0" if
#	it exists; if it is a symbolic link, we save, otherwise
#	we rotate.
#
# BUGS:
#	'Newlog.sh' tries to avoid being fooled by symbolic links, but
#	multiply indirect symlinks are only handled on machines where
#	test(1) supports a check for symlinks.
#
# AUTHOR:
#	Simon J. Gerraty <sjg@crufty.net>
#

# RCSid:
#	$Id: newlog.sh,v 1.27 2024/02/17 17:26:57 sjg Exp $
#
#	SPDX-License-Identifier: BSD-2-Clause
#
#	@(#) Copyright (c) 1993-2016 Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

Mydir=`dirname $0`
case $Mydir in
/*) ;;
*) Mydir=`cd $Mydir; pwd`;;
esac

# places to find chown (and setopts.sh)
PATH=$PATH:/usr/etc:/sbin:/usr/sbin:/usr/local/share/bin:/share/bin:$Mydir

# linux doesn't necessarily have compress,
# and gzip appears in various locations...
Which() {
	case "$1" in
	-*) t=$1; shift;;
	*) t=-x;;
	esac
	case "$1" in
	/*)	test $t $1 && echo $1;;
	*)
		for d in `IFS=:; echo ${2:-$PATH}`
		do
			test $t $d/$1 && { echo $d/$1; break; }
		done
		;;
	esac
}

# shell's typically have test(1) as built-in
# and not all support all options.
test_opt() {
    _o=$1
    _a=$2
    _t=${3:-/}
    
    case `test -$_o $_t 2>&1` in
    *:*) eval test_$_o=$_a;;
    *) eval test_$_o=-$_o;;
    esac
}

# convert find/ls mode to octal
fmode() {
	eval `echo $1 |
		sed 's,\(.\)\(...\)\(...\)\(...\),ft=\1 um=\2 gm=\3 om=\4,'`
	sm=
	case "$um" in
	*s*)	sm=r
		um=`echo $um | sed 's,s,x,'`
		;;
	*)	sm=-;;
	esac
	case "$gm" in
	*[Ss]*)
		sm=${sm}w
		gm=`echo $gm | sed 's,s,x,;s,S,-,'`
		;;
	*)	sm=${sm}-;;
	esac
	case "$om" in
	*t)
		sm=${sm}x
		om=`echo $om | sed 's,t,x,'`
		;;
	*)	sm=${sm}-;;
	esac
	echo $sm $um $gm $om |
	sed 's,rwx,7,g;s,rw-,6,g;s,r-x,5,g;s,r--,4,g;s,-wx,3,g;s,-w-,2,g;s,--x,1,g;s,---,0,g;s, ,,g'
}

get_mode() {
	case "$OS,$STAT" in
	FreeBSD,*)
		$STAT -f %Op $1 | sed 's,.*\(....\),\1,'
		return
		;;
	esac
	# fallback to find
	fmode `find $1 -ls -prune | awk '{ print $3 }'`
}

get_mtime_suffix() {
	case "$OS,$STAT" in
	FreeBSD,*)
		$STAT -t "${2:-$opt_f}" -f %Sm $1
		return
		;;
	esac
	# this will have to do
	date "+${2:-$opt_f}"
}

case /$0 in
*/newlog*) rotate_func=rotate_log;;
*/save*) rotate_func=save_log;;
*) rotate_func=rotate_log;;
esac

opt_n=7
opt_m=
opt_M=444
opt_f=%Y%m%d.%H%M%S
opt_str=dNn:o:g:G:C:M:m:eE:f:RS

. setopts.sh

test $# -gt 0 || exit 0	# nothing to do.

OS=${OS:-`uname`}
STAT=${STAT:-`Which stat`}

# sorry, setops semantics for booleans changed.
case "${opt_d:-0}" in
0)	rm_f=-f
	opt_d=-f
	for x in $opt_C gzip compress
	do
		opt_C=`Which $x "/bin:/usr/bin:$PATH"`
		test -x $opt_C && break
	done
	empty() { test ! -s $1; }
	;;
*)	rm_f=-rf
	opt_d=-d
	opt_M=
	opt_C=:
	empty() { 
	    if [ -d $1 ]; then
		n=`'ls' -a1 $1/. | wc -l`
		[ $n -gt 2 ] && return 1
	    fi
	    return 0
	}
	;;
esac
case "${opt_N:-0}" in
0)	ECHO=;;
*)	ECHO=echo;;
esac
case "${opt_e:-0}" in
0)	force=;;
*)	force=yes;;
esac
case "${opt_R:-0}" in
0) ;;
*) rotate_func=rotate_log;;
esac
case "${opt_S:-0}" in
0) ;;
*) rotate_func=save_log opt_S=;;
esac

# see whether test handles -h or -L
test_opt L -h
test_opt h ""
case "$test_L,$test_h" in
-h,) test_L= ;;			# we don't support either!
esac

case "$test_L" in
"")	# No, so this is about all we can do...
	logs=`'ls' -ld $* | awk '{ print $NF }'`
	;;
*)	# it does
	logs="$*"
	;;
esac

read_link() {
	case "$test_L" in
	"")	'ls' -ld $1 | awk '{ print $NF }'; return;;
	esac
	if test $test_L $1; then
		'ls' -ld $1 | sed 's,.*> ,,'
	else
		echo $1
	fi
}

# create the new log
new_log() {
	log=$1
	mode=$2
	if test "x$opt_M" != x; then
		$ECHO chmod $opt_M $log.0 2> /dev/null
	fi
	# someone may have managed to write to it already
	# so don't truncate it.
	case "$opt_d" in
	-d) $ECHO mkdir -p $log;;
	*) $ECHO touch $log;;
	esac
	# the order here matters
	test "x$opt_o" = x || $ECHO chown $opt_o $log
	test "x$opt_g" = x || $ECHO chgrp $opt_g $log
	test "x$mode" = x || $ECHO chmod $mode $log
}

rotate_log() {
	log=$1
	n=${2:-$opt_n}

	# make sure excess generations are trimmed
	$ECHO rm $rm_f `echo $log.$n | sed 's/\([0-9]\)$/[\1-9]*/'`

	mode=${opt_m:-`get_mode $log`}
	while test $n -gt 0
	do
		p=`expr $n - 1`
		if test -s $log.$p; then
			$ECHO rm $rm_f $log.$p.*
			$ECHO $opt_C $log.$p
			if test "x$opt_M" != x; then
				$ECHO chmod $opt_M $log.$p.* 2> /dev/null
			fi
		fi
		for ext in $opt_E .gz .Z ""
		do
			test $opt_d $log.$p$ext || continue
			$ECHO mv $log.$p$ext $log.$n$ext
		done
		n=$p
	done
	# leave $log.0 uncompressed incase some one still has it open.
	$ECHO mv $log $log.0
	new_log $log $mode
}

# unlike rotate_log we do not rotate files,
# but give each log a unique (but stable name).
# This avoids churn for folk who rsync things.
# We make log.0 a symlink to the most recent log
# so it can be found and compressed next time around.
save_log() {
	log=$1
	n=${2:-$opt_n}
	fmt=$3

	last=`read_link $log.0`
	case "$last" in
	$log.0) # should never happen
		test -s $last && $ECHO mv $last $log.$$;;
	$log.*)
		$ECHO $opt_C $last
		;;
	*.*)	$ECHO $opt_C `dirname $log`/$last
		;;
	esac
	$ECHO rm -f $log.0
	# remove excess logs - we rely on mtime!
	$ECHO rm $rm_f `'ls' -1td $log.* 2> /dev/null | sed "1,${n}d"`

	mode=${opt_m:-`get_mode $log`}
	# this is our default suffix
	opt_S=${opt_S:-`get_mtime_suffix $log $fmt`}
	case "$fmt" in
	""|$opt_f) suffix=$opt_S;;
	*) suffix=`get_mtime_suffix $log $fmt`;;
	esac

	# find a unique name to save current log as
	for nlog in $log.$suffix $log.$suffix.$$
	do
		for f in $nlog*
		do
			break
		done
		test $opt_d $f || break
	done
	# leave $log.0 uncompressed incase some one still has it open.
	$ECHO mv $log $nlog
	test "x$opt_M" = x || $ECHO chmod $opt_M $nlog 2> /dev/null
	$ECHO ln -s `basename $nlog` $log.0
	new_log $log $mode
}

for f in $logs
do
	n=$opt_n
	save=
	case "$f" in
	*:[1-9]*)
		set -- `IFS=:; echo $f`; f=$1; n=$2;;
	*:n=*|*:save=*)
		eval `echo "f=$f" | tr ':' ' '`;;
	esac
	# try and pick the right function to use
	rfunc=$rotate_func	# default
	if test $opt_d $f.0; then
		case `read_link $f.0` in
		$f.0) rfunc=rotate_log;;
		*) rfunc=save_log;;
		esac
	fi
	case "$test_L" in
	-?)
		while test $test_L $f	# it is [still] a symlink
		do
			f=`read_link $f`
		done
		;;
	esac
	case ",${opt_G}," in
	*,${f}:n=*|,${f}:save=*)
		eval `echo ",${opt_G}," | sed "s!.*,${f}:\([^,]*\),.*!\1!;s,:, ,g"`
		;;
	*,${f}:*)
		# opt_G is a , separated list of log:n pairs
		n=`echo ,$opt_G, | sed -e "s,.*${f}:\([0-9][0-9]*\).*,\1,"`
		;;
	esac

	if empty $f; then
		test "$force" || continue
	fi

	test "$save" && rfunc=save_log

	$rfunc $f $n $save
done
