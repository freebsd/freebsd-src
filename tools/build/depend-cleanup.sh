#!/bin/sh
#
# $FreeBSD$
#
# Our current make(1)-based approach to dependency tracking cannot cope with
# certain source tree changes, including:
# - removing source files
# - replacing generated files with files committed to the tree
# - changing file extensions (e.g. a C source file rewritten in C++)
#
# We handle those cases here in an ad-hoc fashion by looking for the known-
# bad case in the main .depend file, and if found deleting all of the related
# .depend files (including for example the lib32 version).
#
# These tests increase the build time (albeit by a small amount), so they
# should be removed once enough time has passed and it is extremely unlikely
# anyone would try a NO_CLEAN build against an object tree from before the
# related change.  One year should be sufficient.

set -e
set -u

warn()
{
	echo "$(basename "$0"):" "$@" >&2
}

err()
{
	warn "$@"
	exit 1
}

usage()
{
	echo "usage: $(basename $0) [-v] [-n] objtop" >&2
}

VERBOSE=
PRETEND=
while getopts vn o; do
	case "$o" in
	v)
		VERBOSE=1
		;;
	n)
		PRETEND=1
		;;
	*)
		usage
		exit 1
		;;
	esac
done
shift $((OPTIND-1))

if [ $# -ne 1 ]; then
	usage
	exit 1
fi

OBJTOP=$1
shift
if [ ! -d "$OBJTOP" ]; then
	err "$OBJTOP: Not a directory"
fi

if [ -z "${MACHINE+set}" ]; then
	err "MACHINE not set"
fi

if [ -z "${MACHINE_ARCH+set}" ]; then
	err "MACHINE_ARCH not set"
fi

run()
{
	if [ "$VERBOSE" ]; then
		echo "$@"
	fi
	if ! [ "$PRETEND" ]; then
		"$@"
	fi
}

# $1 directory
# $2 source filename w/o extension
# $3 source extension
clean_dep()
{
	if egrep -qw "$2\.$3" "$OBJTOP"/$1/.depend.$2.*o 2>/dev/null; then
		echo "Removing stale dependencies and objects for $2.$3"
		run rm -f \
		    "$OBJTOP"/$1/.depend.$2.* \
		    "$OBJTOP"/$1/$2.*o
	fi
	if egrep -qw "$2\.$3" "$OBJTOP"/obj-lib32/$1/.depend.$2.*o 2>/dev/null; then
		echo "Removing 32-bit stale dependencies and objects for $2.$3"
		run rm -f \
		    "$OBJTOP"/obj-lib32/$1/.depend.$2.* \
		    "$OBJTOP"/obj-lib32/$1/$2.*o
	fi
}

# Date      Rev      Description
# 20200310  r358851  rename of openmp's ittnotify_static.c to .cpp
clean_dep lib/libomp ittnotify_static c
# 20200414  r359930  closefrom
clean_dep lib/libc   closefrom S

# 20200826  r364746  OpenZFS merge, apply a big hammer (remove whole tree)
if [ -e "$OBJTOP"/cddl/lib/libzfs/.depend.libzfs_changelist.o ] && \
    egrep -qw "cddl/contrib/opensolaris/lib/libzfs/common/libzfs_changelist.c" \
    "$OBJTOP"/cddl/lib/libzfs/.depend.libzfs_changelist.o; then
	echo "Removing old ZFS tree"
	run rm -rf "$OBJTOP"/cddl "$OBJTOP"/obj-lib32/cddl
fi

# 20200916  WARNS bumped, need bootstrapped crunchgen stubs
if [ -e "$OBJTOP"/rescue/rescue/rescue.c ] && \
    ! grep -q 'crunched_stub_t' "$OBJTOP"/rescue/rescue/rescue.c; then
	echo "Removing old rescue(8) tree"
	run rm -rf "$OBJTOP"/rescue/rescue
fi

# 20210105  fda7daf06301   pfctl gained its own version of pf_ruleset.c
if [ -e "$OBJTOP"/sbin/pfctl/.depend.pf_ruleset.o ] && \
    egrep -qw "sys/netpfil/pf/pf_ruleset.c" \
    "$OBJTOP"/sbin/pfctl/.depend.pf_ruleset.o; then
	echo "Removing old pf_ruleset dependecy file"
	run rm -rf "$OBJTOP"/sbin/pfctl/.depend.pf_ruleset.o
fi

# 20210108  821aa63a0940   non-widechar version of ncurses removed
if [ -e "$OBJTOP"/lib/ncurses/ncursesw ]; then
	echo "Removing stale ncurses objects"
	run rm -rf "$OBJTOP"/lib/ncurses "$OBJTOP"/obj-lib32/lib/ncurses
fi

# 20210608  f20893853e8e    move from atomic.S to atomic.c
clean_dep   cddl/lib/libspl atomic S
# 20211207  cbdec8db18b5    switch to libthr-friendly pdfork
clean_dep   lib/libc        pdfork S

# 20211230  5e6a2d6eb220    libc++.so.1 path changed in ldscript
if [ -e "$OBJTOP"/lib/libc++/libc++.ld ] && \
    fgrep -q "/usr/lib/libc++.so" "$OBJTOP"/lib/libc++/libc++.ld; then
	echo "Removing old libc++ linker script"
	run rm -f "$OBJTOP"/lib/libc++/libc++.ld
fi

# 20220326  fbc002cb72d2    move from bcmp.c to bcmp.S
if [ "$MACHINE_ARCH" = "amd64" ]; then
	clean_dep lib/libc bcmp c
fi

# 20220524  68fe988a40ca    kqueue_test binary replaced shell script
if stat "$OBJTOP"/tests/sys/kqueue/libkqueue/*kqtest* \
    "$OBJTOP"/tests/sys/kqueue/libkqueue/.depend.kqtest* >/dev/null 2>&1; then
	echo "Removing old kqtest"
	run rm -f "$OBJTOP"/tests/sys/kqueue/libkqueue/.depend.* \
	   "$OBJTOP"/tests/sys/kqueue/libkqueue/*
fi

# 20221115  42d10b1b56f2    move from rs.c to rs.cc
clean_dep   usr.bin/rs      rs c

# 20230110  bc42155199b5    usr.sbin/zic/zic -> usr.sbin/zic
if [ -d "$OBJTOP"/usr.sbin/zic/zic ] ; then
	echo "Removing old zic directory"
	run rm -rf "$OBJTOP"/usr.sbin/zic/zic
fi

# 20230208  29c5f8bf9a01    move from mkmakefile.c to mkmakefile.cc
clean_dep   usr.sbin/config  mkmakefile c
# 20230209  83d7ed8af3d9    convert to main.cc and mkoptions.cc
clean_dep   usr.sbin/config  main c
clean_dep   usr.sbin/config  mkoptions c

# 20230401  54579376c05e    kqueue1 from syscall to C wrapper
clean_dep   lib/libc        kqueue1 S

# 20230623  b077aed33b7b    OpenSSL 3.0 update
if [ -f "$OBJTOP"/secure/lib/libcrypto/aria.o ]; then
	echo "Removing old OpenSSL 1.1.1 tree"
	run rm -rf "$OBJTOP"/secure/lib/libcrypto \
	    "$OBJTOP"/secure/lib/libssl \
	    "$OBJTOP"/obj-lib32/secure/lib/libcrypto \
	    "$OBJTOP"/obj-lib32/secure/lib/libssl
fi

# 20230711  ee8b0c436d72    replace ffs/fls implementations with clang builtins
clean_dep   lib/libc        ffs S
