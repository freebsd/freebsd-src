# hints/aix.sh
# AIX 3.x.x hints thanks to Wayne Scott <wscott@ichips.intel.com>
# AIX 4.1 hints thanks to Christopher Chan-Nui <channui@austin.ibm.com>.
# AIX 4.1 pthreading by Christopher Chan-Nui <channui@austin.ibm.com> and
#	  Jarkko Hietaniemi <jhi@iki.fi>.
# AIX 4.3.x LP64 build by Steven Hirsch <hirschs@btv.ibm.com>
# Merged on Mon Feb  6 10:22:35 EST 1995 by
#   Andy Dougherty  <doughera@lafcol.lafayette.edu>

#
# Contact dfavor@corridor.com for any of the following:
#
#    - AIX 43x and above support
#    - gcc + threads support
#    - socks support
#
# Apr 99 changes:
#
#    - use nm in AIX 43x and above
#    - gcc + threads now builds
#    [(added support for socks) Jul 99 SOCKS support rewritten]
#
# Notes:
#
#    - shared libperl support is tricky. if ever libperl.a ends up
#      in /usr/local/lib/* it can override any subsequent builds of
#      that same perl release. to make sure you know where the shared
#      libperl.a is coming from do a 'dump -Hv perl' and check all the
#      library search paths in the loader header.
#
#      it would be nice to warn the user if a libperl.a exists that is
#      going to override the current build, but that would be complex.
#
#      better yet, a solid fix for this situation should be developed.
#

# Configure finds setrgid and setruid, but they're useless.  The man
# pages state:
#    setrgid: The EPERM error code is always returned.
#    setruid: The EPERM error code is always returned. Processes cannot
#	      reset only their real user IDs.
d_setrgid='undef'
d_setruid='undef'

alignbytes=8

case "$usemymalloc" in
'')  usemymalloc='n' ;;
esac

# Intuiting the existence of system calls under AIX is difficult,
# at best; the safest technique is to find them empirically.

# AIX 4.3.* and above default to using nm for symbol extraction
case "$osvers" in
   3.*|4.1.*|4.2.*)
      usenm='undef'
      ;;
   *)
      usenm='true'
      ;;
esac

so="a"
# AIX itself uses .o (libc.o) but we prefer compatibility
# with the rest of the world and with rest of the scripting
# languages (Tcl, Python) and related systems (SWIG).
# Stephanie Beals <bealzy@us.ibm.com>
dlext="so"

# Take possible hint from the environment.  If 32-bit is set in the
# environment, we can override it later.  If set for 64, the
# 'sizeof' test sees a native 64-bit architecture and never looks back.
case "$OBJECT_MODE" in
32)
    cat >&4 <<EOF

You have OBJECT_MODE=32 set in the environment. 
I take this as a hint you do not want to
build for a 64-bit address space. You will be
given the opportunity to change this later.
EOF
    ;;
64)
    cat >&4 <<EOF

You have OBJECT_MODE=64 set in the environment. 
This forces a full 64-bit build.  If that is
not what you intended, please terminate this
program, unset it and restart.
EOF
    ;;
*)  ;;
esac

# Trying to set this breaks the POSIX.c compilation

# Make setsockopt work correctly.  See man page.
# ccflags='-D_BSD=44'

# uname -m output is too specific and not appropriate here
case "$archname" in
'') archname="$osname" ;;
esac

cc=${cc:-cc}

case "$osvers" in
3*) d_fchmod=undef
    ccflags="$ccflags -D_ALL_SOURCE"
    ;;
*)  # These hints at least work for 4.x, possibly other systems too.
    ccflags="$ccflags -D_ALL_SOURCE -D_ANSI_C_SOURCE -D_POSIX_SOURCE"
    case "$cc" in
     *gcc*) ;;
     *) ccflags="$ccflags -qmaxmem=16384" ;;
    esac
    nm_opt='-B'
    ;;
esac

# These functions don't work like Perl expects them to.
d_setregid='undef'
d_setreuid='undef'

# Changes for dynamic linking by Wayne Scott <wscott@ichips.intel.com>
#
# Tell perl which symbols to export for dynamic linking.
case "$cc" in
*gcc*) ccdlflags='-Xlinker' ;;
esac
# the required -bE:$installarchlib/CORE/perl.exp is added by
# libperl.U (Configure) later.

case "$ldlibpthname" in
'') ldlibpthname=LIBPATH ;;
esac

