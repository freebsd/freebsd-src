#!/bin/sh
#
# (c) Wolfram Schneider, September 1995. Public domain.
#
# mklocatedb - build locate database
# 
# usage: mklocatedb [-presort] < filelist > database
#
# $Id: mklocatedb.sh,v 1.2 1996/04/20 21:55:21 wosch Exp wosch $


# The directory containing locate subprograms
: ${LIBEXECDIR=/usr/libexec}; export LIBEXECDIR

PATH=$LIBEXECDIR:/bin:/usr/bin:$PATH; export PATH

umask 077			# protect temp files

: ${TMPDIR=/tmp}; export TMPDIR; 
if test X"$TMPDIR" = X -o ! -d "$TMPDIR"; then
	TMPDIR=/tmp; export TMPDIR
fi

# utilities to built locate database
: ${bigram=locate.bigram}
: ${code=locate.code}
: ${sort=sort}


sortopt="-u -T $TMPDIR"
sortcmd=$sort

# Input already sorted
case X"$1" in 
	X-nosort|X-presort) sortcmd=cat; sortopt=;shift;; 
esac


bigrams=$TMPDIR/_mklocatedb$$.bigrams
filelist=$TMPDIR/_mklocatedb$$.list

trap 'rm -f $bigrams $filelist' 0 1 2 3 5 10 15


if $sortcmd $sortopt > $filelist; then
        $bigram < $filelist | $sort -nr | 
                awk 'NR <= 128 { printf $2 }' > $bigrams &&
        $code $bigrams < $filelist 
else
        echo "`basename $0`: cannot build locate database" >&2
        exit 1
fi
