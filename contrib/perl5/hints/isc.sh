#  isc.sh
#  Interactive Unix Versions 3 and 4. 
#  Compile perl entirely in posix mode. 
#  Andy Dougherty		doughera@lafcol.lafayette.edu
#  Wed Oct  5 15:57:37 EDT 1994
#
# Use Configure -Dcc=gcc to use gcc
#

# We don't want to explicitly mention -lc (since we're using POSIX mode.)
# We also don't want -lx (the Xenix compatability libraries.) The only
# thing that it seems to pick up is chsize(), which has been reported to
# not work.  chsize() can also be implemented via fcntl() in perl (if you
# define -D_SYSV3).  We'll leave in -lPW since it's harmless.  Some
# extension might eventually need it for alloca, though perl doesn't use
# it. 

set `echo X "$libswanted "| sed -e 's/ c / /' -e 's/ x / /'`
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

# getsockname() and getpeername() return 256 for no good reason
ccflags="$ccflags -DBOGUS_GETNAME_RETURN=256"

# rename(2) can't rename long filenames
d_rename=undef

# for ext/IPC/SysV/SysV.xs
ccflags="$ccflags -DPERL_ISC"

# You can also include -D_SYSV3 to pick up "traditionally visible"
# symbols hidden by name-space pollution rules.  This raises some
# compilation "redefinition" warnings, but they appear harmless.
# ccflags="$ccflags -D_SYSV3"

