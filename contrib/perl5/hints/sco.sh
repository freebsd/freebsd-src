# sco.sh
# Courtesy of Joel Rosi-Schwartz <j.schwartz@agonet.it>
###############################################################
# Additional SCO version info from
# Peter Wolfe	<wolfe@teloseng.com>
# Fri Jul 19 14:54:25 EDT 1996
# and again Tue Sep 29 16:37:25 EDT 1998
# by Andy Dougherty  <doughera@lafayette.edu>
# Mostly rewritten on
# Tue Jan 19 23:00:00 CET 1999
# by Francois Desarmenien <desar@club-internet.fr>
# Modified by Boyd Gerber <gerberb@zenez.com>
# Tue Sep 21 1999
###############################################################
#
# To use cc,  use   sh Configure
# To use gcc, use   sh Configure -Dcc=gcc
#
# Default on 3.2v4 is to use static link (dynamic loading unsupported).
# Default on 3.2v5 is to use dynamic loading.
# To use static linkink instead, use to sh Configure -Dusedl=n
#
# Warning: - to use dynamic loading with gcc, you need gcc 2.8.0 or later
# ******** - to compile with older releases of gcc, use Configure -Dusedl=n
#            or it wont compile properly
#
###############################################################
# NOTES:
# -----
#
# I Have removed inclusion of ODBM_File for OSR5
# because it core dumps and make tests fails.
#
# Support for icc compiler has been removed, because it 'breaks'
# a lot of code :-(
#
# It's *always* a good idea to first make a static link to be sure to
# have all symbols resolved with the current choice of libraries, since
# with dynamic linking, unresolved symbols are allowed an will be detected
# only at runtime (when you try to load the module or worse, when you call
# the symbol)
#
# The best choice of compiler on OSR 5 (3.2v5.*) seems to be gcc >= 2.8.0:
# -You cannot optimize with genuine sco cc (miniperl core dumps),
#  so Perl is faster if compiled with gcc.
# -Even optimized for speed, gcc generated code is smaller (!!!)
# -gcc is free
# -I use ld to link which is distributed with the core OS distribution, so you
#  don't need to buy the developement kit, just find someone kind enough to
#  give you a binary release of gcc.
#
#

###############################################################
# figure out what SCO version we are. The output of uname -X is
# something like:
#	System = SCO_SV
#	Node = xxxxx
#	Release = 3.2v5.0.0
#	KernelID = 95/08/08
#   Machine = Pentium
#	BusType = ISA
#	Serial = xxxxx
#	Users = 5-user
#	OEM# = 0
#	Origin# = 1
#   NumCPU = 1

# Use /bin/uname (because GNU uname may be first in $PATH and
# it does not support -X) to figure out what SCO version we are:
# Matching '^Release' is broken by locale setting:
# matching '3.2v' should be enough -- FD
case `/bin/uname -X | egrep '3\.2v'` in
*3.2v4.*) scorls=3 ;;   # OSR 3
*3.2v5.*) scorls=5 ;;   # OSR 5
*)
   # Future of SCO OSR is SCO UnixWare: there should not be new OSR releases
   echo "************************************************************" >&4
   echo "" >&4
   echo "  sco.sh hints file only supports:" >&4
   echo "" >&4
   echo "    - SCO Unix 3.2v4.x (OSR 3)" >&4
   echo "    - SCO Unix 3.2v5.x (OSR 5)" >&4
   echo "" >&4
   echo "" >&4
   echo "  For UnixWare, use svr4.sh hints instead" >&4
   echo "  For UnixWare 7.*, use svr5.sh hints instead" >&4
   echo "" >&4
   echo "***********************************************************" >&4
   exit
;;
esac

###############################################################
# Common fixes for all compilers an releases:

###############################################################
# What is true for SCO5 is true for SCO3 too today, so let's have a single
# symbol for both
ccflags="-U M_XENIX -D PERL_SCO"

###############################################################
# Compilers options section:
if test "$scorls" = "3"
then 
    dlext=''
    case "$cc" in
        *gcc*)  optimize='-O2' ;;
        *)      ccflags="$ccflags -W0 -quiet"
                optimize='-O' ;;
    esac
