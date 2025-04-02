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
#   unconditional `rm -f`: there's no need for an extra call to first check for
#   the file's existence.
#
#   # 20250110  3863fec1ce2d  add strlen SIMD implementation
#   clean_dep   lib/libc strlen S arm-optimized-routines
#   run rm -f "$OBJTOP"/lib/libc/strlen.S
#
# A rule may be required for only one architecture:
#
#   # 20220326  fbc002cb72d2    move from bcmp.c to bcmp.S
#   if [ "$MACHINE_ARCH" = "amd64" ]; then
#           clean_dep lib/libc bcmp c
#   fi

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

# 20241025  cb5e41b16083  Unbundle hash functions fom lib/libcrypt
clean_dep   lib/libcrypt crypt-md5    c
clean_dep   lib/libcrypt crypt-nthash c
clean_dep   lib/libcrypt crypt-sha256 c
clean_dep   lib/libcrypt crypt-sha512 c

# 20241213  b55f5e1c4ae3  jemalloc: Move generated jemalloc.3 into lib/libc tree
if [ -h "$OBJTOP"/lib/libc/jemalloc.3 ]; then
	# Have to cleanup the jemalloc.3 in the obj tree since make gets
	# confused and won't use the one in lib/libc/malloc/jemalloc/jemalloc.3
	echo "Removing stale jemalloc.3 object"
	run rm -f "$OBJTOP"/lib/libc/jemalloc.3
fi

if [ $MACHINE_ARCH = aarch64 ]; then
	# 20250110  5e7d93a60440  add strcmp SIMD implementation
	ALL_libcompats= clean_dep   lib/libc strcmp S arm-optimized-routines
	run rm -f "$OBJTOP"/lib/libc/strcmp.S

	# 20250110  b91003acffe7  add strspn optimized implementation
	ALL_libcompats= clean_dep   lib/libc strspn c

	# 20250110  f2bd390a54f1  add strcspn optimized implementation
	ALL_libcompats= clean_dep   lib/libc strcspn c

	# 20250110  89b3872376cb  add optimized strpbrk & strsep implementations
	ALL_libcompats= clean_dep   lib/libc strpbrk c "libc.string.strpbrk.c"

	# 20250110  79287d783c72  strcat enable use of SIMD
	ALL_libcompats= clean_dep   lib/libc strcat c "libc.string.strcat.c"

	# 20250110  756b7fc80837  add strlcpy SIMD implementation
	ALL_libcompats= clean_dep   lib/libc strlcpy c

	# 20250110  25c485e14769  add strncmp SIMD implementation
	ALL_libcompats= clean_dep   lib/libc strncmp S arm-optimized-routines
	run rm -f "$OBJTOP"/lib/libc/strncmp.S

	# 20250110  bad17991c06d  add memccpy SIMD implementation
	ALL_libcompats= clean_dep   lib/libc memccpy c

	# 20250110  3dc5429158cf  add strncat SIMD implementation
	ALL_libcompats= clean_dep   lib/libc strncat c "libc.string.strncat.c"

	# 20250110  bea89d038ac5  add strlcat SIMD implementation, and move memchr
	ALL_libcompats= clean_dep   lib/libc strlcat c "libc.string.strlcat.c"
	ALL_libcompats= clean_dep   lib/libc memchr S "[[:space:]]memchr.S"
	run rm -f "$OBJTOP"/lib/libc/memchr.S

	# 20250110  3863fec1ce2d  add strlen SIMD implementation
	ALL_libcompats= clean_dep   lib/libc strlen S arm-optimized-routines
	run rm -f "$OBJTOP"/lib/libc/strlen.S

	# 20250110  79e01e7e643c  add bcopy & bzero wrapper
	ALL_libcompats= clean_dep   lib/libc bcopy c "libc.string.bcopy.c"
	ALL_libcompats= clean_dep   lib/libc bzero c "libc.string.bzero.c"

	# 20250110  f2c98669fc1b  add ASIMD-enhanced timingsafe_bcmp implementation
	ALL_libcompats= clean_dep   lib/libc timingsafe_bcmp c

	# 20250110  3f224333af16  add timingsafe_memcmp() assembly implementation
	ALL_libcompats= clean_dep   lib/libc timingsafe_memcmp c
fi

# 20250402  839d0755fea8    ctld converted to C++
clean_dep   usr.sbin/ctld   ctld c
clean_dep   usr.sbin/ctld   conf c
clean_dep   usr.sbin/ctld   discovery c
clean_dep   usr.sbin/ctld   isns c
clean_dep   usr.sbin/ctld   kernel c
clean_dep   usr.sbin/ctld   login c
clean_dep   usr.sbin/ctld   uclparse c
