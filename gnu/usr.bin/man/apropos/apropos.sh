#!/bin/sh
#
# apropos -- search the whatis database for keywords.
#
# Copyright (c) 1990, 1991, John W. Eaton.
#
# You may distribute under the terms of the GNU General Public
# License as specified in the README file that comes with the man
# distribution.  
#
# John W. Eaton
# jwe@che.utexas.edu
# Department of Chemical Engineering
# The University of Texas at Austin
# Austin, Texas  78712
#
# rewritten by Wolfram Schneider, Berlin, Feb 1996
#
# $Id$


PATH=/bin:/usr/bin:$PATH
db=whatis	# name of whatis data base
grepopt=''

# argument test
case $# in 0)  
	echo "usage: `basename $0` keyword ..." >&2
	exit 1
	;; 
esac

case "$0" in
	*whatis) grepopt='-w';;	# run as whatis(1)
	*)	 grepopt='';;	# otherwise run as apropos(1)
esac

# test manpath
manpath=`%bindir%/manpath -q | tr : '\040'`
case X"$manpath" in X) 
	echo "`basename $0`: manpath is null, use \"/usr/share/man\"" >&2
	manpath=/usr/share/man
	;;
esac


# reset $PAGER if $PAGER is empty
case X"$PAGER" in X) 
	PAGER="%pager%"
	;; 
esac

# search for existing */whatis databases
mandir=''
for d in $manpath
do
        if [ -f "$d/$db" -a -r "$d/$db" ]
	then
		mandir="$mandir $d/$db"
	fi
done

case X"$mandir" in X)
	echo "`basename $0`: no whatis databases in $manpath" >&2
	exit 1
esac


for manpage
do
	if grep -hi $grepopt "$manpage" $mandir; then :
	else
        	echo "$manpage: nothing appropriate"
	fi
done | $PAGER

