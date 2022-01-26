# $FreeBSD$

.PATH: ${.CURDIR}/../../include

LIB=		egacy
SRC=
INCSGROUPS=	INCS SYSINCS CASPERINC UFSINCS FFSINCS MSDOSFSINCS DISKINCS
INCSGROUPS+=	MACHINESYSINCS RPCINCS
INCS=

SYSINCSDIR=	${INCLUDEDIR}/sys
CASPERINCDIR=	${INCLUDEDIR}/casper
# Also add ufs/ffs/msdosfs/disk headers to allow building makefs as a bootstrap tool
UFSINCSDIR=	${INCLUDEDIR}/ufs/ufs
FFSINCSDIR=	${INCLUDEDIR}/ufs/ffs
MSDOSFSINCSDIR=	${INCLUDEDIR}/fs/msdosfs
DISKINCSDIR=	${INCLUDEDIR}/sys/disk
MACHINESYSINCSDIR=	${INCLUDEDIR}/machine
RPCINCSDIR=	${INCLUDEDIR}/rpc

BOOTSTRAPPING?=	0


.if ${.MAKE.OS} == "Darwin"
_XCODE_ROOT!=xcode-select -p
# since macOS 10.14 C headers are no longer installed in /usr but only
# provided via the SDK
.if ${_XCODE_ROOT} == "/Library/Developer/CommandLineTools"
# Only command line tools installed -> host headers are in the SDKs directory
_MACOS_SDK_DIR=${_XCODE_ROOT}/SDKs/MacOSX.sdk/
.else
# Full XCode installed -> host headers are below Platforms/MacOSX.platform
_MACOS_SDK_DIR=${_XCODE_ROOT}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
.endif
HOST_INCLUDE_ROOT=${_MACOS_SDK_DIR}/usr/include
.if !exists(${HOST_INCLUDE_ROOT}/stdio.h)
.error You must install the macOS SDK (try xcode-select --install)
.endif
.else
HOST_INCLUDE_ROOT=/usr/include
.endif

# Allow building libc-internal files (also on non-FreeBSD hosts)
CFLAGS+=	-I${.CURDIR}/libc-bootstrap
# Symbol versioning is not required for -legacy (and macOS bootstrap)
MK_SYMVER=	no

_WITH_PWCACHEDB!= grep -c pwcache_groupdb ${HOST_INCLUDE_ROOT}/grp.h || true
.if ${_WITH_PWCACHEDB} == 0
.PATH: ${.CURDIR}/../../contrib/libc-pwcache
CFLAGS.pwcache.c+=	-I${.CURDIR}/../../contrib/libc-pwcache
SRCS+=		pwcache.c
.endif

_WITH_STRSVIS!=	grep -c strsvis ${HOST_INCLUDE_ROOT}/vis.h 2>/dev/null || true
.if ${_WITH_STRSVIS} == 0
.PATH: ${.CURDIR}/../../contrib/libc-vis
INCS+=		vis.h
SRCS+=		vis.c unvis.c
CFLAGS.vis.c+=	-I${.CURDIR}/../../contrib/libc-vis -DHAVE_VIS=0 -DHAVE_SVIS=0
CFLAGS.unvis.c+=	-I${.CURDIR}/../../contrib/libc-vis -DHAVE_VIS=0 -DHAVE_SVIS=0
.endif

_WITH_REALLOCARRAY!= grep -c reallocarray ${HOST_INCLUDE_ROOT}/stdlib.h || true
.if ${_WITH_REALLOCARRAY} == 0
.PATH: ${.CURDIR}/../../lib/libc/stdlib
INCS+=		stdlib.h
SRCS+=		reallocarray.c
.endif

_WITH_UTIMENS!= grep -c utimensat ${HOST_INCLUDE_ROOT}/sys/stat.h || true
.if ${_WITH_UTIMENS} == 0
SYSINCS+=	stat.h
SRCS+=		futimens.c utimensat.c
.endif

_WITH_EXPLICIT_BZERO!= grep -c explicit_bzero ${HOST_INCLUDE_ROOT}/strings.h || true
.if ${_WITH_EXPLICIT_BZERO} == 0
# .PATH: ${SRCTOP}/sys/libkern
# Adding sys/libkern to .PATH breaks building the cross-build compat library
# since that attempts to build strlcpy.c from libc and adding libkern here will
# cause it to pick the file from libkern instead (which won't compile).
# Avoid modifying .PATH by creating a copy in the build directory instead.
explicit_bzero.c: ${SRCTOP}/sys/libkern/explicit_bzero.c
	cp ${.ALLSRC} ${.TARGET}
CLEANFILES+=	explicit_bzero.c
INCS+=		strings.h
SRCS+=		explicit_bzero.c
.endif

