#!/bin/sh
#
# Copyright (c) September 1995 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
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
# mklocatedb - build locate database
# 
# usage: mklocatedb [-presort] < filelist > database
#
# $Id: mklocatedb.sh,v 1.2 1996/08/27 20:04:25 wosch Exp $


# The directory containing locate subprograms
: ${LIBEXECDIR=/usr/libexec}; export LIBEXECDIR

PATH=$LIBEXECDIR:/bin:/usr/bin:$PATH; export PATH

umask 077			# protect temp files

TMPDIR=${TMPDIR:-/tmp}; export TMPDIR
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
