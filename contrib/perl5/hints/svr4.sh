# svr4 hints, System V Release 4.x
# Last modified 1996/10/25 by Tye McQueen, tye@metronet.com
# Merged 1998/04/23 with perl5.004_04 distribution by 
# Andy Dougherty <doughera@lafayette.edu>

# Use Configure -Dcc=gcc to use gcc.
case "$cc" in
'') cc='/bin/cc'
    test -f $cc || cc='/usr/ccs/bin/cc'
    ;;
esac

# We include support for using libraries in /usr/ucblib, but the setting
# of libswanted excludes some libraries found there.  If you run into
# problems, you may have to remove "ucb" from libswanted.  Just delete
# the comment '#' from the sed command below.
ldflags='-L/usr/ccs/lib -L/usr/ucblib'
ccflags='-I/usr/include -I/usr/ucbinclude'
# Don't use problematic libraries:
libswanted=`echo " $libswanted " | sed -e 's/ malloc / /'` # -e 's/ ucb / /'`
# libmalloc.a - Probably using Perl's malloc() anyway.
# libucb.a - Remove it if you have problems ld'ing.  We include it because
#   it is needed for ODBM_File and NDBM_File extensions.

if [ -r /usr/ucblib/libucb.a ]; then	# If using BSD-compat. library:
    d_Gconvert='gcvt((x),(n),(b))'	# Try gcvt() before gconvert().
    # Use the "native" counterparts, not the BSD emulation stuff:
    d_bcmp='undef' d_bcopy='undef' d_bzero='undef' d_safebcpy='undef'
    d_index='undef' d_killpg='undef' d_getprior='undef' d_setprior='undef'
    d_setlinebuf='undef' 
    # d_setregid='undef' d_setreuid='undef'  # ???
fi

# UnixWare has /usr/lib/libc.so.1, /usr/lib/libc.so.1.1, and
# /usr/ccs/lib/libc.so.  Configure chooses libc.so.1.1 while it
# appears that /usr/ccs/lib/libc.so contains more symbols:
#
# Try the following if you want to use nm-extraction.  We'll just
# skip the nm-extraction phase, since searching for all the different
# library versions will be hard to keep up-to-date.
#
# if [ "" = "$libc" -a -f /usr/ccs/lib/libc.so -a \
#   -f /usr/lib/libc.so.1 -a -f /usr/lib/libc.so.1.1 ]; then
#     if nm -h /usr/ccs/lib/libc.so | egrep '\<_?select$' >/dev/null; then
# 	if nm -h /usr/lib/libc.so.1 | egrep '\<_?select$'` >/dev/null ||
# 	   nm -h /usr/lib/libc.so.1.1 | egrep '\<_?select$'` >/dev/null; then
# 	    :
# 	else
# 	    libc=/usr/ccs/lib/libc.so
# 	fi
#     fi
# fi
#
#  Don't bother with nm.  Just compile & link a small C program.
case "$usenm" in
'') usenm=false;;
esac

# Broken C-Shell tests (Thanks to Tye McQueen):
# The OS-specific checks may be obsoleted by the this generic test.
	sh_cnt=`sh -c 'echo /*' | wc -c`
	csh_cnt=`csh -f -c 'glob /*' 2>/dev/null | wc -c`
	csh_cnt=`expr 1 + $csh_cnt`
if [ "$sh_cnt" -ne "$csh_cnt" ]; then
    echo "You're csh has a broken 'glob', disabling..." >&2
    d_csh='undef'
fi

# Unixware-specific problems.  The undocumented -X argument to uname 
# is probably a reasonable way of detecting UnixWare.  
# UnixWare has a broken csh.  (This might already be detected above).
# In Unixware 2.1.1 the fields in FILE* got renamed!
# Unixware 1.1 can't cast large floats to 32-bit ints.
# Configure can't detect memcpy or memset on Unixware 2 or 7
#
#    Leave leading tabs on the next two lines so Configure doesn't 
#    propagate these variables to config.sh
	uw_ver=`uname -v`
	uw_isuw=`uname -X 2>&1 | grep Release`

if [ "$uw_isuw" = "Release = 4.2" ]; then
   case $uw_ver in
   1.1)
      d_casti32='undef'
      ;;
   esac
fi
if [ "$uw_isuw" = "Release = 4.2MP" ]; then
   case $uw_ver in
   2.1)
	d_csh='undef'
	d_memcpy='define'
	d_memset='define'
	;;
   2.1.*)
	d_csh='undef'
	d_memcpy='define'
	d_memset='define'
	stdio_cnt='((fp)->__cnt)'
	d_stdio_cnt_lval='define'
	stdio_ptr='((fp)->__ptr)'
	d_stdio_ptr_lval='define'
	;;
   esac
fi
if [ "$uw_isuw" = "Release = 5" ]; then
   case $uw_ver in
   7)
	d_csh='undef'
	d_memcpy='define'
	d_memset='define'
	stdio_cnt='((fp)->__cnt)'
	d_stdio_cnt_lval='define'
	stdio_ptr='((fp)->__ptr)'
	d_stdio_ptr_lval='define'
	;;
   esac
fi
# End of Unixware-specific tests.

# DDE SMES Supermax Enterprise Server
case "`uname -sm`" in
"UNIX_SV SMES")
    # the *grent functions are in libgen.
    libswanted="$libswanted gen"
    # csh is broken (also) in SMES
    # This may already be detected by the generic test above.
    d_csh='undef'
    case "$cc" in
    *gcc*) ;;
    *)	# for cc we need -K PIC (not -K pic)
 	cccdlflags="$cccdlflags -K PIC"
	;;
    esac
    ;;
esac

# NCR MP-RAS.  Thanks to Doug Hendricks for this info.
# The output of uname -a looks like this
#	foo foo 4.0 3.0 3441 Pentium III(TM)-ISA/PCI
# Configure sets osname=svr4.0, osvers=3.0, archname='3441-svr4.0'
case "$myuname" in
*3441*)
    # With the NCR High Performance C Compiler R3.0c, miniperl fails 
    # t/op/regexp.t test 461 unless we compile with optimizie=-g.
    # The whole O/S is being phased out, so more detailed probing
    # is probably not warranted.
    case "$optimize" in 
    '') optimize='-g' ;;
    esac
    ;;
esac

# Configure may fail to find lstat() since it's a static/inline function
# in <sys/stat.h> on Unisys U6000 SVR4, UnixWare 2.x, and possibly other
# SVR4 derivatives.  (Though UnixWare has it in /usr/ccs/lib/libc.so.)
d_lstat=define

d_suidsafe='define'	# "./Configure -d" can't figure this out easilly

