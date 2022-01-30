#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) September 1995-2022 Wolfram Schneider <wosch@FreeBSD.org>
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
# $FreeBSD$

# stop on first error
set -e
set -o pipefail

# The directory containing locate subprograms
: ${LIBEXECDIR:=/usr/libexec}; export LIBEXECDIR
: ${TMPDIR:=/var/tmp}; export TMPDIR

PATH=$LIBEXECDIR:/bin:/usr/bin:$PATH; export PATH

# utilities to built locate database
: ${bigram:=locate.bigram}
: ${code:=locate.code}
: ${locate:=locate}
: ${sort:=sort}

sort_opt="-u -T $TMPDIR -S 20%"

bigrams=$(mktemp -t mklocatedb.bigrams)
filelist=$(mktemp -t mklocatedb.filelist)

trap 'rm -f $bigrams $filelist' 0 1 2 3 5 10 15

# Input already sorted
if [ X"$1" = "X-presort" ]; then
    shift; 

    # Locate database bootstrapping
    # 1. first build a temp database without bigram compression
    # 2. create the bigram from the temp database
    # 3. create the real locate database with bigram compression.
    #
    # This scheme avoid large temporary files in /tmp

    $code $bigrams > $filelist
    $locate -d $filelist / | $bigram | $sort -nr | \
      awk 'NR <= 128 && /^[ \t]*[1-9][0-9]*[ \t]+..$/ { printf("%s", substr($0, length($0)-1, 2)) }' > $bigrams
    $locate -d $filelist / | $code $bigrams
else
    $sort $sort_opt > $filelist
    $bigram < $filelist | $sort -nr | \
      awk 'NR <= 128 && /^[ \t]*[1-9][0-9]*[ \t]+..$/ { printf("%s", substr($0, length($0)-1, 2)) }' > $bigrams
    $code $bigrams < $filelist
fi

#EOF
