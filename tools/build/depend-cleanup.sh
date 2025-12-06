#!/bin/sh
#
#
# Our current make(1)-based approach to dependency tracking cannot cope with
# certain source tree changes, including:
#
# - removing source files
# - replacing generated files with files committed to the tree
# - changing file extensions (e.g. a C source file rewritten in C++)
# - moving a file from one directory to another
#
# Note that changing extensions or moving files may occur in effect as a result
# of switching from a generic machine-independent (MI) implementation file to a
# machine-dependent (MD) one.
#
# We handle those cases here in an ad-hoc fashion by looking for the known-
# bad case in the main .depend file, and if found deleting all of the related
# .depend files (including for example the lib32 version).
#
# These tests increase the build time (albeit by a small amount), so they
# should be removed once enough time has passed and it is extremely unlikely
# anyone would try a NO_CLEAN build against an object tree from before the
# related change.  One year should be sufficient.
#
# Groups of cleanup rules begin with a comment including the date and git hash
# of the affected commit, and a description.  The clean_dep function (below)
# handles common dependency cleanup cases.  See the comment above the function
# for its arguments.
#
# Examples of each of the special cases:
#
# - Removing a source file (including changing a file's extension).  The path,
#   file, and extension are passed to clean_dep.
#
#   # 20231031  0527c9bdc718    Remove forward compat ino64 stuff
#   clean_dep   lib/libc        fstat         c
#
#   # 20221115  42d10b1b56f2    move from rs.c to rs.cc
#   clean_dep   usr.bin/rs      rs c
#
# - Moving a file from one directory to another.  Note that a regex is passed to
#   clean_dep, as the default regex is derived from the file name (strncat.c in
#   this example) does not change.  The regex matches the old location, does not
#   match the new location, and does not match any dependency shared between
#   them.  The `/`s are replaced with `.` to avoid awkward escaping.
#
#   # 20250110  3dc5429158cf  add strncat SIMD implementation
#   clean_dep   lib/libc strncat c "libc.string.strncat.c"
#
# - Replacing generated files with files committed to the tree.  This is special
#   case of moving from one directory to another.  The stale generated file also
#   needs to be deleted, so that it isn't found in make's .PATH.  Note the
#   unconditional `rm -fv`: there's no need for an extra call to first check for
#   the file's existence.
#
#   # 20250110  3863fec1ce2d  add strlen SIMD implementation
#   clean_dep   lib/libc strlen S arm-optimized-routines
#   run rm -fv "$OBJTOP"/lib/libc/strlen.S
#
# A rule may be required for only one architecture:
#
#   # 20220326  fbc002cb72d2    move from bcmp.c to bcmp.S
#   if [ "$MACHINE_ARCH" = "amd64" ]; then
#           clean_dep lib/libc bcmp c
#   fi
#
# We also have a big hammer at the top of the tree, .clean_build_epoch, to be
# used in severe cases where we can't surgically remove just the parts that
# need rebuilt.  This should be used sparingly.

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
	echo "usage: $(basename $0) [-v] [-n] objtop srctop" >&2
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

