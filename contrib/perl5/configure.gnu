#! /bin/sh
#
# $Id: configure,v 3.0.1.1 1995/07/25 14:16:21 ram Exp $
#
# GNU configure-like front end to metaconfig's Configure.
#
# Written by Andy Dougherty <doughera@lafcol.lafayette.edu>
# and Matthew Green <mrg@mame.mu.oz.au>.
#
# Reformatted and modified for inclusion in the dist-3.0 package by
# Raphael Manfredi <ram@hptnos02.grenoble.hp.com>.
#
# This script belongs to the public domain and may be freely redistributed.
#
# The remaining of this leading shell comment may be removed if you
# include this script in your own package.
#
# $Log: configure,v $
# Revision 3.0.1.1  1995/07/25  14:16:21  ram
# patch56: created
#

(exit $?0) || exec sh $0 $argv:q

case "$0" in
*configure)
    if cmp $0 `echo $0 | sed -e s/configure/Configure/` >/dev/null; then
	echo "Your configure and Configure scripts seem to be identical."
	echo "This can happen on filesystems that aren't fully case sensitive."
	echo "You'll have to explicitly extract Configure and run that."
	exit 1
    fi
    ;;
esac

opts=''
verbose=''
create='-e'
while test $# -gt 0; do
	case $1 in
    --help)
		cat <<EOM
Usage: configure.gnu [options]
This is GNU configure-like front end for a metaconfig-generated Configure.
It emulates the following GNU configure options (must be fully spelled out):
	--help
	--no-create
	--prefix=PREFIX
 	--cache-file  (ignored)
	--quiet
	--silent
	--verbose
	--version

And it honours these environment variables: CC, CFLAGS and DEFS.
EOM
		exit 0
		;;
	--no-create)
		create='-E'
		shift
		;;
	--prefix=*)
		arg=`echo $1 | sed 's/--prefix=/-Dprefix=/'`
		opts="$opts $arg"
		shift
		;;
 	--cache-file=*)
 		shift		# Just ignore it.
 		;;
	--quiet|--silent)
		exec >/dev/null 2>&1
		shift
		;;
	--verbose)
		verbose=true
		shift
		;;
	--version)
		copt="$copt -V"
		shift
		;;
	--*)
		opt=`echo $1 | sed 's/=.*//'`
		echo "This GNU configure front end does not understand $opt"
		exit 1
		;;
	*)
		opts="$opts '$1'"
		shift
		;;
	esac
done

case "$CC" in
'') ;;
*) opts="$opts -Dcc='$CC'";;
esac

# Join DEFS and CFLAGS together.
ccflags=''
case "$DEFS" in
'') ;;
*) ccflags=$DEFS;;
esac
case "$CFLAGS" in
'') ;;
*) ccflags="$ccflags $CFLAGS";;
esac
case "$ccflags" in
'') ;;
*) opts="$opts -Dccflags='$ccflags'";;
esac
case "$LDFLAGS" in
'') ;;
*) ldflags="$ldflags $LDFLAGS";;
esac
case "$ldflags" in
'') ;;
*) opts="$opts -Dldflags='$ldflags'";;
esac

# Don't use -s if they want verbose mode
case "$verbose" in
'') copt="$copt -ds";;
*) copt="$copt -d";;
esac

eval "set X sh Configure $copt $create $opts"
shift
echo "$@"
exec "$@"