# The first 3 options would not be needed if dynamic libs. could be linked
# with the compiler instead of ld.
# -bI:$(PERL_INC)/perl.exp  Read the exported symbols from the perl binary
# -bE:$(BASEEXT).exp	    Export these symbols.  This file contains only one
#			    symbol: boot_$(EXP)	 can it be auto-generated?
case "$osvers" in
3*) 
    lddlflags="$lddlflags -H512 -T512 -bhalt:4 -bM:SRE -bI:\$(PERL_INC)/perl.exp -bE:\$(BASEEXT).exp -e _nostart -lc"
    ;;
*) 
    lddlflags="$lddlflags -bhalt:4 -bM:SRE -bI:\$(PERL_INC)/perl.exp -bE:\$(BASEEXT).exp -b noentry -lc"
    ;;
esac

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
	ccflags="$ccflags -DNEED_PTHREAD_INIT"
	case "$cc" in
	gcc) ;;
	cc_r) ;;
	cc|xl[cC]_r) 
	    echo >&4 "Switching cc to cc_r because of POSIX threads."
	    # xlc_r has been known to produce buggy code in AIX 4.3.2.
	    # (e.g. pragma/overload core dumps)	 Let's suspect xlC_r, too.
	    # --jhi@iki.fi
	    cc=cc_r
	    ;;
	'') 
	    cc=cc_r
	    ;;
	*)
	    cat >&4 <<EOM
For pthreads you should use the AIX C compiler cc_r.
(now your compiler was set to '$cc')
Cannot continue, aborting.
EOM
	    exit 1
	    ;;
	esac

	# c_rify libswanted.
	set `echo X "$libswanted "| sed -e 's/ \([cC]\) / \1_r /g'`
	shift
	libswanted="$*"
	# c_rify lddlflags.
	set `echo X "$lddlflags "| sed -e 's/ \(-l[cC]\) / \1_r /g'`
	shift
	lddlflags="$*"

	# Insert pthreads to libswanted, before any libc or libC.
	set `echo X "$libswanted "| sed -e 's/ \([cC]\) / pthreads \1 /'`
	shift
	libswanted="$*"
	# Insert pthreads to lddlflags, before any libc or libC.
	set `echo X "$lddlflags " | sed -e 's/ \(-l[cC]\) / -lpthreads \1 /'`
	shift
	lddlflags="$*"

	;;
esac
EOCBU

# This script UU/uselargefiles.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use large files.
cat > UU/uselargefiles.cbu <<'EOCBU'
case "$uselargefiles" in
''|$define|true|[yY]*)
	lfcflags="`getconf XBS5_ILP32_OFFBIG_CFLAGS 2>/dev/null`"
	lfldflags="`getconf XBS5_ILP32_OFFBIG_LDFLAGS 2>/dev/null`"
	# _Somehow_ in AIX 4.3.1.0 the above getconf call manages to
	# insert(?) *something* to $ldflags so that later (in Configure) evaluating
	# $ldflags causes a newline after the '-b64' (the result of the getconf).
	# (nothing strange shows up in $ldflags even in hexdump;
	#  so it may be something in the shell, instead?)
	# Try it out: just uncomment the below line and rerun Configure:
# echo >&4 "AIX 4.3.1.0 $lfldflags mystery" ; exit 1
	# Just don't ask me how AIX does it, I spent hours wondering.
	# Therefore the line re-evaluating lfldflags: it seems to fix
	# the whatever it was that AIX managed to break. --jhi
	lfldflags="`echo $lfldflags`"
	lflibs="`getconf XBS5_ILP32_OFFBIG_LIBS 2>/dev/null|sed -e 's@^-l@@' -e 's@ -l@ @g`"
	case "$lfcflags$lfldflags$lflibs" in
	'');;
	*) ccflags="$ccflags $lfcflags"
	   ldflags="$ldflags $lfldflags"
	   libswanted="$libswanted $lflibs"
	   ;;
	esac
	lfcflags=''
	lfldflags=''
	lflibs=''
	;;
esac
EOCBU

# This script UU/use64bitint.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use 64 bit integers.
cat > UU/use64bitint.cbu <<'EOCBU'
case "$use64bitint" in
$define|true|[yY]*)
	    case "`oslevel`" in
	    3.*|4.[012].*)
		cat >&4 <<EOM
AIX `oslevel` does not support 64-bit interfaces.
You should upgrade to at least AIX 4.3.
EOM
		exit 1
		;;
	    esac
	    ;;
esac
EOCBU

cat > UU/use64bitall.cbu <<'EOCBU'
# This script UU/use64bitall.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to be maximally 64-bitty.
case "$use64bitall" in
$define|true|[yY]*)
	    case "`oslevel`" in
	    3.*|4.[012].*)
		cat >&4 <<EOM
AIX `oslevel` does not support 64-bit interfaces.
You should upgrade to at least AIX 4.3.
EOM
		exit 1
		;;
	    esac
	    echo " "
	    echo "Checking the CPU width of your hardware..." >&4
	    $cat >size.c <<EOCP
