# hints/irix_6.sh
#
# original from Krishna Sethuraman, krishna@sgi.com
#
# Modified Mon Jul 22 14:52:25 EDT 1996
# 	Andy Dougherty <doughera@lafcol.lafayette.edu>
# 	with help from Dean Roehrich <roehrich@cray.com>.
#   cc -n32 update info from Krishna Sethuraman, krishna@sgi.com.
#       additional update from Scott Henry, scotth@sgi.com

# Futzed with by John Stoffel <jfs@fluent.com> on 4/24/1997
#    - assumes 'cc -n32' by default
#    - tries to check for various compiler versions and do the right 
#      thing when it can
#    - warnings turned off (-n32 messages):
#       1116 - non-void function should return a value
#       1048 - cast between pointer-to-object and pointer-to-function
#       1042 - operand types are incompatible

# Tweaked by Chip Salzenberg <chip@perl.com> on 5/13/97
#    - don't assume 'cc -n32' if the n32 libm.so is missing

# Threaded by Jarkko Hietaniemi <jhi@iki.fi> on 11/18/97
#    - POSIX threads knowledge by IRIX version

# gcc-enabled by Kurt Starsinic <kstar@isinet.com> on 3/24/1998

# Use   sh Configure -Dcc='cc -n32' to try compiling with -n32.
#     or -Dcc='cc -n32 -mips3' (or -mips4) to force (non)portability
# Don't bother with -n32 unless you have the 7.1 or later compilers.
#     But there's no quick and light-weight way to check in 6.2.

# Let's assume we want to use 'cc -n32' by default, unless the
# necessary libm is missing (which has happened at least twice)
case "$cc" in
'')
    if test -f /usr/lib32/libm.so
    then
	cc='cc -n32'
    fi ;;
esac

# Check for which compiler we're using

case "$cc" in
*"cc -n32"*)

	# Perl 5.004_57 introduced new qsort code into pp_ctl.c that
	# makes IRIX  cc prior to 7.2.1 to emit bad code.
	# so some serious hackery follows to set pp_ctl flags correctly.

	# Check for which version of the compiler we're running
	case "`$cc -version 2>&1`" in
	*7.0*)                        # Mongoose 7.0
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1042,1048,1110,1116,1184 -OPT:Olimit=0"
	     optimize='none'	  
	     ;;
	*7.1*|*7.2|*7.20)             # Mongoose 7.1+
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1184 -OPT:Olimit=0"
	     optimize='-O3'	  
# This is a temporary fix for 5.005.
# Leave pp_ctl_cflags  line at left margin for Configure.  See 
# hints/README.hints, especially the section 
# =head2 Propagating variables to config.sh
pp_ctl_cflags='optimize=-O'
	     ;;
	*7.*)                         # Mongoose 7.2.1+
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1184 -OPT:Olimit=0:space=on"
	     optimize='-O3'	  
	     ;;
	*6.2*)                        # Ragnarok 6.2
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1184"
	     optimize='none'	  
	     ;;
	*)                            # Be safe and not optimize
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1184 -OPT:Olimit=0"
	     optimize='none'
	     ;;
	esac

	ld=ld
	# perl's malloc can return improperly aligned buffer
	usemymalloc='undef'
	# NOTE: -L/usr/lib32 -L/lib32 are automatically selected by the linker
	ldflags=' -L/usr/local/lib32 -L/usr/local/lib'
	cccdlflags=' '
    # From: David Billinghurst <David.Billinghurst@riotinto.com.au>
    # If you get complaints about so_locations then change the following
    # line to something like:
    #	lddlflags="-n32 -shared -check_registry /usr/lib32/so_locations"
	lddlflags="-n32 -shared"
	libc='/usr/lib32/libc.so'
	plibpth='/usr/lib32 /lib32 /usr/ccs/lib'
	nm_opt='-p'
	nm_so_opt='-p'
	;;
*gcc*)
	ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -D_POSIX_C_SOURCE"
	optimize="-O3"
	usenm='undef'
	;;
*)
	# this is needed to force the old-32 paths
	#  since the system default can be changed.
	ccflags="$ccflags -32 -D_BSD_TYPES -D_BSD_TIME -Olimit 3100"
	optimize='-O'	  
	;;
esac

# We don't want these libraries.
# Socket networking is in libc, these are not installed by default,
# and just slow perl down. (scotth@sgi.com)
set `echo X "$libswanted "|sed -e 's/ socket / /' -e 's/ nsl / /' -e 's/ dl / /'`
shift
libswanted="$*"

# I have conflicting reports about the sun, crypt, bsd, and PW
# libraries on Irix 6.2.
#
# One user rerports:
# Don't need sun crypt bsd PW under 6.2.  You *may* need to link
# with these if you want to run perl built under 6.2 on a 5.3 machine
# (I haven't checked)
#
# Another user reported that if he included those libraries, a large number
# of the tests failed (approx. 20-25) and he would get a core dump. To
# make things worse, test results were inconsistent, i.e., some of the
# tests would pass some times and fail at other times.
# The safest thing to do seems to be to eliminate them.
#
#  Actually, the only libs that you want are '-lm'.  Everything else
# you need is in libc.  You do also need '-lbsd' if you choose not
# to use the -D_BSD_* defines.  Note that as of 6.2 the only
# difference between '-lmalloc' and '-lc' malloc is the debugging
# and control calls, which aren't used by perl. -- scotth@sgi.com

set `echo X "$libswanted "|sed -e 's/ sun / /' -e 's/ crypt / /' -e 's/ bsd / /' -e 's/ PW / /' -e 's/ malloc / /'`
shift
libswanted="$*"

if [ "X$usethreads" = "X$define" -o "X$usethreads" = "Xy" ]; then
    if test ! -f /usr/include/pthread.h -o ! -f /usr/lib/libpthread.so; then
	uname_r=`uname -r`
	case "`uname -r`" in
	5*|6.0|6.1)
	    echo >&4 "IRIX $uname_r does not have the POSIX threads."
	    echo >&4 "You should upgrade to at least IRIX 6.2 with pthread patches."
	    echo >&4 "Cannot continue, aborting."
	    exit 1
	    ;;
	6.2)
	    echo >&4 ""
cat >&4 <<EOF
IRIX 6.2 $uname_r can have the POSIX threads.
The following IRIX patches (or their replacements) must, however, be installed:

        1404 Irix 6.2 Posix 1003.1b man pages
        1645 IRIX 6.2 & 6.3 POSIX header file updates
        2000 Irix 6.2 Posix 1003.1b support modules
        2254 Pthread library fixes
	2401 6.2 all platform kernel rollup
IMPORTANT:
	Without patch 2401, a kernel bug in IRIX 6.2 will
	cause your machine to panic and crash when running
	threaded perl. IRIX 6.3 and up should be OK.


Cannot continue, aborting.
EOF
	    exit 1
	    ;;
	6.*|7.*)
	    echo >&4 "IRIX $uname_r should have the POSIX threads."
	    echo >&4 "But somehow you do not seem to have them installed."
	    echo >&4 "Cannot continue, aborting."
	    exit 1
	    ;;
	esac
	unset uname_r
    fi
    # -lpthread needs to come before -lc but after other libraries such
    # as -lgdbm and such like. We assume here that -lc is present in
    # libswanted. If that fails to be true in future, then this can be
    # changed to add pthread to the very end of libswanted.
    set `echo X "$libswanted "| sed -e 's/ c / pthread /'`
    ld="${cc:-cc}"
    shift
    libswanted="$*"
    usemymalloc='n'
fi
