:
# This is mostly redundant.
# These days I use the pseudo machine "host" when building for host
# and $TARGET_HOST for its objdir

# RCSid:
#	$Id: machine.sh,v 1.19 2023/01/17 18:30:21 sjg Exp $
#
#	@(#) Copyright (c) 1994-2023 Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that 
#	the above copyright notice and this notice are
#	left intact. 
#      
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

# leverage os.sh
Mydir=`dirname $0`
. $Mydir/os.sh

# some further overrides - mostly for MACHINE_ACH
case $OS in
AIX)	# from http://gnats.netbsd.org/29386
	MACHINE_ARCH=`bootinfo -T`
	;;
Bitrig)
	MACHINE_ARCH=$MACHINE;
	;;
HP-UX)
	MACHINE_ARCH=`IFS="/-."; set $MACHINE; echo $1`
	;;
esac

(
case "$0" in
arch*)	echo $MACHINE_ARCH;;
*)
	case "$1" in
	"")	echo $MACHINE;;
	*)	echo $MACHINE_ARCH;;
	esac
	;;
esac
) | toLower
