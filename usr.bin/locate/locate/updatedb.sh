#!/bin/sh
#
# Copyright (c) 1989, 1993
#	The Regents of the University of California.  All rights reserved.
#
# This code is derived from software contributed to Berkeley by
# James A. Woods.
#
# Modified to be a /bin/sh script by Nate Williams
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
#	@(#)updatedb.csh	8.3 (Berkeley) 3/19/94
#

SRCHPATHS="/"				# directories to be put in the database
LIBDIR="/usr/libexec"			# for subprograms
FCODES="/var/db/locate.database"	# the database
if [ "$TMPDIR" = "" ]; then
    TMPDIR="/var/tmp"			# for temp files
fi

PATH=/bin:/usr/bin
BIGRAMS="$TMPDIR/locate.bigrams.$$"
FILELIST="$TMPDIR/locate.list.$$"
ERRS="$TMPDIR/locate.errs.$$"

# Make a file list and compute common bigrams.
# Alphabetize '/' before any other char with 'tr'.
# If the system is very short of sort space, 'bigram' can be made
# smarter to accumulate common bigrams directly without sorting
# ('awk', with its associative memory capacity, can do this in several
# lines, but is too slow, and runs out of string space on small machines).

# search locally or everything
# find ${SRCHPATHS} -print | \
find ${SRCHPATHS} ! -fstype local -prune -or -print | \
	tr '/' '\001' | \
	(sort -T $TMPDIR -f; echo $? > $ERRS) | tr '\001' '/' > $FILELIST

$LIBDIR/locate.bigram < $FILELIST | \
	(sort -T $TMPDIR ; echo $? >> $ERRS) | \
	uniq -c | sort -T $TMPDIR -nr | \
	awk '{ if (NR <= 128) print $2 }' | tr -d '\012' > $BIGRAMS

# code the file list
if [ `sort -u $ERRS | grep -s -v 0` ]; then
	printf 'locate: updatedb failed\n\n'
else
	$LIBDIR/locate.code $BIGRAMS < $FILELIST > $FCODES
	chmod 644 $FCODES
	rm $BIGRAMS $FILELIST $ERRS
fi
