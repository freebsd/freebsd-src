# sco.sh 
# Courtesy of Joel Rosi-Schwartz <j.schwartz@agonet.it>

# Additional SCO version info from
# Peter Wolfe	<wolfe@teloseng.com>
# Last revised 
# Fri Jul 19 14:54:25 EDT 1996
# by Andy Dougherty  <doughera@lafcol.lafayette.edu>

# To use gcc, use     sh Configure -Dcc=gcc
# But gcc will *not* do dynamic laoding on 3.2.5,
# for that use        sh Configure -Dcc=icc
# See below for more details.

# figure out what SCO version we are. The output of uname -X is
# something like:
#	System = SCO_SV
#	Node = xxxxx
#	Release = 3.2v5.0.0
#	KernelID = 95/08/08
#	Machine = Pentium  
#	BusType = ISA
#	Serial = xxxxx
#	Users = 5-user
#	OEM# = 0
#	Origin# = 1
#	NumCPU = 1 
 
# Use /bin/uname (because Gnu may be first on the path and
# it does not support -X) to figure out what SCO version we are:
case `/bin/uname -X | egrep '^Release'` in
*3.2v4.*) scorls=3 ;;   # I don't know why this is 3 instead of 4 :-)
*3.2v5.*) scorls=5 ;;
*) scorls=3 ;; # this probabaly shouldn't happen
esac

# Try to use libintl.a since it has strcoll and strxfrm
libswanted="intl $libswanted"
# Try to use libdbm.nfs.a since it has dbmclose.
# 
if test -f /usr/lib/libdbm.nfs.a ; then
    libswanted=`echo "dbm.nfs $libswanted " | sed -e 's/ dbm / /'`
fi
set X $libswanted
shift
libswanted="$*"

# We don't want Xenix cross-development libraries
glibpth=`echo $glibpth | sed -e 's! /usr/lib/386 ! !' -e 's! /lib/386 ! !'`
xlibpth=''

case "$cc" in
*gcc*)	ccflags="$ccflags -U M_XENIX"
	optimize="$optimize -O2"
	;;
scocc)	;;

# On SCO 3.2v5 both cc and icc can build dynamic load, but cc core
# dumps if optimised, so I am only setting this up for icc.
# It is possible that some 3.2v4.2 system have icc, I seem to
# recall it was available as a seperate product but I have no
# knowledge if it can do dynamic loading and if so how.
#	Joel Rosi-Schwartz
icc)# Apparently, SCO's cc gives rather verbose warnings
	# Set -w0 to turn them off.
	case $scorls in
	3) ccflags="$ccflags -W0 -quiet -U M_XENIX" ;;
	5) ccflags="$ccflags -belf -w0 -U M_XENIX"
	   optimize="-O1" # -g -O1 will not work
	 # optimize="-O0" may be needed for pack test to pass.
       lddlflags='-G -L/usr/local/lib'
       ldflags=' -W l,-Bexport -L/usr/local/lib'
       dlext='so'
       dlsrc='dl_dlopen.xs'
       usedl='define'
	   ;;
	esac
	;;

*)	# Apparently, miniperl core dumps if -O is used.
	case "$optimize" in
	'') optimize=none ;;
	esac
	# Apparently, SCO's cc gives rather verbose warnings
	# Set -w0 to turn them off.
	case $scorls in
	3) ccflags="$ccflags -W0 -quiet -U M_XENIX" ;;
	5) ccflags="$ccflags -w0 -U M_XENIX -DPERL_SCO5" ;;
	esac
	;;
esac
i_varargs=undef

# I have received one report that nm extraction doesn't work if you're
# using the scocc compiler.  This system had the following 'myconfig'
# uname='xxx xxx 3.2 2 i386 '
# cc='scocc', optimize='-O'
usenm='false'

# If you want to use nm, you'll probably have to use nm -p.  The
# following does that for you:
nm_opt='-p'

# I have received one report that you can't include utime.h in
# pp_sys.c.  Uncomment the following line if that happens to you:
# i_utime=undef

# Apparently, some versions of SCO include both .so and .a libraries,
# but they don't mix as they do on other ELF systems.  The upshot is
# that Configure finds -ldl (libdl.so) but 'ld' complains it can't
# find libdl.a. 
# I don't know which systems have this feature, so I'll just remove
# -dl from libswanted for all SCO systems until someone can figure
# out how to get dynamic loading working on SCO.
#
# The output of uname -X on one such system was
#	System = SCO_SV
#	Node = xxxxx
#	Release = 3.2v5.0.0
#	KernelID = 95/08/08
#	Machine = Pentium  
#	BusType = ISA
#	Serial = xxxxx
#	Users = 5-user
#	OEM# = 0
#	Origin# = 1
#	NumCPU = 1 
#
# The 5.0.0 on the Release= line is probably the thing to watch.
#	Andy Dougherty <doughera@lafcol.lafayette.edu>
#	Thu Feb  1 15:06:56 EST 1996
libswanted=`echo " $libswanted " | sed -e 's/ dl / /'`
set X $libswanted
shift
libswanted="$*"

# Perl 5.003_05 and later try to include both <time.h> and <sys/select.h>
# in pp_sys.c, but that fails due to a redefinition of struct timeval.
# This will generate a WHOA THERE.  Accept the default.
i_sysselct=$undef
