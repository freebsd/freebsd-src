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

# 64-bitty by Jarkko Hietaniemi on 9/1998

# Use   sh Configure -Dcc='cc -n32' to try compiling with -n32.
#     or -Dcc='cc -n32 -mips3' (or -mips4) to force (non)portability
# Don't bother with -n32 unless you have the 7.1 or later compilers.
#     But there's no quick and light-weight way to check in 6.2.

# NOTE: some IRIX cc versions, e.g. 7.3.1.1m (try cc -version) have
# been known to have issues (coredumps) when compiling perl.c.
# If you've used -OPT:fast_io=ON and this happens, try removing it.
# If that fails, or you didn't use that, then try adjusting other
# optimization options (-LNO, -INLINE, -O3 to -O2, etcetera).
# The compiler bug has been reported to SGI.
# -- Allen Smith <easmith@beatrice.rutgers.edu>

# Let's assume we want to use 'cc -n32' by default, unless the
# necessary libm is missing (which has happened at least twice)
case "$cc" in
'') case "$use64bitall" in
    "$define"|true|[yY]*) test -f /usr/lib64/libm.so && cc='cc -64' ;;
    *) test -f /usr/lib32/libm.so && cc='cc -n32' ;;
    esac    	
esac

cc=${cc:-cc}

case "$cc" in
*gcc*) ;;
*) ccversion=`cc -version` ;;
esac

case "$use64bitint" in
$define|true|[yY]*)
	    case "`uname -r`" in
	    [1-5]*|6.[01])
		cat >&4 <<EOM
IRIX `uname -r` does not support 64-bit types.
You should upgrade to at least IRIX 6.2.
Cannot continue, aborting.
EOM
		exit 1
		;;
	    esac
	    ;;
esac

case "$use64bitall" in
"$define"|true|[yY]*)
  case "`uname -s`" in
  IRIX)
            cat >&4 <<EOM
You cannot use -Duse64bitall in 32-bit IRIX, sorry.

Cannot continue, aborting.
EOM
            exit 1
	;;
  esac
  ;;
esac

# Check for which compiler we're using

case "$cc" in
*"cc -n32"*)

	# If a library is requested to link against, make sure the
	# objects in the library are of the same ABI we are compiling
	# against. Albert Chin-A-Young <china@thewrittenword.com>
	libscheck='case "$xxx" in
*.a) /bin/ar p $xxx `/bin/ar t $xxx | /usr/bsd/head -1` >$$.o;
  case "`/usr/bin/file $$.o`" in
  *N32*) rm -f $$.o ;;
  *) rm -f $$.o; xxx=/no/n32$xxx ;;
  esac ;;
*) case "`/usr/bin/file $xxx`" in
  *N32*) ;;
  *) xxx=/no/n32$xxx ;;
  esac ;;
esac'

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
	;;
*"cc -64"*)

	loclibpth="$loclibpth /usr/lib64"
	libscheck='case "`/usr/bin/file $xxx`" in
*64-bit*) ;;
*) xxx=/no/64-bit$xxx ;;
esac'
	# NOTE: -L/usr/lib64 -L/lib64 are automatically selected by the linker
	ldflags=' -L/usr/local/lib64 -L/usr/local/lib'
	cccdlflags=' '
    # From: David Billinghurst <David.Billinghurst@riotinto.com.au>
    # If you get complaints about so_locations then change the following
    # line to something like:
    #	lddlflags="-64 -shared -check_registry /usr/lib64/so_locations"
	lddlflags="-64 -shared"
	libc='/usr/lib64/libc.so'
	plibpth='/usr/lib64 /lib64 /usr/ccs/lib'
	;;
*gcc*)
	ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -D_POSIX_C_SOURCE"
	optimize="-O3"
	usenm='undef'
	case "`uname -s`" in
	# Without the -mabi=64 gcc in 64-bit IRIX has problems passing
	# and returning small structures.  This affects inet_*() and semctl().
	# See http://reality.sgi.com/ariel/freeware/gcc-2.8.1-notes.html
	# for more information.  Reported by Lionel Cons <lionel.cons@cern.ch>.
	IRIX64)	ccflags="$ccflags -mabi=64"
		ldflags="$ldflags -mabi=64 -L/usr/lib64"
		lddlflags="$lddlflags -mabi=64"
		;;
	*)	ccflags="$ccflags -DIRIX32_SEMUN_BROKEN_BY_GCC"
		;;
	esac
	;;
*)
	# this is needed to force the old-32 paths
	#  since the system default can be changed.
	ccflags="$ccflags -32 -D_BSD_TYPES -D_BSD_TIME -Olimit 3100"
	optimize='-O'	  
	;;
esac

# Settings common to both native compiler modes.
case "$cc" in
*"cc -n32"*|*"cc -64"*)
	ld=$cc

	# perl's malloc can return improperly aligned buffer
	# which (under 5.6.0RC1) leads into really bizarre bus errors
	# and freak test failures (lib/safe1 #18, for example),
	# even more so with -Duse64bitall: for example lib/io_linenumtb.
	# fails under the harness but succeeds when run separately,
	# under make test pragma/warnings #98 fails, and lib/io_dir
	# apparently coredumps (the last two don't happen under
    	# the harness.  Helmut Jarausch is seeing bus errors from
        # miniperl, as was Scott Henry with snapshots from just before
	# the RC1. --jhi
	usemymalloc='undef'