else
    ###############################################################
    # Need this in release 5 because of changed fpu exeption rules
    ccflags="$ccflags -D PERL_SCO5"

    ###############################################################
    # In Release 5, always compile ELF objects
    case "$cc" in
        *gcc*)
            ccflags="$ccflags -melf"
            optimize='-O2'
        ;;
        *)
            ccflags="$ccflags -w0 -belf"
            optimize='-O0'
        ;;
    esac
    ###############################################################
    # Dynamic loading section:
    #
    # We use ld to build shared libraries as it is always available
    # and seems to work better than GNU's one on SCO
    #
    # ccdlflags : must tell the linker to export all global symbols
    # cccdlflags: must tell the compiler to generate relocatable code
    # lddlflags : must tell the linker to output a shared library
    #
    # /usr/local/lib is added for convenience, since 'foreign' libraries
    # are usually put there in sco
    #
    if test "$usedl" != "n"; then
        ld='ld'
        case "$cc" in
            *gcc*)
                ccdlflags='-Xlinker -Bexport -L/usr/local/lib'
                cccdlflags='-fpic'
                lddlflags='-G -L/usr/local/lib'
            ;;
            *)
                ccdlflags='-Bexport -L/usr/local/lib'
                cccdlflags='-Kpic'
                lddlflags='-G -L/usr/local/lib'
            ;;
        esac

        ###############################################################
        # Use dynamic loading
        usedl='define'
        dlext='so'
        dlsrc='dl_dlopen.xs'

        ###############################################################
        # Force to define those symbols, as they are #defines and not
        # catched by Configure, and they are useful
        d_dlopen='define'
        d_dlerror='define'
    fi
fi


###############################################################
# Various hints, common to all releases, to have it work better:

###############################################################
# We need to remove libdl, as libdl.so exists, but ld complains
# it can't find libdl.a ! Bug or feature ? :-)
libswanted=`echo " $libswanted " | sed -e 's/ dl / /'`
set X $libswanted
shift
libswanted="$*"

###############################################################
# Remove libbind because it conflicts with libsocket.
libswanted=`echo " $libswanted " | sed -e 's/ bind / /'`
set X $libswanted
shift
libswanted="$*"

###############################################################
# Try to use libintl.a since it has strcoll and strxfrm
libswanted="intl $libswanted"

###############################################################
# Try to use libdbm.nfs.a since it has dbmclose.
if test -f /usr/lib/libdbm.nfs.a ; then
    libswanted=`echo "dbm.nfs $libswanted " | sed -e 's/ dbm / /'`
    set X $libswanted
    shift
    libswanted="$*"
fi

###############################################################
# We disable ODBM_File if OSR5 because it's mostly broken
# but keep it for ODT3 as it seems to work.
if test "$scorls" = "5"; then
    i_dbm='undef'
fi

###############################################################
# We don't want Xenix cross-development libraries
glibpth=`echo $glibpth | sed -e 's! /usr/lib/386 ! !' -e 's! /lib/386 ! !'`
xlibpth=''

###############################################################
# I have received one report that nm extraction doesn't work if you're
# using the scocc compiler.  This system had the following 'myconfig'
# uname='xxx xxx 3.2 2 i386 '
# cc='scocc', optimize='-O'
# You can override this with Configure -Dusenm.
case "$usenm" in
'') usenm='false' ;;
esac

###############################################################
# If you want to use nm, you'll probably have to use nm -p.  The
# following does that for you:
nm_opt='-p'

###############################################################
# I have received one report that you can't include utime.h in
# pp_sys.c.  Uncomment the following line if that happens to you:
# i_utime=undef

###############################################################
# Perl 5.003_05 and later try to include both <time.h> and <sys/select.h>
# in pp_sys.c, but that fails due to a redefinition of struct timeval.
# This will generate a WHOA THERE.  Accept the default.
i_sysselct=$undef


###############################################################
#END of hint file
