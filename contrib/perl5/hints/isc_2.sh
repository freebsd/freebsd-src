#  isc_2.sh
#  Interactive Unix Version 2.2
#  Compile perl entirely in posix mode. 
#  Andy Dougherty		doughera@lafcol.lafayette.edu
#  Wed Oct  5 15:57:37 EDT 1994
#
# Use Configure -Dcc=gcc to use gcc
#
set `echo X "$libswanted "| sed -e 's/ c / /'`
shift
libswanted="$*"
case "$cc" in
*gcc*)	ccflags="$ccflags -posix"
	ldflags="$ldflags -posix"
	;;
*)	ccflags="$ccflags -Xp -D_POSIX_SOURCE"
	ldflags="$ldflags -Xp"
    	;;
esac
# Compensate for conflicts in <net/errno.h>
doio_cflags='ccflags="$ccflags -DENOTSOCK=103"'
pp_sys_cflags='ccflags="$ccflags -DENOTSOCK=103"'

# for ext/IPC/SysV/SysV.xs
ccflags="$ccflags -DPERL_ISC"