_WITH_FSPACECTL!= grep -c fspacectl ${HOST_INCLUDE_ROOT}/fcntl.h || true
.if ${_WITH_FSPACECTL} == 0
INCS+=		fcntl.h
SRCS+=		fspacectl.c
.endif

.if exists(${HOST_INCLUDE_ROOT}/capsicum_helpers.h)
_WITH_CAPH_ENTER!= grep -c caph_enter ${HOST_INCLUDE_ROOT}/capsicum_helpers.h || true
_WITH_CAPH_RIGHTS_LIMIT!= grep -c caph_rights_limit ${HOST_INCLUDE_ROOT}/capsicum_helpers.h || true
.endif
.if !defined(_WITH_CAPH_ENTER) || ${_WITH_CAPH_ENTER} == 0 || ${_WITH_CAPH_RIGHTS_LIMIT} == 0
.PATH: ${SRCTOP}/lib/libcapsicum
INCS+=		capsicum_helpers.h
.PATH: ${SRCTOP}/lib/libcasper/libcasper
INCS+=		libcasper.h
.endif

# rpcgen should build against the source tree rpc/types.h and not the host.
# This is especially important on non-FreeBSD systems where the types may
# not match.
RPCINCS+=	${SRCTOP}/sys/rpc/types.h

INCS+=	${SRCTOP}/include/mpool.h
INCS+=	${SRCTOP}/include/ndbm.h
INCS+=	${SRCTOP}/include/err.h
INCS+=	${SRCTOP}/include/stringlist.h

# Needed to build arc4random.c
INCSGROUPS+=	CHACHA20INCS
CHACHA20INCSDIR=	${INCLUDEDIR}/crypto/chacha20
CHACHA20INCS+=	${SRCTOP}/sys/crypto/chacha20/_chacha.h \
	${SRCTOP}/sys/crypto/chacha20/chacha.h

_host_arch=${MACHINE}
.if ${_host_arch} == "x86_64"
# bmake on Linux/mac often prints that instead of amd64
_host_arch=amd64
.elif ${_host_arch} == "aarch64"
# Linux calls arm64, aarch64, across the board
_host_arch=arm64
.elif ${_host_arch:Mppc*}
_host_arch=powerpc
.endif
.if ${_host_arch} == "unknown"
# HACK: If MACHINE is unknown, assume we are building on x86
_host_arch=amd64
.endif
MACHINESYSINCS+=	${SRCTOP}/sys/${_host_arch}/include/elf.h
.if ${_host_arch} == "amd64" || ${_host_arch} == "i386"
INCSGROUPS+=	X86INCS
X86INCSDIR=	${INCLUDEDIR}/x86
X86INCS+=	${SRCTOP}/sys/x86/include/elf.h
.endif

# needed for btxld:
MACHINESYSINCS+=	${SRCTOP}/sys/${_host_arch}/include/exec.h
MACHINESYSINCS+=	${SRCTOP}/sys/${_host_arch}/include/reloc.h
INCS+=	${SRCTOP}/include/a.out.h
INCS+=	${SRCTOP}/include/nlist.h
SYSINCS+=	${SRCTOP}/sys/sys/imgact_aout.h
SYSINCS+=	${SRCTOP}/sys/sys/nlist_aout.h


.if ${.MAKE.OS} != "FreeBSD"
.PATH: ${.CURDIR}/cross-build
# dbopen() behaves differently on Linux and FreeBSD so we ensure that we
# bootstrap the FreeBSD db code. The cross-build headers #define dbopen() to
# __freebsd_dbopen() so that we don't ever use the host version
INCS+=	${SRCTOP}/include/db.h
LIBC_SRCTOP=	${SRCTOP}/lib/libc/
.include "${LIBC_SRCTOP}/db/Makefile.inc"
# Do the same as we did for dbopen() for getopt() on since it's not compatible
# on Linux (and to avoid surprises also compile the FreeBSD code on macOS)
.PATH: ${LIBC_SRCTOP}/stdlib
SRCS+=	getopt.c getopt_long.c
INCS+=	 ${SRCTOP}/include/getopt.h

# getcap.c is needed for cap_mkdb:
.PATH: ${LIBC_SRCTOP}/gen
SRCS+=	getcap.c
# Glibc does not provide all err*/warn* functions, and for macOS we need the
# alias with the extra underscore.
SRCS+=	err.c
# Add various libbc functions that are not available in glibc:
SRCS+=	stringlist.c setmode.c
SRCS+=	strtonum.c merge.c heapsort.c reallocf.c
.PATH: ${LIBC_SRCTOP}/locale
SRCS+=	rpmatch.c

