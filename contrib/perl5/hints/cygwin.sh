#! /bin/sh
# cygwin.sh - hints for building perl using the Cygwin environment for Win32
#

# not otherwise settable
exe_ext='.exe'
firstmakefile='GNUmakefile'
case "$ldlibpthname" in
'') ldlibpthname=PATH ;;
esac
archobjs='cygwin.o'

# mandatory (overrides incorrect defaults)
test -z "$cc" && cc='gcc'
if test -z "$plibpth"
then
    plibpth=`gcc -print-file-name=libc.a`
    plibpth=`dirname $plibpth`
    plibpth=`cd $plibpth && pwd`
fi
so='dll'
# - eliminate -lc, implied by gcc
libswanted=`echo " $libswanted " | sed -e 's/ c / /g'`
libswanted="$libswanted cygipc cygwin kernel32"
# - otherwise i686-cygwin
archname='cygwin'

# dynamic loading
# - otherwise -fpic
cccdlflags=' '
ld='ld2'

# optional(ish)
# - perl malloc needs to be unpolluted
bincompat5005='undef'

# stubs (ENOSYS, not implemented)
d_chroot='undef'
d_seteuid='undef'
d_setegid='undef'

# Win9x problem with non-blocking read from a closed pipe
d_eofnblk='define'

# strip exe's and dll's
#ldflags="$ldflags -s"
#ccdlflags="$ccdlflags -s"
#lddlflags="$lddlflags -s"
