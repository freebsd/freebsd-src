# hints/os390.sh
#
# OS/390 hints by David J. Fiander <davidf@mks.com>
#
# OS/390 OpenEdition Release 3 Mon Sep 22 1997 thanks to:
# 
#     John Goodyear <johngood@us.ibm.com>
#     John Pfuntner <pfuntner@vnet.ibm.com>
#     Len Johnson <lenjay@ibm.net>
#     Bud Huff  <BAHUFF@us.oracle.com>
#     Peter Prymmer <pvhp@forte.com>
#     Andy Dougherty  <doughera@lafcol.lafayette.edu>
#     Tim Bunce  <Tim.Bunce@ig.co.uk>
#
#  as well as the authors of the aix.sh file
#

# To get ANSI C, we need to use c89, and ld doesn't exist
# You can override this with Configure -Dcc=gcc -Dld=ld.
case "$cc" in
'') cc='c89' ;;
esac
case "$ld" in
'') ld='c89' ;;
esac

# -DMAXSIG=38 maximum signal number
# -DOEMVS is used in place of #ifdef __MVS__ in certain places.
# -D_OE_SOCKETS alters system headers.
# -D_XOPEN_SOURCE_EXTENDEDA alters system headers.
# c89 hides most of the useful header stuff, _ALL_SOURCE turns it on again.
# YYDYNAMIC ensures that the OS/390 yacc generated parser is reentrant.
# -DEBCDIC should come from Configure and need not be mentioned here.
# Prepend your favorites with Configure -Dccflags=your_favorites
case "$ccflags" in
'') ccflags='-DMAXSIG=38 -DOEMVS -D_OE_SOCKETS -D_XOPEN_SOURCE_EXTENDED -D_ALL_SOURCE -DYYDYNAMIC' ;;
*) ccflags="$ccflags -DMAXSIG=38 -DOEMVS -D_OE_SOCKETS -D_XOPEN_SOURCE_EXTENDED -D_ALL_SOURCE -DYYDYNAMIC" ;;
esac

# Turning on optimization breaks perl.
# You can override this with Configure -Doptimize='-O' or somesuch.
case "$optimize" in
'') optimize='none' ;;
esac

# To link via definition side decks we need the dll option
# You can override this with Configure -Ucccdlflags or somesuch.
case "$cccdlflags" in
'') cccdlflags='-W 0,dll' ;;
esac

case "$so" in
'') so='a' ;;
esac

case "$alignbytes" in
'') alignbytes=8 ;;
esac

case "$usemymalloc" in
'') usemymalloc='n' ;;
esac

# On OS/390, libc.a doesn't really hold anything at all,
# so running nm on it is pretty useless.
# You can override this with Configure -Dusenm.
case "$usenm" in
'') usenm='false' ;;
esac

# Setting ldflags='-Wl,EDIT=NO' will get rid of the symbol
# information at the end of the executable (=> smaller binaries).
# Override this option with -Dldflags='whatever else you wanted'.
case "$ldflags" in
'') ldflags='-Wl,EDIT=NO' ;;
esac

# In order to build with dynamic be sure to specify:
#   Configure -Dusedl
# Do not forget to add $archlibexp/CORE to your LIBPATH.
# You might want to override some of this with things like:
#  Configure -Dusedl -Ddlext=so -Ddlsrc=dl_dllload.xs.
case "$usedl" in
'')
    usedl='n' 
    case "$dlext" in
    '') dlext='none' ;;
    esac
    ;;
define)
    case "$useshrplib" in
    '') useshrplib='true' ;;
    esac
    case "$dlsrc" in
    '') dlsrc='dl_dllload.xs' ;;
    esac
    # For performance use 'so' at or beyond v2.8, 'dll' for 2.7 and prior versions
    case "`uname -v`x`uname -r`" in
    02x0[89].*|02x1[0-9].*|[0-9][3-9]x*) 
        so='so'
        case "$dlext" in
        '') dlext='so' ;;
        esac
        ;;
    *) 
        so='dll'
        case "$dlext" in
        '') dlext='dll' ;;
        esac
        ;;
    esac
    libperl="libperl.$so"
    ccflags="$ccflags -D_SHR_ENVIRON -DPERL_EXTERNAL_GLOB -Wc,dll"
    cccdlflags='-c -Wc,dll,EXPORTALL'
    # The following will need to be modified for the installed libperl.x.
    # The modification to Config.pm is done by the installperl script after the build and test.
    ccdlflags="-W l,dll `pwd`/libperl.x"
    lddlflags="-W l,dll `pwd`/libperl.x"
    ;;
esac
# even on static builds using LIBPATH should be OK.
case "$ldlibpthname" in
'') ldlibpthname=LIBPATH ;;
esac

# Header files to include.
# You can override these with Configure -Ui_time -Ui_systime.
case "$i_time" in
'') i_time='define' ;;
esac
case "$i_systime" in
'') i_systime='define' ;;
esac

# (from aix.sh)
# uname -m output is too specific and not appropriate here
# osname should come from Configure
# You can override this with Configure -Darchname='s390' but please don't.
case "$archname" in
'') archname="$osname" ;;
esac

# We have our own cppstdin script.  This is not a variable since 
# Configure sees the presence of the script file.
# We put system header -D definitions in so that Configure
# can find the shmat() prototype in <sys/shm.h> and various
# other things.  Unfortunately, cppflags occurs too late to be of 
# value external to the script.  This may need to be revisited 
# under a compiler other than c89.
case "$usedl" in
define)
echo 'cat >.$$.c; '"$cc"' -D_OE_SOCKETS -D_XOPEN_SOURCE_EXTENDED -D_ALL_SOURCE -D_SHR_ENVIRON -E -Wc,NOLOC ${1+"$@"} .$$.c; rm .$$.c' > cppstdin
    ;;
*)
echo 'cat >.$$.c; '"$cc"' -D_OE_SOCKETS -D_XOPEN_SOURCE_EXTENDED -D_ALL_SOURCE -E -Wc,NOLOC ${1+"$@"} .$$.c; rm .$$.c' > cppstdin
    ;;
esac

#
# Note that Makefile.SH employs a bare yacc command to generate 
# perly.[hc] and a2p.[hc], hence you may wish to:
#
#    alias yacc='myyacc'
#
# Then if you would like to use myyacc and skip past the
# following warnings try invoking Configure like so: 
#
#    sh Configure -Dbyacc=yacc
#
# This trick ought to work even if your yacc is byacc.
#
if test "X$byacc" = "Xbyacc" ; then
    if test -e /etc/yyparse.c ; then
        : we should be OK - perhaps do a test -r?
    else
        cat <<EOWARN >&4

Warning.  You do not have a copy of yyparse.c, the default 
yacc parser template file, in place in /etc.
EOWARN
        if test -e /samples/yyparse.c ; then
            cat <<EOWARN >&4

There does appear to be a template file in /samples though.
Please run:

      cp /samples/yyparse.c /etc

before attempting to Configure the build of $package.

EOWARN
        else
            cat <<EOWARN >&4

There does not appear to be one in /samples either.  
If you feel you can make use of an alternate yacc-like 
parser generator then please read the comments in the
hints/os390.sh file carefully.

EOWARN
        fi
        exit 1
    fi
fi

