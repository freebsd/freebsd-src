# machten.sh
# This file has been put together by Mark Pease <peasem@primenet.com>
# Comments, questions, and improvements welcome!
#
# MachTen does not support dynamic loading. If you wish to, you
# can fetch, compile, and install the dld package.
# This ought to work with the ext/DynaLoader/dl_dld.xs in the 
# perl5 package. Have fun!
# Some possible locations for dld:
# ftp-swiss.ai.mit.edu:pub/scm/dld-3.2.7.tar.gz
# prep.ai.mit.edu:/pub/gnu/jacal/dld-3.2.7.tar.gz
# ftp.cs.indiana.edu:/pub/scheme-repository/imp/SCM-support/dld-3.2.7.tar.gz
# tsx-11.mit.edu:/pub/linux/sources/libs/dld-3.2.7.tar.gz
#
#  Original version was for MachTen 2.1.1.
#  Last modified by Andy Dougherty   <doughera@lafcol.lafayette.edu>
#  Tue Aug 13 12:31:01 EDT 1996
#
#  Warning about tests which no longer fail
#    fixed by Tom Phoenix <rootbeer@teleport.com>
#  March 5, 1997
#
#  Locale, optimization, and malloc changes by Tom Phoenix Mar 15, 1997
#
#  groupstype change and note about t/lib/findbin.t by Tom, Mar 24, 1997

# MachTen's ability to have valid filepaths beginning with "//" may
# be causing lib/FindBin.pm to fail. I don't know how to fix it, but
# the reader is encouraged to do so! :-)  -- Tom

# There seem to be some hard-to-diagnose problems under MachTen's
# malloc, so we'll use Perl's. If you have problems which Perl's
# malloc's diagnostics can't help you with, you may wish to use
# MachTen's malloc after all.
case "$usemymalloc" in
'') usemymalloc='y' ;;
esac

# I (Tom Phoenix) don't know how to test for locales on MachTen. (If
# you do, please fix this hints file!) But since mine didn't come
# with locales working out of the box, I'll assume that's the case
# for most folks.
case "$d_setlocale" in
'') d_setlocale=undef
esac

# MachTen doesn't have secure setid scripts
d_suidsafe='undef'

# groupstype should be gid_t, as near as I can tell, but it only
# seems to work right when it's int. 
groupstype='int'

case "$optimize" in
'') optimize='-O2' ;;
esac

so='none'
# These are useful only if you have DLD, but harmless otherwise.
# Make sure gcc doesn't use -fpic.
cccdlflags=' '  # That's an empty space.
lddlflags='-r'
dlext='o'

# MachTen does not support POSIX enough to compile the POSIX module.
useposix=false

#MachTen might have an incomplete Berkeley DB implementation.
i_db=$undef

#MachTen versions 2.X have no hard links.  This variable is used
# by File::Find.
# This will generate a harmless message:
# Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
#	Propagating recommended variable dont_use_nlink
# Without this, tests io/fs #4 and op/stat #3 will fail.
dont_use_nlink=define

cat <<'EOM' >&4

During Configure, you may get two "WHOA THERE" messages, for $d_setlocale
and $i_db being 'undef'. You may keep the undef value.

At the end of Configure, you will see a harmless message

Hmm...You had some extra variables I don't know about...I'll try to keep 'em.
	Propagating recommended variable dont_use_nlink

Read the File::Find documentation for more information.

It's possible that test t/lib/findbin.t will fail on some configurations
of MachTen.

EOM