.if ${.MAKE.OS} == "Linux"
# On Linux, glibc does not provide strlcpy,strlcat or strmode.
.PATH: ${LIBC_SRCTOP}/string
SRCS+=	strlcpy.c strlcat.c strmode.c
# Compile the fgetln/fgetwln/closefrom fallback code from libbsd:
SRCS+=	fgetln_fallback.c fgetwln_fallback.c closefrom.c
CFLAGS.closefrom.c+=	-DSTDC_HEADERS -DHAVE_SYS_DIR_H -DHAVE_DIRENT_H \
	-DHAVE_DIRFD -DHAVE_SYSCONF
# Provide getprogname/setprograme
SRCS+=	progname.c
# Stub implementations of fflagstostr/strtofflags
SRCS+=	fflags.c
.endif
# Provide the same arc4random implementation on Linux/macOS
CFLAGS.arc4random.c+=	-I${SRCTOP}/sys/crypto/chacha20 -D__isthreaded=1
SRCS+=	arc4random.c arc4random_uniform.c

# expand_number() is not provided by either Linux or MacOS libutil
.PATH: ${SRCTOP}/lib/libutil
SRCS+=	expand_number.c
# Linux libutil also doesn't have fparseln
SRCS+=	fparseln.c
# A dummy sysctl for tzsetup:
SRCS+=	fake_sysctl.c

# capsicum support
SYSINCS+=	${SRCTOP}/sys/sys/capsicum.h
SYSINCS+=	${SRCTOP}/sys/sys/caprights.h
SRCS+=	capsicum_stubs.c
# XXX: we can't add ${SRCTOP}/sys/kern to .PATH since that will causes
# conflicts with other files. Instead copy subr_capability to the build dir.
subr_capability.c: ${SRCTOP}/sys/kern/subr_capability.c
	cp ${.ALLSRC} ${.TARGET}
SRCS+=	subr_capability.c
CLEANFILES+=	subr_capability.c
.endif

CASPERINC+=	${SRCTOP}/lib/libcasper/services/cap_fileargs/cap_fileargs.h
CASPERINC+=	${SRCTOP}/lib/libcasper/services/cap_net/cap_net.h

.if empty(SRCS)
SRCS=		dummy.c
.endif

.if defined(CROSS_BUILD_TESTING)
SUBDIR=		cross-build
.endif

# To allow bootstrapping makefs on FreeBSD 11 or non-FreeBSD systems:
UFSINCS+=	${SRCTOP}/sys/ufs/ufs/dinode.h
UFSINCS+=	${SRCTOP}/sys/ufs/ufs/dir.h
FFSINCS+=	${SRCTOP}/sys/ufs/ffs/fs.h

MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/bootsect.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/bpb.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/denode.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/direntry.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/fat.h
MSDOSFSINCS+=	${SRCTOP}/sys/fs/msdosfs/msdosfsmount.h
DISKINCS+=	${SRCTOP}/sys/sys/disk/bsd.h

# Needed to build config (since it uses libnv)
SYSINCS+=	${SRCTOP}/sys/sys/nv.h ${SRCTOP}/sys/sys/cnv.h \
		${SRCTOP}/sys/sys/dnv.h

# Needed when bootstrapping ldd (since it uses DF_1_PIE)
SYSINCS+=	${SRCTOP}/sys/sys/elf32.h
SYSINCS+=	${SRCTOP}/sys/sys/elf64.h
SYSINCS+=	${SRCTOP}/sys/sys/elf_common.h
SYSINCS+=	${SRCTOP}/sys/sys/elf_generic.h
SYSINCS+=	${SRCTOP}/sys/sys/queue.h
SYSINCS+=	${SRCTOP}/sys/sys/md5.h
SYSINCS+=	${SRCTOP}/sys/sys/sbuf.h
SYSINCS+=	${SRCTOP}/sys/sys/tree.h

# vtfontcvt is using sys/font.h
SYSINCS+=	${SRCTOP}/sys/sys/font.h
# For mkscrfil.c:
SYSINCS+=	${SRCTOP}/sys/sys/consio.h
# for gencat:
INCS+=	${SRCTOP}/include/nl_types.h
# for vtfontcvt:
SYSINCS+=	${SRCTOP}/sys/sys/fnv_hash.h
# opensolaris compatibility
INCS+=	${SRCTOP}/include/elf.h
SYSINCS+=	${SRCTOP}/sys/sys/elf.h

# We want to run the build with only ${WORLDTMP} in $PATH to ensure we don't
# accidentally run tools that are incompatible but happen to be in $PATH.
# This is especially important when building on Linux/MacOS where many of the
# programs used during the build accept different flags or generate different
# output. On those platforms we only symlink the tools known to be compatible
# (e.g. basic utilities such as mkdir) into ${WORLDTMP} and build all others
# from the FreeBSD sources during the bootstrap-tools stage.

