#!/bin/sh
#
# apropos -- search the whatis database for keywords.
#
# Copyright (c) February 1996 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
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
# $FreeBSD$


PATH=/bin:/usr/bin:$PATH
db=whatis	# name of whatis data base
grepopt=''

# man -k complain if exit_nomatch=1 and no keyword matched
: ${exit_nomatch=0}	
exit_error=2

# argument test
case $# in 0)  
	echo "usage: `basename $0` keyword ..." >&2
	exit $exit_error
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

man_locales=`%bindir%/manpath -qL`

# search for existing */whatis databases
mandir=''
for d in $manpath
do
        if [ -f "$d/$db" -a -r "$d/$db" ]
	then
		mandir="$mandir $d/$db"
	fi

	# Check for localized manpage subdirectories
	if [ X"$man_locales" != X ]; then
		for l in $man_locales
		do
			if [ -f "$d/$l/$db" -a -r "$d/$l/$db" ];
			then
				mandir="$mandir $d/$l/$db"
			fi
		done
	fi
done

case X"$mandir" in X)
	echo "`basename $0`: no whatis databases in $manpath" >&2
	exit $exit_error
esac


for manpage
do
	if grep -hi $grepopt -- "$manpage" $mandir; then :
	else
        	echo "$manpage: nothing appropriate"
	fi
done | 

( 	# start $PAGER only if we find a manual page
	while read line
 	do
		case $line in
			# collect error(s)
			*": nothing appropriate") line2="$line2$line\n";;
			# matched line or EOF
			*) break;;
		esac
	done

	# nothing found, exit
	if [ -z "$line" -a ! -z "$line2"]; then
		printf -- "$line2"
		exit $exit_nomatch
	else
		( printf -- "$line2"; echo "$line"; cat ) | $PAGER
	fi
)
