# This file has been put together by Anno Siegel <siegel@zrz.TU-Berlin.DE>,
# Andreas Koenig <k@franz.ww.TU-Berlin.DE> and Gerd Knops <gerti@BITart.com>.
# Comments, questions, and improvements welcome!
#
# These hints work for NeXT 3.2 and 3.3.  3.0 has it's own
# special hint file.
#

######################################################################
# THE MALLOC STORY
######################################################################
# 1994:
# the simple program `for ($i=1;$i<38771;$i++){$t{$i}=123}' fails
# with Larry's malloc on NS 3.2 due to broken sbrk()
#
# setting usemymalloc='n' was the solution back then. Later came
# reports that perl would run unstable on 3.2:
#
# 1996:
# From about perl5.002beta1h perl became unstable on the
# NeXT. Intermittent coredumps were frequent on 3.2 OS. There were
# reports, that the developer version of 3.3 didn't have problems, so it
# seemed pretty obvious that we had to work around an malloc bug in 3.2.
# This hints file reflects a patch to perl5.002_01 that introduces a
# home made sbrk routine (remember, NeXT's sbrk _never_ worked). This
# sbrk makes it possible to run perl with its own malloc. Thanks to
# Ilya who showed me the way to his sbrk for OS/2!!
#
# The whole malloc desaster lead to a failing gdbm test. It is far
# beyond my understanding, why GDBM_File breaks with the "fix", but in
# general I consider it better to have a working perl with broken GDBM
# than no perl at all.
#
# So, this hintsfile is using perl's malloc. If you want to turn
# perl's malloc off, you need to remove '-DUSE_PERL_SBRK'
# from the ccflags and set usemymalloc to 'n'.
#
# 1997:
# From perl5.003_22 the malloc bug has no impact any more. We can run
# a perl without a special sbrk. Apparently Chip Salzenberg, the hero
# of 5.004 anyway, earned another trophy during Australien Open.
#
# use the following two lines to enable USE_PERL_SBRK. Try this if you
# encounter intermittent core dumps:
#ccflags='-DUSE_NEXT_CTYPE -DUSE_PERL_SBRK'
#usemymalloc='y'
# use the following two lines if you have perl5.003_22 or better and
# do not encounter intermittent core dumps.

ccflags="$ccflags -DUSE_NEXT_CTYPE"
usemymalloc='n'

######################################################################
# End of the MALLOC story
######################################################################

ldflags='-u libsys_s'
libswanted='dbm gdbm db'

lddlflags='-nostdlib -r'
# Give cccdlflags an empty value since Configure will detect we are
# using GNU cc and try to specify -fpic for cccdlflags.
cccdlflags=' '

######################################################################
# MAB support
######################################################################
# By default we will build for all architectures your development
# environment supports. If you only want to build for the platform
# you are on, simply comment or remove the line below.
#
# If you want to build for specific architectures, change the line
# below to something like
#
#	archs='m68k i386'
#
archs=`/bin/lipo -info /usr/lib/libm.a | sed -n 's/^[^:]*:[^:]*: //p'`

#
# leave the following part alone
#
archcount=`echo $archs |wc -w`
if [ $archcount -gt 1 ]
then
	for d in $archs
	do
			mabflags="$mabflags -arch $d"
	done
	ccflags="$ccflags $mabflags"
	ldflags="$ldflags $mabflags"
	lddlflags="$lddlflags $mabflags"
	archname='next-fat'
fi
######################################################################
# END MAB support
######################################################################
ld='cc'

i_utime='undef'
groupstype='int'
direntrytype='struct direct'
d_strcoll='undef'
d_uname='define'
#
# At least on m68k there are situations when memcmp doesn't behave
# as expected.  So we'll use perl's memcmp.
#
d_sanemcmp='undef'
# setpgid() is in the posix library, but we don't use -posix, so
# we don't see it.  ext/POSIX/POSIX.xs  *does* use -posix, so
# setpgid is still available as POSIX::setpgid.
# See ext/POSIX/POSIX/hints/next.pl.
d_setpgid='undef'
d_setsid='define'
d_tcgetpgrp='define'
d_tcsetpgrp='define'

#
# On some NeXT machines, the timestamp put by ranlib is not correct, and
# this may cause useless recompiles.  Fix that by adding a sleep before
# running ranlib.  The '5' is an empirical number that's "long enough."
#
ranlib='sleep 5; /bin/ranlib' 

#
# There where reports that the compiler on HPPA machines
# fails with the -O flag on pp.c.
# Compiling pp.c with -O for HPPA machines results in a broken perl.
# This is true whether we're on an HPPA machine or cross-compiling
# for one.
pp_cflags='optimize=""'

# The SysV IPC is optional (ftp://ftp.nluug.nl/pub/comp/next/SysVIPC/)
# Gerben_Wierda@RnA.nl
if [ -f /usr/local/lib/libIPC.a ]; then
  libswanted="$libswanted IPC"
  # As of Sep 1998 d_msg wasn't supported in that library,
  # only d_sem and d_shm, but Configure should be able to
  # figure that out. --jhi
  # Note also the next3 ext/IPC/SysV hints file.
fi
