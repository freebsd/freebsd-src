#!/bin/sh
#
# (c) Wolfram Schneider, Berlin. September 1995. Public domain.
#
# concatdb - concatenate locate databases
# 
# usage: concatdb database1 ... databaseN > newdb
#
# Sequence of databases is important.
#
# $Id: concatdb.sh,v 1.2 1996/04/20 21:55:21 wosch Exp wosch $

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


case $# in 
        [01]) 	echo 'usage: concatdb databases1 ... databaseN > newdb'
		exit 1
		;;
esac


bigrams=$TMPDIR/_concatdb$$.bigrams
trap 'rm -f $bigrams' 0 1 2 3 5 10 15

for db 
do
       $locate -d $db /
done | $bigram | $sort -nr | awk 'NR <= 128 { printf $2 }' > $bigrams

for db
do
	$locate -d $db /
done | $code $bigrams