# basic commands: It is fine to use the host version for all of these even on
# Linux/MacOS since we only use flags that are supported by all of them.
_host_tools_to_symlink=	basename bzip2 bunzip2 chmod chown cmp comm cp date dd \
	dirname echo env false find fmt gzip gunzip head hostname id ln ls \
	mkdir mv nice patch rm sh sleep stat tee time touch tr true uname uniq \
	unxz wc which xz

# We also need a symlink to the absolute path to the make binary used for
# the toplevel makefile. This is not necessarily the same as `which make`
# since e.g. on Linux and MacOS that will be GNU make.
_make_abs!=	which "${MAKE}"
_host_abs_tools_to_symlink=	${_make_abs}:make ${_make_abs}:bmake

.if ${.MAKE.OS} == "FreeBSD"
# When building on FreeBSD we always copy the host tools instead of linking
# into WORLDTMP to avoid issues with incompatible libraries (see r364030).
# Note: we could create links if we don't intend to update the current machine.
_COPY_HOST_TOOL=cp -pf
.else
# However, this is not necessary on Linux/macOS. Additionally, copying the host
# tools to another directory with cp -p results in freezes on macOS Big Sur for
# some unknown reason. It can also break building inside docker containers if
# there are ACLs on shared volumes.
_COPY_HOST_TOOL=ln -sfn
_make_abs!=	which "${MAKE}"
_host_abs_tools_to_symlink+=	${_make_abs}:make ${_make_abs}:bmake
.if ${.MAKE.OS} == "Darwin"
# /usr/bin/cpp may invoke xcrun:
_host_tools_to_symlink+=xcrun
.endif  # ${.MAKE.OS} == "Darwin"
# On Ubuntu /bin/sh is dash which is totally useless. Let's just link bash
# as the build sh since that will work fine.
_host_abs_tools_to_symlink+=	/bin/bash:sh
_host_tools_to_symlink:=${_host_tools_to_symlink:Nsh}
.endif

host-symlinks:
	@echo "Linking host tools into ${DESTDIR}/bin"
.for _tool in ${_host_tools_to_symlink}
	@export PATH=$${PATH}:/usr/local/bin; \
	source_path=`which ${_tool} || echo /dev/null/no/such`; \
	if [ ! -e "$${source_path}" ] ; then \
		echo "Cannot find host tool '${_tool}' in PATH ($$PATH)." >&2; false; \
	fi; \
	rm -f "${DESTDIR}/bin/${_tool}"; \
	${_COPY_HOST_TOOL} "$${source_path}" "${DESTDIR}/bin/${_tool}"
.endfor
.for _tool in ${_host_abs_tools_to_symlink}
	@source_path="${_tool:S/:/ /:[1]}"; \
	target_path="${DESTDIR}/bin/${_tool:S/:/ /:[2]}"; \
	if [ ! -e "$${source_path}" ] ; then \
		echo "Host tool '$${source_path}' is missing"; false; \
	fi; \
	rm -f "$${target_path}"; \
	${_COPY_HOST_TOOL} "$${source_path}" "$${target_path}"
.endfor
.if exists(/usr/libexec/flua)
	rm -f ${DESTDIR}/usr/libexec/flua
	${_COPY_HOST_TOOL} /usr/libexec/flua ${DESTDIR}/usr/libexec/flua
.endif

# Create all the directories that are needed during the legacy, bootstrap-tools
# and cross-tools stages. We do this here using mkdir since mtree may not exist
# yet (this happens if we are crossbuilding from Linux/Mac).
INSTALLDIR_LIST= \
	bin \
	lib/casper \
	lib/geom \
	usr/include/casper \
	usr/include/private/ucl \
	usr/include/private/zstd \
	usr/lib \
	usr/libdata/pkgconfig \
	usr/libexec

installdirs:
	mkdir -p ${INSTALLDIR_LIST:S,^,${DESTDIR}/,}

# Link usr/bin, sbin, and usr/sbin to bin so that it doesn't matter whether a
# bootstrap tool was added to WORLTMP with a symlink or by building it in the
# bootstrap-tools phase. We could also overrride BINDIR when building bootstrap
# tools but adding the symlinks is easier and means all tools are also
# in the directory that they are installed to normally.

.for _dir in sbin usr/sbin usr/bin
# delete existing directories from before r340157
	@if [ -e ${DESTDIR}/${_dir} ] && [ ! -L ${DESTDIR}/${_dir} ]; then \
	    echo "removing old non-symlink ${DESTDIR}/${_dir}"; \
	    rm -rf "${DESTDIR}/${_dir}"; \
	fi
.endfor
	ln -sfn bin ${DESTDIR}/sbin
	ln -sfn ../bin ${DESTDIR}/usr/bin
	ln -sfn ../bin ${DESTDIR}/usr/sbin
.for _group in ${INCSGROUPS:NINCS}
	mkdir -p "${DESTDIR}/${${_group}DIR}"
.endfor

.include <bsd.lib.mk>
