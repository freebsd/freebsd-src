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
