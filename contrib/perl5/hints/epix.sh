# epix.sh
# Hint file for EP/IX on CDC RISC boxes.
#
# From: Stanley Donald Capelik <sd9sdc@hp100.den.mmc.com>
# Modified by Andy Dougherty   <doughera@lafcol.lafayette.edu>
# Last modified:  Mon May  8 15:29:18 EDT 1995
#
#  This hint file appears to be based on the svr4 hints for perl5.000,
#  with some CDC-specific additions.  I've tried to updated it to
#  match the 5.001 svr4 hints, which allow for dynamic loading,
#  but I have no way of testing the resulting file.
#
#  There were also some contradictions that I've tried to straighten
#  out, but I'm not sure I got them all right.
#
# Edit config.sh to change shmattype from 'char *' to 'void *'"

# Use Configure -Dcc=gcc to use gcc.
case "$cc" in
'') cc='/bin/cc3.11'
    test -f $cc || cc='/usr/ccs/bin/cc'
    ;;
esac

usrinc='/svr4/usr/include'

# Various things that Configure apparently doesn't get right.
strings='/svr4/usr/include/string.h'
timeincl='/svr4/usr/include/sys/time.h '
libc='/svr4/usr/lib/libc.a'
glibpth="/svr4/usr/lib /svr4/usr/lib/cmplrs/cc /usr/ccs/lib /svr4/lib /svr4/usr/ucblib $glibpth"
osname='epix2'
archname='epix2'
d_suidsafe='define'	# "./Configure -d" can't figure this out easilly
d_flock='undef'

# Old version had this, but I'm not sure why since the old version
# also mucked around with libswanted.  This is also definitely wrong
# if the user is trying to use DB_File or GDBM_File.
# libs='-lsocket -lnsl -ldbm -ldl -lc -lcrypt -lm -lucb'

# We include support for using libraries in /usr/ucblib, but the setting
# of libswanted excludes some libraries found there.  You may want to
# prevent "ucb" from being removed from libswanted and see if perl will
# build on your system.
ldflags='-non_shared -systype svr4 -L/svr4/usr/lib -L/svr4/usr/lib/cmplrs/cc -L/usr/ccs/lib -L/svr4/usr/ucblib'
ccflags='-systype svr4 -D__STDC__=0 -I/svr4/usr/include -I/svr4/usr/ucbinclude'
cppflags='-D__STDC__=0 -I/svr4/usr/include -I/svr4/usr/ucbinclude'

# Don't use problematic libraries:

libswanted=`echo " $libswanted " | sed -e 's/ malloc / /'` # -e 's/ ucb / /'`
# libmalloc.a - Probably using Perl's malloc() anyway.
# libucb.a - Remove it if you have problems ld'ing.  We include it because
#   it is needed for ODBM_File and NDBM_File extensions.
if [ -r /usr/ucblib/libucb.a ]; then	# If using BSD-compat. library:
    # Use the "native" counterparts, not the BSD emulation stuff:
    d_bcmp='undef'; d_bcopy='undef'; d_bzero='undef'; d_safebcpy='undef'
    d_index='undef'; d_killpg='undef'; d_getprior='undef'; d_setprior='undef'
    d_setlinebuf='undef'; d_setregid='undef'; d_setreuid='undef'
fi

lddlflags="-G $ldflags"	# Probably needed for dynamic loading
# We _do_ want the -L paths in ldflags, but we don't want the -non_shared.
lddlflags=`echo $lddlflags | sed 's/-non_shared//'`

cat <<'EOM' >&4

If you wish to use dynamic linking, you must use 
	LD_LIBRARY_PATH=`pwd`; export LD_LIBRARY_PATH
or
	setenv LD_LIBRARY_PATH `pwd`
before running make.

EOM
