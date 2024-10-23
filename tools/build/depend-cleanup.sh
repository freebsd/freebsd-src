#!/bin/sh
#
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
			run rm -f \
			    "$OBJTOP"/$dirprfx$1/.depend.$2.* \
			    "$OBJTOP"/$dirprfx$1/$2.*o
		fi
	done
}

# Date      Rev      Description

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
	for libcompat in "" $ALL_libcompats; do
		dirprfx=${libcompat:+obj-lib${libcompat}/}
		run rm -rf "$OBJTOP"/${dirprfx}secure/lib/libcrypto \
		    "$OBJTOP"/${dirprfx}secure/lib/libssl
	done
fi

# 20230714  ee8b0c436d72    replace ffs/fls implementations with clang builtins
clean_dep   lib/libc        ffs   S
clean_dep   lib/libc        ffsl  S
clean_dep   lib/libc        ffsll S
clean_dep   lib/libc        fls   S
clean_dep   lib/libc        flsl  S
clean_dep   lib/libc        flsll S

# 20230815  28f6c2f29280    GoogleTest update
if [ -e "$OBJTOP"/tests/sys/fs/fusefs/mockfs.o ] && \
    grep -q '_ZN7testing8internal18g_linked_ptr_mutexE' "$OBJTOP"/tests/sys/fs/fusefs/mockfs.o; then
	echo "Removing stale fusefs GoogleTest objects"
	run rm -rf "$OBJTOP"/tests/sys/fs/fusefs
fi

# 20231031  0527c9bdc718    Remove forward compat ino64 stuff
clean_dep   lib/libc        fstat         c
clean_dep   lib/libc        fstatat       c
clean_dep   lib/libc        fstatfs       c
clean_dep   lib/libc        getdirentries c
clean_dep   lib/libc        getfsstat     c
clean_dep   lib/libc        statfs        c

# 20240308  e6ffc7669a56    Remove pointless MD syscall(2)
# 20240308  0ee0ae237324    Remove pointless MD syscall(2)
# 20240308  7b3836c28188    Remove pointless MD syscall(2)
if [ ${MACHINE} != i386 ]; then
	libcompats=
	for libcompat in $ALL_libcompats; do
		if [ $MACHINE = amd64 ] && [ $libcompat = 32 ]; then
			continue
		fi
		libcompats="${libcompats+$libcompats }$libcompat"
	done
	ALL_libcompats="$libcompats" clean_dep   lib/libsys  syscall S ".*/syscall\.S"
	ALL_libcompats="$libcompats" clean_dep   lib/libc    syscall S ".*/syscall\.S"
fi

# 20240416  2fda3ab0ac19    WITH_NVME: Remove from broken
if [ -f "$OBJTOP"/rescue/rescue/rescue.mk ] && \
    ! grep -q 'nvme_util.o' "$OBJTOP"/rescue/rescue/rescue.mk; then
	echo "removing rescue.mk without nvme_util.o"
	run rm -f "$OBJTOP"/rescue/rescue/rescue.mk
fi

# 20240910  e2df9bb44109
clean_dep   cddl/lib/libzpool abd_os c "linux/zfs/abd_os\.c"

# 20241007
clean_dep   cddl/lib/libzpool zfs_debug c "linux/zfs/zfs_debug\.c"

# 20241011
clean_dep   cddl/lib/libzpool arc_os c "linux/zfs/arc_os\.c"

# 20241018  1363acbf25de    libc/csu: Support IFUNCs on riscv
if [ ${MACHINE} = riscv ]; then
	for f in "$OBJTOP"/lib/libc/.depend.libc_start1.*o; do
		if [ ! -f "$f" ]; then
			continue
		fi
		if ! grep -q 'lib/libc/csu/riscv/reloc\.c' "$f"; then
			echo "Removing stale dependencies and objects for libc_start1.c"
			run rm -f \
			    "$OBJTOP"/lib/libc/.depend.libc_start1.* \
			    "$OBJTOP"/lib/libc/libc_start1.*o
			break
		fi
	done
fi

# 20241018  5deeebd8c6ca   Merge llvm-project release/19.x llvmorg-19.1.2-0-g7ba7d8e2f7b6
p="$OBJTOP"/lib/clang/libclang/clang/Basic
f="$p"/arm_mve_builtin_sema.inc
if [ -e "$f" ]; then
	if grep -q SemaBuiltinConstantArgRange "$f"; then
		echo "Removing pre-llvm19 clang-tblgen output"
		run rm -f "$p"/*.inc
	fi
fi
