# hints/dec_osf.sh

#	* If you want to debug perl or want to send a
#	stack trace for inclusion into an bug report, call
#	Configure with the additional argument  -Doptimize=-g2
#	or uncomment this assignment to "optimize":
#
#optimize=-g2
#
#	If you want both to optimise and debug with the DEC cc
#	you must have -g3, e.g. "-O4 -g3", and (re)run Configure.
#
#	* gcc can always have both -g and optimisation on.
#
#	* debugging optimised code, no matter what compiler
#	one is using, can be surprising and confusing because of
#	the optimisation tricks like code motion, code removal,
#	loop unrolling, and inlining. The source code and the
#	executable code simply do not agree any more while in
#	mid-execution, the optimiser only cares about the results.
#
#	* Configure will automatically add the often quoted
#	-DDEBUGGING for you if the -g is specified.
#
#	* There is even more optimisation available in the new
#	(GEM) DEC cc: -O5 and -fast. "man cc" will tell more about them.
#	The jury is still out whether either or neither help for Perl
#	and how much. Based on very quick testing, -fast boosts
#	raw data copy by about 5-15% (-fast brings in, among other
#	things, inlined, ahem, fast memcpy()), while on the other
#	hand searching things (index, m//, s///), seems to get slower.
#	Your mileage will vary.
#
#	* The -std is needed because the following compiled
#	without the -std and linked with -lm
#
#	#include <math.h>
#	#include <stdio.h>
#	int main(){short x=10,y=sqrt(x);printf("%d\n",y);}
#
#	will in Digital UNIX 3.* and 4.0b print 0 -- and in Digital
#	UNIX 4.0{,a} dump core: Floating point exception in the printf(),
#	the y has become a signaling NaN.
#
#	* Compilation warnings like:
#
#	"Undefined the ANSI standard macro ..."
#
#	can be ignored, at least while compiling the POSIX extension
#	and especially if using the sfio (the latter is not a standard
#	part of Perl, never mind if it says little to you).
#

# If using the DEC compiler we must find out the DEC compiler style:
# the style changed between Digital UNIX (aka DEC OSF/1) 3 and
# Digital UNIX 4. The old compiler was originally from Ultrix and
# the MIPS company, the new compiler is originally from the VAX world
# and it is called GEM. Many of the options we are going to use depend
# on the compiler style.

# do NOT, I repeat, *NOT* take away those leading tabs
	# reset
	_DEC_uname_r=
	_DEC_cc_style=
	# set
	_DEC_uname_r=`uname -r`
	# _DEC_cc_style set soon below
# Configure Black Magic (TM)

case "$cc" in
*gcc*)	;; # pass
*)	# compile something small: taint.c is fine for this.
    	# the main point is the '-v' flag of 'cc'.
       	case "`cc -v -I. -c taint.c -o /tmp/taint$$.o 2>&1`" in
	*/gemc_cc*)	# we have the new DEC GEM CC
			_DEC_cc_style=new
			;;
	*)		# we have the old MIPS CC
			_DEC_cc_style=old
			;;
	esac
	# cleanup
	rm -f /tmp/taint$$.o
	;;
esac

# be nauseatingly ANSI
case "$cc" in
*gcc*)	ccflags="$ccflags -ansi"
	;;
*)	ccflags="$ccflags -std"
	;;
esac

# for gcc the Configure knows about the -fpic:
# position-independent code for dynamic loading

# we want optimisation

case "$optimize" in
'')	case "$cc" in 
	*gcc*)	
		optimize='-O3'				;;
	*)	case "$_DEC_cc_style" in
		new)	optimize='-O4'
			ccflags="$ccflags -fprm d -ieee"
			;;
		old)	optimize='-O2 -Olimit 3200'	;;
	    	esac
		ccflags="$ccflags -D_INTRINSICS"
		;;
	esac
	;;
esac

# Make glibpth agree with the compiler suite.  Note that /shlib
# is not here.  That's on purpose.  Even though that's where libc
# really lives from V4.0 on, the linker (and /sbin/loader) won't
# look there by default.  The sharable /sbin utilities were all
# built with "-Wl,-rpath,/shlib" to get around that.  This makes
# no attempt to figure out the additional location(s) searched by
# gcc, since not all versions of gcc are easily coerced into
# revealing that information.
glibpth="/usr/shlib /usr/ccs/lib /usr/lib/cmplrs/cc"
glibpth="$glibpth /usr/lib /usr/local/lib /var/shlib"

# dlopen() is in libc
libswanted="`echo $libswanted | sed -e 's/ dl / /'`"

# libPW contains nothing useful for perl
libswanted="`echo $libswanted | sed -e 's/ PW / /'`"

# libnet contains nothing useful for perl here, and doesn't work
libswanted="`echo $libswanted | sed -e 's/ net / /'`"

# libbsd contains nothing used by perl that is not already in libc
libswanted="`echo $libswanted | sed -e 's/ bsd / /'`"

# libc need not be separately listed
libswanted="`echo $libswanted | sed -e 's/ c / /'`"

# ndbm is already in libc
libswanted="`echo $libswanted | sed -e 's/ ndbm / /'`"

# the basic lddlflags used always
lddlflags='-shared -expect_unresolved "*"'

# Fancy compiler suites use optimising linker as well as compiler.
# <spider@Orb.Nashua.NH.US>
case "$_DEC_uname_r" in
*[123].*)	# old loader
		lddlflags="$lddlflags -O3"
		;;
