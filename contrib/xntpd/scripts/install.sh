#!/bin/sh
#
# Emulate the BSD install command with cpset for System V
# Tom Moore - NCR Corporation
#
PATH=/bin:/etc:/usr/bin:/usr/ucb
export PATH

# Default values
mode=0755
owner=bin
group=bin
strip=FALSE
remove=TRUE

USAGE="install [-s] [-c] [-m mode] [-o owner] [-g group] source file|directory"
set -- `getopt scm:o:g: $*` || {
	echo $USAGE >&2
	exit 2
}
for option in $*
do
	case $option in
	-s)	# Strip the installed file
		strip=TRUE
		shift
		;;
	-c)	# Copy the source file rather than move it
		remove=FALSE
		shift
		;;
	-m)	# File mode
		mode=$2
		shift 2
		;;
	-o)	# File owner
		owner=$2
		shift 2
		;;
	-g)	# File group
		group=$2
		shift 2
		;;
	--)	# End of options
		shift
		break
		;;
	esac
done

case $# in
0)	echo "install: no file or destination specified" >&2
	exit 2
	;;
1)	echo "install: no destination specified" >&2
	exit 2
	;;
2)	source=$1
	destination=$2
	;;
*)	echo "install: too many files specified" >&2
	exit 2
	;;
esac

[ $source = $destination -o $destination = . ] && {
	echo "install: can't move $source onto itself" >&2
	exit 1
}

[ -f $source ] || {
	echo "install: can't open $source" >&2
	exit 1
}

if [ -d $destination ]
then
	file=`basename $source`
	OLDdestination=$destination/OLD$file
	destination=$destination/$file
else
	dir=`dirname $destination`
	file=`basename $destination`
	OLDdestination=$dir/OLD$file
fi

(cp $source $destination &&
  chmod $mode $destination &&
  chown $owner $destination &&
  chgrp $group $destination) || exit 1

# /bin/rm -f $OLDdestination

[ $strip = TRUE ] &&
	strip $destination

[ $remove = TRUE ] &&
	rm -f $source

exit 0
