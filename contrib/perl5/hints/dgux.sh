# $Id: dgux.sh,v 1.8 1996-11-29 18:16:43-05 roderick Exp $

# This is a hints file for DGUX, which is Data General's Unix.  It was
# originally developed with version 5.4.3.10 of the OS, and then was
# later updated running under version 4.11.2 (running on m88k hardware).
# The gross features should work with versions going back to 2.nil but
# some tweaking will probably be necessary.
#
# DGUX is a SVR4 derivative.  It ships with gcc as the standard
# compiler.  Since version 3.0 it has shipped with Perl 4.036
# installed in /usr/bin, which is kind of neat.  Be careful when you
# install that you don't overwrite the system version, though (by
# answering yes to the question about installing perl as /usr/bin/perl),
# as it would suck to try to get support if the vendor learned that you
# were physically replacing the system binaries.
#
# Be aware that if you opt to use dynamic loading you'll need to set
# your $LD_LIBRARY_PATH to include the source directory when you build,
# test and install the software.
#
# -Roderick Schertler <roderick@argon.org>


# Here are the things from some old DGUX hints files which are different
# from what's in here now.  I don't know the exact reasons that most of
# these settings were in the hints files, presumably they can be chalked
# up to old Configure inadequacies and changes in the OS headers and the
# like.  These settings might make a good place to start looking if you
# have problems.
#
# This was specified the the 4.036 hints file.  That hints file didn't
# say what version of the OS it was developed using.
#
#     cppstdin='/lib/cpp'
#
# The 4.036 and 5.001 hints files both contained these.  The 5.001 hints
# file said it was developed with version 2.01 of DGUX.
#
#     gidtype='gid_t'
#     groupstype='gid_t'
#     uidtype='uid_t'
#     d_index='define'
#     cc='gcc'
#
# These were peculiar to the 5.001 hints file.
#
#     ccflags='-D_POSIX_SOURCE -D_DGUX_SOURCE'
#
#     # an ugly hack, since the Configure test for "gcc -P -" hangs.
#     # can't just use 'cppstdin', since our DG has a broken cppstdin :-(
#     cppstdin=`cd ..; pwd`/cppstdin
#     cpprun=`cd ..; pwd`/cppstdin
#
# One last note:  The 5.001 hints file said "you don't want to use
# /usr/ucb/cc" in the place at which it set cc to gcc.  That in
# particular baffles me, as I used to have 2.01 loaded and my memory
# is telling me that even then /usr/ucb was a symlink to /usr/bin.


# The standard system compiler is gcc, but invoking it as cc changes its
# behavior.  I have to pick one name or the other so I can get the
# dynamic loading switches right (they vary depending on this).  I'm
# picking gcc because there's no way to get at the optimization options
# and so on when you call it cc.
case $cc in
    '')
	cc=gcc
	case $optimize in
	    '') optimize=-O2;;
	esac
	;;
esac

usevfork=true

# DG has this thing set up with symlinks which point to different places
# depending on environment variables (see elink(5)) and the compiler and
# related tools use them to access different development environments
# (COFF, ELF, m88k BCS and so on), see sde(5).  The upshot, however, is
# that when a normal program tries to access one of these elinks it sees
# no such file (like stat()ting a mis-directed symlink).  Setting
# $plibpth to explicitly include the place to which the elinks point
# allows Configure to find libraries which vary based on the development
# environment.
#
# Starting with version 4.10 (the first time the OS supported Intel
# hardware) all libraries are accessed with this mechanism.
#
# The default $TARGET_BINARY_INTERFACE changed with version 4.10.  The
# system now comes with a link named /usr/sde/default which points to
# the proper entry, but older versions lacked this and used m88kdgux
# directly.

: && sde_path=${SDE_PATH:-/usr}/sde	# hide from Configure
while : # dummy loop
do
    if [ -n "$TARGET_BINARY_INTERFACE" ]
	then set X "$TARGET_BINARY_INTERFACE"
	else set X default dg m88k_dg ix86_dg m88kdgux m88kdguxelf
    fi
    shift
    default_sde=$1
    for sde
    do
	[ -d "$sde_path/$sde" ] && break 2
    done
    cat <<END >&2

NOTE:  I can't figure out what SDE is used by default on this machine (I
didn't find a likely directory under $sde_path).  This is bad news.  If
this is a R4.10 or newer system I'm not going to be able to find any of
your libraries, if this system is R3.10 or older I won't be able to find
the math library.  You should re-run Configure with the environment
variable TARGET_BINARY_INTERFACE set to the proper value for this
machine, see sde(5) and the notes in hints/dgux.sh.

END
    sde=$default_sde
    break
done

plibpth="$plibpth $sde_path/$sde/usr/lib"
unset sde_path default_sde sde

# Many functions (eg, gethostent(), killpg(), getpriority(), setruid()
# dbm_*(), and plenty more) are defined in -ldgc.  Usually you don't
# need to know this (it seems that libdgc.so is searched automatically
# by ld), but Configure needs to check it otherwise it will report all
# those functions as missing.
libswanted="dgc $libswanted"

# Dynamic loading works using the dlopen() functions.  Note that dlfcn.h
# used to be broken, it declared _dl*() rather than dl*().  This was the
# case up to 3.10, it has been fixed in 4.11.  I'm not sure if it was
# fixed in 4.10.  If you have the older header just ignore the warnings
# (since pointers and integers have the same format on m88k).
usedl=true
# For cc rather than gcc the flags would be `-K PIC' for compiling and
# -G for loading.  I haven't tested this.
cccdlflags=-fpic
lddlflags=-shared
