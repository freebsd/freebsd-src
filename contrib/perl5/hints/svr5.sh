# svr5 hints, System V Release 5.x (UnixWare 7)
# mods after mail fm Andy Dougherty
# Reworked by hops@sco.com Sept/Oct 1999 for UW7.1 platform support 
#   Boyd Gerber, gerberb@zenez.com 1999/09/21 for threads support.
# Originally taken from svr4 hints.sh  21-Sep-98 hops@sco.com
# which was version of 1996/10/25 by Tye McQueen, tye@metronet.com

# Use Configure -Dusethreads to enable threads.
# Use Configure -Dcc=gcc to use gcc.
case "$cc" in
*gcc*)
    #  "$gccversion" not set yet
    vers=`gcc -v 2>&1 | sed -n -e 's@.*version \([^ ][^ ]*\) .*@\1@p'`
    case $vers in
    *2.95*)
         ccflags='-fno-strict-aliasing'
        # More optimisation provided in gcc-2.95 causes miniperl to segv.
        # -fno-strict-aliasing is supposed to correct this but 
        # if it doesn't and you get segv when the build runs miniperl then 
        # disable optimisation as below
        #  optimize=' '
        ;;
    esac
    ;;  
esac

# Hardwire the processor to 586 for consistancy with autoconf
# archname='i586-svr5'
#  -- seems this is generally disliked by perl porters so leave it to float

# Our default setup excludes anything from /usr/ucblib (and consequently dbm)
# as later modules assume symbols found are available in shared libs 
# On svr5 these are static archives which causes problems for
# dynamic modules loaded later (and ucblib is a bad dream anyway)
# 
# However there is a dbm library built from the ucb sources outside ucblib
# at http://www.sco.com/skunkware (installing into /usr/local) so if we
# detect this we'll use it. You can change the default
# (to allow ucblib and its dbm or disallowing non ucb dbm) by 
# changing 'want_*' config values below to '' to disable or otherwise to enable

#    Leave leading tabs so Configure doesn't propagate variables to config.sh

	want_ucb=''		# don't use anything from /usr/ucblib - icky
	want_dbm='yes'		# use dbm if can find library in /usr/local/lib
	want_gdbm='yes'		# use gdbm if can find library in /usr/local/lib
	want_udk70=''		# link with old static libc pieces
            # link with udk70 if building on 7.1 abd want resulting binary 
            # to run on uw7.0* - it will link in referenced static symbols 
            # of libc that are (now) in the shared libc.so on 7.1 but were 
            # not there in 7.0.
            # There are still scenarios where this is still insufficient so 
            # overall it is preferable to get ptf7051e 
            #   ftp://ftp.sco.com/SLS/ptf7051e.Z
            # installed on any/all 7.0 systems and leave the above unset.

if [ "$want_ucb" ] ; then 
    ldflags= '-L/usr/ucblib'
    ccflags='-I/usr/ucbinclude'
    # /usr/ccs/include and /usr/ccs/lib are used implicitly by cc as reqd
else
    libswanted=`echo " $libswanted " | sed -e 's/ ucb / /'`
    glibpth=`echo " $glibpth " | sed -e 's/ \/usr\/ucblib / /'`

    # If see libdbm in /usr/local and not overidden assume its the 
    # non ucblib rebuild from skunkware  and use it
    if [ ! -f /usr/local/lib/libdbm.so -o ! "$want_dbm" ] ; then
        i_dbm='undef'
        libswanted=`echo " $libswanted " | sed -e 's/ dbm / /'`
    fi
fi

if [ ! "$want_gdbm" ] ; then 
   i_gdbm='undef'
   libswanted=`echo " $libswanted " | sed -e 's/ gdbm / /'`
fi


# Don't use problematic libraries:
#   libmalloc.a - Probably using Perl's malloc() anyway.
#   libc:  on UW7 don't want -lc explicitly as native cc gives warnings/errors
libswanted=`echo " $libswanted " | sed -e 's/ malloc / /' -e 's/ c / /'`

# remove /shlib and /lib from library search path as both symlink to /usr/lib
# where runtime shared libc is 
glibpth=`echo " $glibpth " | sed -e 's/ \/shlib / /' -e 's/ \/lib / /`

