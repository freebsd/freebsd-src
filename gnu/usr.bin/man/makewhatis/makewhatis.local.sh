#!/bin/sh
# (c) Wolfram Schneider, Berlin. April 1996. Public Domain.
#
# makewhatis.local - start makewhatis(1) only for file systems 
#		     physically mounted on the system
#
# Running makewhatis from /etc/weekly for rw nfs-mounted /usr may kill
# your NFS server -- all clients start makewhatis at the same time!
# So use this wrapper instead calling makewhatis directly.
#
# PS: this wrapper works also for catman(1)
#
# $Id: makewhatis.local.sh,v 1.1 1996/05/14 10:27:27 wosch Exp $

PATH=/bin:/usr/bin:$PATH; export PATH
opt= dirs= localdirs=

for arg
do
	case "$arg" in
		-*) 	opt="$opt $arg";;
		*)	dirs="$dirs $arg";;
	esac
done

dirs=`echo $dirs | sed 's/:/ /g'`
case X"$dirs" in X) echo "usage: $0 [options] directories ..."; exit 1;; esac

localdirs=`find -H $dirs -fstype local -type d -prune -print`

case X"$localdirs" in
	X) 	echo "$0: no local-mounted manual directories found: $dirs"
		exit 1;;
	*) 	exec `basename $0 .local` $opt $localdirs;;
esac
