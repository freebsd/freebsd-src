# irix_5.sh
# Tue Jan  9 16:04:38 EST 1996
#  Add note about socket patch.
#
# Tue Jan  2 14:52:36 EST 1996
# Apparently, there's a stdio bug that can lead to memory
# corruption using perl's malloc, but not SGI's malloc.
usemymalloc='n'

ld=ld
i_time='define'

case "$cc" in
*gcc*) ccflags="$ccflags -D_BSD_TYPES" ;;
*) ccflags="$ccflags -D_POSIX_SOURCE -ansiposix -D_BSD_TYPES -Olimit 4000" ;;
esac

lddlflags="-shared"
# For some reason we don't want -lsocket -lnsl or -ldl.  Can anyone
# contribute an explanation?
set `echo X "$libswanted "|sed -e 's/ socket / /' -e 's/ nsl / /' -e 's/ dl / /'`
shift
libswanted="$*"

# Date: Fri, 22 Dec 1995 11:49:17 -0800
# From: Matthew Black <black@csulb.edu>
# Subject: sockets broken under IRIX 5.3? YES...how to fix
# Anyone attempting to use perl4 or perl5 with SGI IRIX 5.3 may discover
# that sockets are essentially broken.  The syslog interface for perl also
# fails because it uses the broken socket interface.  This problem was
# reported to SGI as bug #255347 and it can be fixed by installing 
# patchSG0000596.  The patch can be downloaded from Advantage OnLine (SGI's
# WWW server) or from the Support Advantage 9/95 Patch CDROM.  Thanks to Tom 
# Christiansen and others who provided assistance.

case "$usethreads" in
$define|true|[yY]*)
        cat >&4 <<EOM
IRIX `uname -r` does not support POSIX threads.
You should upgrade to at least IRIX 6.2 with pthread patches.
EOM
	exit 1
	;;
esac

case " $use64bits $use64bitint $use64bitall " in
*" $define "*|*" true "*|*" [yY] "*)
	cat >&4 <<EOM
IRIX `uname -r` does not support 64-bit types.
You should upgrade to at least IRIX 6.2.
Cannot continue, aborting.
EOM
	exit 1
esac