# Don't use BSD emulation pieces (/usr/ucblib) regardless
# these would probably be autonondetected anyway but ...
d_Gconvert='gcvt((x),(n),(b))'	# Try gcvt() before gconvert().
d_bcopy='undef' d_bcmp='undef'  d_bzero='undef'  d_safebcpy='undef'
d_index='undef' d_killpg='undef' d_getprior='undef' d_setprior='undef'
d_setlinebuf='undef' 
d_setregid='undef' d_setreuid='undef'  # -- in /usr/lib/libc.so.1


# Broken C-Shell tests (Thanks to Tye McQueen):
# The OS-specific checks may be obsoleted by the this generic test.
	sh_cnt=`sh -c 'echo /*' | wc -c`
	csh_cnt=`csh -f -c 'glob /*' 2>/dev/null | wc -c`
	csh_cnt=`expr 1 + $csh_cnt`
if [ "$sh_cnt" -ne "$csh_cnt" ]; then
    echo "You're csh has a broken 'glob', disabling..." >&2
    d_csh='undef'
fi

# Unixware-specific problems.  UW7 give correctname with uname -s
# UnixWare has a broken csh.  (This might already be detected above).
# Configure can't detect memcpy or memset on Unixware 2 or 7
#
#    Leave leading tabs on the next two lines so Configure doesn't 
#    propagate these variables to config.sh
	uw_ver=`uname -v`
	uw_isuw=`uname -s 2>&1`

if [ "$uw_isuw" = "UnixWare" ]; then
   case $uw_ver in
   7.1*)
	d_csh='undef'
	d_memcpy='define'
	d_memset='define'
	stdio_cnt='((fp)->__cnt)'
	d_stdio_cnt_lval='define'
	stdio_ptr='((fp)->__ptr)'
	d_stdio_ptr_lval='define'

        d_bcopy='define'    # In /usr/lib/libc.so.1
        d_setregid='define' #  " 
        d_setreuid='define' #  " 

        if [ -f /usr/ccs/lib/libcudk70.a -a "$want_udk70" ] ; then
            libswanted=" $libswanted cudk70"
        fi
	;;
   7*)
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

###############################################################
# Dynamic loading section: Is default so it should just happen.
# set below to explicitly force.
# usedl='define'
# dlext='so'
# dlsrc='dl_dlopen.xs'
#
# ccdlflags : must tell the linker to export all global symbols
# cccdlflags: must tell the compiler to generate relocatable code
# lddlflags : must tell the linker to output a shared library

# use shared perl lib    
useshrplib='true'

case "$cc" in
       *gcc*)
           ccdlflags='-Xlinker -Bexport '
           cccdlflags='-fpic'
           lddlflags='-G '
        ;;

        *)
           ccdlflags='-Wl,-Bexport'
           cccdlflags='-Kpic'
           lddlflags='-G -Wl,-Bexport'
        ;;
esac

############################################################################
# Thread support
# use Configure -Dusethreads to enable
# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
        ccflags="$ccflags"
        shift
        libswanted="$*"
  case "$cc" in
       *gcc*)
           ccflags="-D_REENTRANT $ccflags -fpic -pthread"
           cccdlflags='-fpic'
           lddlflags='-pthread -G '
        ;;
        *)
           ccflags="-D_REENTRANT $ccflags -KPIC -Kthread"
           ccdlflags='-Kthread -Wl,-Bexport'
           cccdlflags='-KPIC -Kthread'
           lddlflags='-G -Kthread -Wl,-Bexport '
     	   ldflags='-Kthread'
        ;;
  esac
esac
EOCBU


d_suidsafe='define'	# "./Configure -d" can't figure this out easily

################## final caveat msgs to builder ###############
cat <<'EOM' >&4

If you wish to use dynamic linking, you must use 
	LD_LIBRARY_PATH=`pwd`; export LD_LIBRARY_PATH
or
	setenv LD_LIBRARY_PATH `pwd`
before running make.

If you are using shared libraries from /usr/local/lib
for libdbm or libgdbm you may need to set
	LD_RUN_PATH=/usr/local/lib; export LD_RUN_PATH
in order for Configure to compile the simple test program

EOM