if [ $# -ne 2 ]; then
	usage
	exit 1
fi

OBJTOP=$1
shift
SRCTOP=$1
shift

if [ ! -d "$OBJTOP" ]; then
	err "$OBJTOP: Not a directory"
fi

if [ ! -d "$SRCTOP" -o ! -f "$SRCTOP/Makefile.inc1" ]; then
	err "$SRCTOP: Not the root of a src tree"
fi

: ${CLEANMK=""}
if [ -n "$CLEANMK" ]; then
	if [ -z "${MAKE+set}" ]; then
		err "MAKE not set"
	fi
fi

if [ -z "${MACHINE+set}" ]; then
	err "MACHINE not set"
fi

if [ -z "${MACHINE_ARCH+set}" ]; then
	err "MACHINE_ARCH not set"
fi

if [ -z "${ALL_libcompats+set}" ]; then
	err "ALL_libcompats not set"
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

# Clean the depend and object files for a given source file if the
# depend file matches a regex (which defaults to the source file
# name).  This is typically used if a file was renamed, especially if
# only its extension was changed (e.g. from .c to .cc).
#
# $1 directory
# $2 source filename w/o extension
# $3 source extension
# $4 optional regex for egrep -w
clean_dep()
{
	for libcompat in "" $ALL_libcompats; do
		dirprfx=${libcompat:+obj-lib${libcompat}/}
		if egrep -qw "${4:-$2\.$3}" "$OBJTOP"/$dirprfx$1/.depend.$2.*o 2>/dev/null; then
			echo "Removing stale ${libcompat:+lib${libcompat} }dependencies and objects for $2.$3"
			run rm -fv \
			    "$OBJTOP"/$dirprfx$1/.depend.$2.* \
			    "$OBJTOP"/$dirprfx$1/$2.*o
		fi
	done
}

# Clean the object file for a given source file if it exists and
# matches a regex.  This is typically used if a a change in CFLAGS or
# similar caused a change in the generated code without a change in
# the sources.
#
# $1 directory
# $2 source filename w/o extension
# $3 source extension
# $4 regex for egrep -w
clean_obj()
{
	for libcompat in "" $ALL_libcompats; do
		dirprfx=${libcompat:+obj-lib${libcompat}/}
		if strings "$OBJTOP"/$dirprfx$1/$2.*o 2>/dev/null | egrep -qw "${4}"; then
			echo "Removing stale ${libcompat:+lib${libcompat} }objects for $2.$3"
			run rm -fv \
			    "$OBJTOP"/$dirprfx$1/$2.*o
		fi
	done
}

extract_epoch()
{
	[ -s "$1" ] || return 0

	awk 'int($1) > 0 { epoch = $1 } END { print epoch }' "$1"
}

clean_world()
{
	local buildepoch="$1"

	# The caller may set CLEANMK in the environment to make target(s) that
	# should be invoked instead of just destroying everything.  This is
	# generally used after legacy/bootstrap tools to avoid over-cleansing
	# since we're generally in the temporary tree's ancestor.
	if [ -n "$CLEANMK" ]; then
		echo "Cleaning up the object tree"
		run $MAKE -C "$SRCTOP" -f "$SRCTOP"/Makefile.inc1 $CLEANMK
	else
		echo "Cleaning up the temporary build tree"
		run rm -rf "$OBJTOP"
	fi

	# We don't assume that all callers will have grabbed the build epoch, so
	# we'll do it here as needed.  This will be useful if we add other
	# non-epoch reasons to force clean.
	if  [ -z "$buildepoch" ]; then
		buildepoch=$(extract_epoch "$SRCTOP"/.clean_build_epoch)
	fi

	mkdir -p "$OBJTOP"
	echo "$buildepoch" > "$OBJTOP"/.clean_build_epoch

	exit 0
}

check_epoch()
{
	local srcepoch objepoch

	srcepoch=$(extract_epoch "$SRCTOP"/.clean_build_epoch)
	if [ -z "$srcepoch" ]; then
		err "Malformed .clean_build_epoch; please validate the last line"
	fi

	# We don't discriminate between the varying degrees of difference
	# between epochs.  If it went backwards we could be bisecting across
	# epochs, in which case the original need to clean likely still stands.
	objepoch=$(extract_epoch "$OBJTOP"/.clean_build_epoch)
	if [ -z "$objepoch" ] || [ "$srcepoch" -ne "$objepoch" ]; then
		if [ "$VERBOSE" ]; then
			echo "Cleaning - src epoch: $srcepoch, objdir epoch: ${objepoch:-unknown}"
		fi

		clean_world "$srcepoch"
		# NORETURN
	fi
}

check_epoch

#### Typical dependency cleanup begins here.

# Date      Rev      Description

# latest clean epoch (but not pushed until 20250814)
# 20250807	# All OpenSSL-using bits need rebuilt

# Examples from the past, not currently active
#
#Binary program replaced a shell script
# 20220524  68fe988a40ca    kqueue_test binary replaced shell script
#if stat "$OBJTOP"/tests/sys/kqueue/libkqueue/*kqtest* \
#    "$OBJTOP"/tests/sys/kqueue/libkqueue/.depend.kqtest* >/dev/null 2>&1; then
#       echo "Removing old kqtest"
#       run rm -fv "$OBJTOP"/tests/sys/kqueue/libkqueue/.depend.* \
#          "$OBJTOP"/tests/sys/kqueue/libkqueue/*
#fi

# 20250904  aef807876c30    moused binary to directory
if [ -f "$OBJTOP"/usr.sbin/moused/moused ]; then
	echo "Removing old moused binary"
        run rm -fv "$OBJTOP"/usr.sbin/moused/moused
fi

if [ ${MACHINE} = riscv ]; then
	# 20251031  df21a004be23  libc: scalar strrchr() in RISC-V assembly
	clean_dep   lib/libc strrchr c

	# 20251031  563efdd3bd5d  libc: scalar memchr() in RISC-V assembly
	clean_dep   lib/libc memchr c

	# 20251031  40a958d5850d  libc: scalar memset() in RISC-V assembly
	clean_dep   lib/libc memset c

	# 20251031  e09c1583eddd  libc: scalar strlen() in RISC-V assembly
	clean_dep   lib/libc strlen c

	# 20251031  25fdd86a4c92  libc: scalar memcpy() in RISC-V assembly
	clean_dep   lib/libc memcpy c

	# 20251031  5a52f0704435  libc: scalar strnlen() in RISC-V assembly
	clean_dep   lib/libc strnlen c

	# 20251031  08af0bbc9c7d  libc: scalar strchrnul() in RISC-V assembly
	clean_dep   lib/libc strchrnul c

	# 20251031  b5dbf3de5611  libc/riscv64: implement bcopy() and bzero() through memcpy() and memset()
	clean_dep   lib/libc bcopy c "libc.string.bcopy.c"
	clean_dep   lib/libc bzero c "libc.string.bzero.c"
fi
