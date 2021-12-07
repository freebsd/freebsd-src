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

OBJTOP=$1
if [ ! -d "$OBJTOP" ]; then
	echo "usage: $(basename $0) objtop" >&2
	exit 1
fi

# $1 directory
# $2 source filename w/o extension
# $3 source extension
clean_dep()
{
	if [ -e "$OBJTOP"/$1/.depend.$2.pico ] && \
	    egrep -qw "$2\.$3" "$OBJTOP"/$1/.depend.$2.pico; then \
		echo "Removing stale dependencies and objects for $2.$3"; \
		rm -f \
		    "$OBJTOP"/$1/.depend.$2.* \
		    "$OBJTOP"/$1/$2.*o \
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
	rm -rf "$OBJTOP"/cddl "$OBJTOP"/obj-lib32/cddl
fi

# 20200916  WARNS bumped, need bootstrapped crunchgen stubs
if [ -e "$OBJTOP"/rescue/rescue/rescue.c ] && \
    ! grep -q 'crunched_stub_t' "$OBJTOP"/rescue/rescue/rescue.c; then
	echo "Removing old rescue(8) tree"
	rm -rf "$OBJTOP"/rescue/rescue
fi

# 20210105  fda7daf06301   pfctl gained its own version of pf_ruleset.c
if [ -e "$OBJTOP"/sbin/pfctl/.depend.pf_ruleset.o ] && \
    egrep -qw "sys/netpfil/pf/pf_ruleset.c" \
    "$OBJTOP"/sbin/pfctl/.depend.pf_ruleset.o; then
	echo "Removing old pf_ruleset dependecy file"
	rm -rf "$OBJTOP"/sbin/pfctl/.depend.pf_ruleset.o
fi

# 20210108  821aa63a0940   non-widechar version of ncurses removed
if [ -e "$OBJTOP"/lib/ncurses/ncursesw ]; then
	echo "Removing stale ncurses objects"
	rm -rf "$OBJTOP"/lib/ncurses "$OBJTOP"/obj-lib32/lib/ncurses
fi

# 20210608  f20893853e8e    move from atomic.S to atomic.c
clean_dep   cddl/lib/libspl atomic S
# 20211207  cbdec8db18b5    switch to libthr-friendly pdfork
clean_dep   lib/libc        pdfork S
