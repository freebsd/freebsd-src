# hints/os390.sh
#
# OS/390 hints by David J. Fiander <davidf@mks.com>
#
# OS/390 OpenEdition Release 3 Mon Sep 22 1997 thanks to:
#     
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
cc='c89'
ld='c89'
# To link via definition side decks we need the dll option
cccdlflags='-W 0,dll,"langlvl(extended)"'
# c89 hides most of the useful header stuff, _ALL_SOURCE turns it on again,
# YYDYNAMIC ensures that the OS/390 yacc generated parser is reentrant.
# -DEBCDIC should come from Configure.
ccflags='-DMAXSIG=38 -DOEMVS -D_OE_SOCKETS -D_XOPEN_SOURCE_EXTENDED -D_ALL_SOURCE -DYYDYNAMIC'
# Turning on optimization breaks perl
optimize='none'

alignbytes=8

usemymalloc='n'

so='a'

# On OS/390, libc.a doesn't really hold anything at all,
# so running nm on it is pretty useless.
usenm='n'

# Dynamic loading doesn't work on OS/390 quite yet
usedl='n'
dlext='none'

# Configure can't figure this out for some reason
d_shmatprototype='define'

usenm='false'
i_time='define'
i_systime='define'

# (from aix.sh)
# uname -m output is too specific and not appropriate here
# osname should come from Configure
#
case "$archname" in
'') archname="$osname" ;;
esac

archobjs=ebcdic.o

# We have our own cppstdin.
echo 'cat >.$$.c; '"$cc"' -E -Wc,NOLOC ${1+"$@"} .$$.c; rm .$$.c' > cppstdin

#
# Note that Makefile.SH employs a bare yacc to generate 
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

