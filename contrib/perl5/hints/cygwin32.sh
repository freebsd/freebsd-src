#! /bin/sh
# cygwin32.sh - hintsfile for building perl on Windows NT using the
#     Cygnus Win32 Development Kit.
#     See "http://www.cygnus.com/misc/gnu-win32/" to learn about the kit.
#
path_sep=\;
exe_ext='.exe'
firstmakefile='GNUmakefile'
if test -f $sh.exe; then sh=$sh.exe; fi
startsh="#!$sh"
cc='gcc2'
ld='ld2'
usrinc='/gnuwin32/H-i386-cygwin32/i386-cygwin32/include'
libpth='/gnuwin32/H-i386-cygwin32/i386-cygwin32/lib /gnuwin32/H-i386-cygwin32/lib'
libs='-lcygwin -lm -lc -lkernel32'
# dynamic lib stuff
so='dll'
#i_dlfcn='define'
dlsrc='dl_cygwin32.xs'
usedl='y'
# flag to include the perl.exe export variable translation file cw32imp.h
# when building extension libs
cccdlflags='-DCYGWIN32 -DDLLIMPORT '
# flag that signals gcc2 to build exportable perl
ccdlflags='-buildperl '
lddlflags='-L../.. -L/gnuwin32/H-i386-cygwin32/i386-cygwin32/lib -lperlexp -lcygwin'
d_voidsig='undef'
extensions='Fcntl IO Opcode SDBM_File'
lns='cp'
signal_t='int'
useposix='false'
rd_nodata='0'
eagain='EAGAIN'
archname='cygwin32'
#

installbin='/usr/local/bin'
installman1dir=''
installman3dir=''
installprivlib='/usr/local/lib/perl5'
installscript='/usr/local/bin'

installsitelib='/usr/local/lib/perl5/site_perl'
libc='/gnuwin32/H-i386-cygwin32/i386-cygwin32/lib/libc.a'

perlpath='/usr/local/bin/perl'

sitelib='/usr/local/lib/perl5/site_perl'
sitelibexp='/usr/local/lib/perl5/site_perl'
usrinc='/gnuwin32/H-i386-cygwin32/i386-cygwin32/include'
