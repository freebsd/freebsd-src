# The MPE/iX linker doesn't complain about unresolved symbols, and so the only
# way to test for unresolved symbols in a program is by attempting to run it.
# But this is slow, and fraught with problems, so the better solution is to use
# nm.
#
# MPE/iX lacks a fully functional native nm, so we need to use our fake nm
# script which will extract the symbol info from the native link editor and
# reformat into something nm-like.
#
# Created for 5.003 by Mark Klein, mklein@dis.com.
# Substantially revised for 5.004_01 by Mark Bixby, markb@cccd.edu.
# Revised again for 5.004_69 by Mark Bixby, markb@cccd.edu.
#
osname='mpeix'
osvers='5.5'
#
# Force Configure to use our wrapper mpeix/nm script
#
PATH="$PWD/mpeix:$PATH"
nm="$PWD/mpeix/nm"
_nm=$nm
nm_opt='-configperl'
usenm='true'
#
# Various directory locations.
#
prefix='/PERL/PUB'
archname='PA-RISC1.1'
bin="$prefix"
installman1dir="$prefix/man/man1"
installman3dir="$prefix/man/man3"
man1dir="$prefix/man/man1"
man3dir="$prefix/man/man3"
perlpath="$prefix/PERL"
scriptdir="$prefix"
startperl="#!$prefix/perl"
startsh='#!/bin/sh'
#
# Compiling.
#
cc='gcc'
cccdlflags='none'
ccflags='-DMPE -D_POSIX_SOURCE -D_SOCKET_SOURCE -D_POSIX_JOB_CONTROL -DIS_SOCKET_CLIB_ITSELF'
locincpth='/usr/local/include /usr/contrib/include /BIND/PUB/include'
optimize='-O2'
ranlib='/bin/true'
# Special compiling options for certain source files.
regcomp_cflags='optimize=-O'
toke_cflags='ccflags="$ccflags -DARG_ZERO_IS_SCRIPT"'
#
# Linking.
#
lddlflags='-b'
libs='-lbind -lsyslog -lcurses -lsvipc -lsocket -lm -lc'
loclibpth='/usr/local/lib /usr/contrib/lib /BIND/PUB/lib /SYSLOG/PUB'
#
# External functions and data items.
#
d_crypt='define'
d_difftime='define'
d_dlerror='undef'
d_dlopen='undef'
d_Gconvert='gcvt((x),(n),(b))'
d_inetaton='undef'
d_link='undef'
d_mblen='define'
d_mbstowcs='define'
d_mbtowc='define'
d_memcmp='define'
d_memcpy='define'
d_memmove='define'
d_memset='define'
d_pwage='undef'
d_pwcomment='undef'
d_pwgecos='undef'
d_pwpasswd='undef'
d_setpgid='undef'
d_setsid='undef'
d_setvbuf='define'
d_statblks='undef'
d_strchr='define'
d_strcoll='define'
d_strerrm='strerror(e)'
d_strerror='define'
d_strtod='define'
d_strtol='define'
d_strtoul='define'
d_strxfrm='define'
d_syserrlst='define'
d_time='define'
d_wcstombs='define'
d_wctomb='define'
#
# Include files.
#
i_termios='undef'
i_time='define'
i_systime='undef'
i_systimek='undef'
timeincl='/usr/include/time.h'
#
# Data types.
#
timetype='time_t'