#malloc_cflags='ccflags="-DSTRICT_ALIGNMENT $ccflags"'

	nm_opt='-p'
	nm_so_opt='-p'

	# Perl 5.004_57 introduced new qsort code into pp_ctl.c that
	# makes IRIX  cc prior to 7.2.1 to emit bad code.
	# so some serious hackery follows to set pp_ctl flags correctly.

	# Check for which version of the compiler we're running
	case "`$cc -version 2>&1`" in
	*7.0*)                        # Mongoose 7.0
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1042,1048,1110,1116,1174,1184,1552 -OPT:Olimit=0"
	     optimize='none'
	     ;;
	*7.1*|*7.2|*7.20)             # Mongoose 7.1+
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1174,1184,1552 -OPT:Olimit=0"
	     optimize='-O3'
# This is a temporary fix for 5.005.
# Leave pp_ctl_cflags  line at left margin for Configure.  See 
# hints/README.hints, especially the section 
# =head2 Propagating variables to config.sh
pp_ctl_cflags='optimize=-O'
	     ;;
	*7.*)                         # Mongoose 7.2.1+
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1174,1184,1552 -OPT:Olimit=0:space=ON"
	     optimize='-O3'
	     ;;
	*6.2*)                        # Ragnarok 6.2
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1174,1184,1552"
	     optimize='none'
	     ;;
	*)                            # Be safe and not optimize
	     ccflags="$ccflags -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1174,1184,1552 -OPT:Olimit=0"
	     optimize='none'
	     ;;
	esac

# this is to accommodate the 'modules' capability of the 
# 7.2 MIPSPro compilers, which allows for the compilers to be installed
# in a nondefault location.  Almost everything works as expected, but
# /usr/include isn't caught properly.  Hence see the /usr/include/pthread.h
# change below to include TOOLROOT (a modules environment variable),
# and the following code.  Additional
# code to accommodate the 'modules' environment should probably be added
# here if possible, or be inserted as a ${TOOLROOT} reference before
# absolute paths (again, see the pthread.h change below). 
# -- krishna@sgi.com, 8/23/98

	if [ "X${TOOLROOT}" != "X" ]; then
	# we cant set cppflags because it gets overwritten
	# we dont actually need $TOOLROOT/usr/include on the cc line cuz the 
	# modules functionality already includes it but
	# XXX - how do I change cppflags in the hints file?
		ccflags="$ccflags -I${TOOLROOT}/usr/include"
	usrinc="${TOOLROOT}/usr/include"
        fi

	;;
esac

# Don't groan about unused libraries.
ldflags="$ldflags -Wl,-woff,84"

# workaround for an optimizer bug
case "`$cc -version 2>&1`" in
*7.2.*)   op_cflags='optimize=-O1'; opmini_cflags='optimize=-O1' ;;
*7.3.1.*) op_cflags='optimize=-O2'; opmini_cflags='optimize=-O2' ;;
esac

# We don't want these libraries.
# Socket networking is in libc, these are not installed by default,
# and just slow perl down. (scotth@sgi.com)
set `echo X "$libswanted "|sed -e 's/ socket / /' -e 's/ nsl / /' -e 's/ dl / /'`
shift
libswanted="$*"

# Irix 6.5.6 seems to have a broken header <sys/mode.h>
# don't include that (it doesn't contain S_IFMT, S_IFREG, et al)

i_sysmode="$undef"

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

cat > UU/usethreads.cbu <<'EOCBU'
# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
case "$usethreads" in
$define|true|[yY]*)
        if test ! -f ${TOOLROOT}/usr/include/pthread.h -o ! -f /usr/lib/libpthread.so; then
            case "`uname -r`" in
            [1-5].*|6.[01])
 	        cat >&4 <<EOM
IRIX `uname -r` does not support POSIX threads.
You should upgrade to at least IRIX 6.2 with pthread patches.
EOM
	        ;;
	    6.2)
 	        cat >&4 <<EOM
IRIX 6.2 can have the POSIX threads.
However, the following IRIX patches (or their replacements) MUST be installed:
        1404 Irix 6.2 Posix 1003.1b man pages
        1645 IRIX 6.2 & 6.3 POSIX header file updates
        2000 Irix 6.2 Posix 1003.1b support modules
        2254 Pthread library fixes
	2401 6.2 all platform kernel rollup
IMPORTANT:
	Without patch 2401, a kernel bug in IRIX 6.2 will
	cause your machine to panic and crash when running
	threaded perl. IRIX 6.3 and up should be OK.
EOM
	        ;;
  	    [67].*)
	        cat >&4 <<EOM
IRIX `uname -r` should have the POSIX threads.
But, somehow, you do not seem to have them installed.
EOM
	        ;;
	    esac
            cat >&4 <<EOM
Cannot continue, aborting.
EOM
            exit 1
        fi
        set `echo X "$libswanted "| sed -e 's/ c / pthread /'`
        ld="${cc:-cc}"
        shift
        libswanted="$*"

        usemymalloc='n'
	;;
esac
EOCBU

# The -n32 makes off_t to be 8 bytes, so we should have largefileness.