*)		lddlflags="$lddlflags $optimize -msym"
		# -msym: If using a sufficiently recent /sbin/loader,
		# keep the module symbols with the modules.
		;;
esac
# Yes, the above loses if gcc does not use the system linker.
# If that happens, let me know about it. <jhi@iki.fi>


# If debugging or (old systems and doing shared)
# then do not strip the lib, otherwise, strip.
# As noted above the -DDEBUGGING is added automagically by Configure if -g.
case "$optimize" in
	*-g*) ;; # left intentionally blank
*)	case "$_DEC_uname_r" in
	*[123].*)
		case "$useshrplib" in
		false|undef|'')	lddlflags="$lddlflags -s"	;;
		esac
		;;
        *) lddlflags="$lddlflags -s"
	        ;;
    	esac
    	;;
esac

#
# Make embedding in things like INN and Apache more memory friendly.
# Keep it overridable on the Configure command line, though, so that
# "-Uuseshrplib" prevents this default.
#

case "$_DEC_cc_style.$useshrplib" in
	new.)	useshrplib="$define"	;;
esac

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
        # Threads interfaces changed with V4.0.
        case "`uname -r`" in
        *[123].*)
	    libswanted="$libswanted pthreads mach exc c_r"
  	    ccflags="-threads $ccflags"
	    ;;
        *)
	    libswanted="$libswanted pthread exc"
    	    ccflags="-pthread $ccflags"
	    ;;
        esac

        usemymalloc='n'
	;;
esac
EOCBU

#
# Unset temporary variables no more needed.
#

unset _DEC_cc_style
unset _DEC_uname_r
    
#
# History:
#
# perl5.004_57:
#
#	19-Dec-1997 Spider Boardman <spider@Orb.Nashua.NH.US>
#
#	* Newer Digital UNIX compilers enforce signaling for NaN without
#	  -ieee.  Added -fprm d at the same time since it's friendlier for
#	  embedding.
#
#	* Fixed the library search path to match cc, ld, and /sbin/loader.
#
#	* Default to building -Duseshrplib on newer systems.  -Uuseshrplib
#	  still overrides.
#
#	* Fix -pthread additions for useshrplib.  ld has no -pthread option.
#
#
# perl5.004_04:
#
#       19-Sep-1997 Spider Boardman <spider@Orb.Nashua.NH.US>
#
#	* libnet on Digital UNIX is for JAVA, not for sockets.
#
#
# perl5.003_28:
#
#       22-Feb-1997 Jarkko Hietaniemi <jhi@iki.fi>
#
#	* Restructuring Spider's suggestions.
#
#	* Older Digital UNIXes cannot handle -Olimit ... for $lddlflags.
#	
#	* ld -s cannot be used in older Digital UNIXes when doing shared.
#
#
#       21-Feb-1997 Spider Boardman <spider@Orb.Nashua.NH.US>
#
#	* -hidden removed.
#	
#	* -DSTANDARD_C removed.
#
#	* -D_INTRINSICS added. (that -fast does not seem to buy much confirmed)
#
#	* odbm not in libc, only ndbm. Therefore dbm back to $libswanted.
#
#	* -msym for the newer runtime loaders.
#
#	* $optimize also in $lddflags.
#
#
# perl5.003_27:
#
#	18-Feb-1997 Jarkko Hietaniemi <jhi@iki.fi>
#
#	* unset _DEC_cc_style and more commentary on -std.
#
#
# perl5.003_26:
#
#	15-Feb-1997 Jarkko Hietaniemi <jhi@iki.fi>
#
#	* -std and -ansi.
#
#
# perl5.003_24:
#
#	30-Jan-1997 Jarkko Hietaniemi <jhi@iki.fi>
#
#	* Fixing the note on -DDEBUGGING.
#
#	* Note on -O5 -fast.
#
#
# perl5.003_23:
#
#	26-Jan-1997 Jarkko Hietaniemi <jhi@iki.fi>
#
#	* Notes on how to do both optimisation and debugging.
#
#
#	25-Jan-1997 Jarkko Hietaniemi <jhi@iki.fi>
#
#	* Remove unneeded libraries from $libswanted: PW, bsd, c, dbm
#
#	* Restructure the $lddlflags build.
#
#	* $optimize based on which compiler we have.
#
#
# perl5.003_22:
#
#	23-Jan-1997 Achim Bohnet <ach@rosat.mpe-garching.mpg.de>
#
#	* Added comments 'how to create a debugging version of perl'
#
#	* Fixed logic of this script to prevent stripping of shared
#         objects by the loader (see ld man page for -s) is debugging
#         is set via the -g switch.
#
#
#	21-Jan-1997 Achim Bohnet <ach@rosat.mpe-garching.mpg.de>
#
#	* now 'dl' is always removed from libswanted. Not only if
#	  optimize is an empty string.
#	 
#
#	17-Jan-1997 Achim Bohnet <ach@rosat.mpe-garching.mpg.de>
#
#	* Removed 'dl' from libswanted: When the FreePort binary
#	  translator for Sun binaries is installed Configure concludes
#	  that it should use libdl.x.yz.fpx.so :-(
#	  Because the dlopen, dlclose,... calls are in the
#	  C library it not necessary at all to check for the
#	  dl library.  Therefore dl is removed from libswanted.
#	
#
#	1-Jan-1997 Achim Bohnet <ach@rosat.mpe-garching.mpg.de>
#	
#	* Set -Olimit to 3200 because perl_yylex.c got too big
#	  for the optimizer.
#