#include <stdio.h>
#include <sys/systemcfg.h>
int main (void)
{
  printf("%d\n",_system_configuration.width);
  return(0);
}
EOCP
	    set size
	    if eval $compile_ok; then
		lfcpuwidth=`./size`
		echo "You are running on $lfcpuwidth bit hardware."
	    else
		dflt="32"
		echo " "
		echo "(I can't seem to compile the test program.  Guessing...)"
		rp="What is the width of your CPU (in bits)?"
		. ./myread
		lfcpuwidth="$ans"
	    fi
	    $rm -f size.c size
	    case "$lfcpuwidth" in
	    32*)
		cat >&4 <<EOM
Bzzzt! At present, you can only perform a
full 64-bit build on a 64-bit machine.
EOM
		exit 1
		;;
	    esac
	    lfcflags="`getconf XBS5_LP64_OFF64_CFLAGS 2>/dev/null`"
	    lfldflags="`getconf XBS5_LP64_OFF64_LDFLAGS 2>/dev/null`"
	    # See jhi's comments above regarding this re-eval.  I've
	    # seen similar weirdness in the form of:
	    #
# 1506-173 (W) Option lm is not valid.  Enter xlc for list of valid options.
	    #
	    # error messages from 'cc -E' invocation. Again, the offending
	    # string is simply not detectable by any means.  Since it doesn't
	    # do any harm, I didn't pursue it. -- sh
	    lfldflags="`echo $lfldflags`"
	    lflibs="`getconf XBS5_LP64_OFF64_LIBS 2>/dev/null|sed -e 's@^-l@@' -e 's@ -l@ @g`"
	    # -q32 and -b32 may have been set by uselargefiles or user.
    	    # Remove them.
	    ccflags="`echo $ccflags | sed -e 's@-q32@@'`"
	    ldflags="`echo $ldflags | sed -e 's@-b32@@'`"
	    # Tell archiver to use large format.  Unless we remove 'ar'
	    # from 'trylist', the Configure script will just reset it to 'ar'
	    # immediately prior to writing config.sh.  This took me hours
	    # to figure out.
	    trylist="`echo $trylist | sed -e 's@^ar @@' -e 's@ ar @ @g' -e 's@ ar$@@'`"
	    ar="ar -X64"
	    nm_opt="-X64 $nm_opt"
	    # Note: Placing the 'lfcflags' variable into the 'ldflags' string
	    # is NOT a typo.  ldlflags is passed to the C compiler for final
	    # linking, and it wants -q64 (-b64 is for ld only!).
	    case "$lfcflags$lfldflags$lflibs" in
	    '');;
	    *) ccflags="$ccflags $lfcflags"
	       ldflags="$ldflags $lfcflags"
	       lddlflags="$lfldflags $lddlflags"
	       libswanted="$libswanted $lflibs"
	       ;;
	    esac
	    case "$ccflags" in
	    *-DUSE_64_BIT_ALL*) ;;
      	    *) ccflags="$ccflags -DUSE_64_BIT_ALL";;
	    esac
	    case "$archname64" in
	    ''|64*) archname64=64all ;;
	    esac
	    longsize="8"
	    # Don't try backwards compatibility
	    bincompat="$undef"
	    d_bincompat5005="$undef"
	    lfcflags=''
	    lfldflags=''
	    lflibs=''
	    lfcpuwidth=''
	    ;;
esac
EOCBU

cat > UU/uselongdouble.cbu <<'EOCBU'
# This script UU/uselongdouble.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use long doubles.
case "$uselongdouble" in
$define|true|[yY]*)
	ccflags="$ccflags -qlongdouble"
	# The explicit cc128, xlc128, xlC128 are not needed,
	# the -qlongdouble should do the trick. --jhi
	d_Gconvert='sprintf((b),"%.*llg",(n),(x))'
	;;
esac
EOCBU

# If the C++ libraries, libC and libC_r, are available we will prefer them
# over the vanilla libc, because the libC contain loadAndInit() and
# terminateAndUnload() which work correctly with C++ statics while libc
# load() and unload() do not.  See ext/DynaLoader/dl_aix.xs.
# The C-to-C_r switch is done by usethreads.cbu, if needed.
if test -f /lib/libC.a -a X"`$cc -v 2>&1 | grep gcc`" = X; then
    # Cify libswanted.
    set `echo X "$libswanted "| sed -e 's/ c / C c /'`
    shift
    libswanted="$*"
    # Cify lddlflags.
    set `echo X "$lddlflags "| sed -e 's/ -lc / -lC -lc /'`
    shift
    lddlflags="$*"
fi

# EOF
