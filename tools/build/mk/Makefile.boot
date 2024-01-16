
CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib

.if ${.MAKE.OS} != "FreeBSD"
# On MacOS using a non-mac ar will fail the build, similarly on Linux using
# nm may not work as expected if the nm for the target architecture comes in
# $PATH before a nm that supports the host architecture.
# To ensure that host binary compile as expected we use the tools from /usr/bin.
AR:=	/usr/bin/ar
RANLIB:=	/usr/bin/ranlib
NM:=	/usr/bin/nm

# Avoid stale dependecy warnings:
LIBC:=
LIBM:=
LIBUTIL:=
LIBCPLUSPLUS:=
LIBARCHIVE:=
LIBPTHREAD:=
LIBMD:=${WORLDTMP}/legacy/usr/lib/libmd.a
LIBNV:=${WORLDTMP}/legacy/usr/lib/libnv.a
LIBSBUF:=${WORLDTMP}/legacy/usr/lib/libsbuf.a
LIBY:=${WORLDTMP}/legacy/usr/lib/liby.a
LIBL:=${WORLDTMP}/legacy/usr/lib/libl.a
LIBROKEN:=${WORLDTMP}/legacy/usr/lib/libroken.a
LIBDWARF:=${WORLDTMP}/legacy/usr/lib/libdwarf.a
LIBELF:=${WORLDTMP}/legacy/usr/lib/libelf.a
LIBZ:=${WORLDTMP}/legacy/usr/lib/libz.a

# Add various -Werror flags to catch missing function declarations
CFLAGS+=	-Werror=implicit-function-declaration -Werror=implicit-int \
		-Werror=return-type -Wundef
CFLAGS+=	-DHAVE_NBTOOL_CONFIG_H=1
# This is needed for code that compiles for pre-C11 C standards
CWARNFLAGS.clang+=-Wno-typedef-redefinition
# bsd.sys.mk explicitly turns on -Wsystem-headers, but that's extremely
# noisy when building on Linux.
CWARNFLAGS+=	-Wno-system-headers
CWARNFLAGS.clang+=-Werror=incompatible-pointer-types-discards-qualifiers

.if ${.MAKE.OS} == "Linux"
CFLAGS+=	-I${SRCTOP}/tools/build/cross-build/include/linux
CFLAGS+=	-D_GNU_SOURCE=1
# Needed for sem_init, etc. on Linux (used by usr.bin/sort)
LDADD+=	-pthread
.if exists(/usr/lib/libfts.so) || exists(/usr/lib/libfts.a) || exists(/lib/libfts.so) || exists(/lib/libfts.a)
# Needed for fts_open, etc. on musl (used by usr.bin/grep)
LDADD+=	-lfts
.endif

.elif ${.MAKE.OS} == "Darwin"
CFLAGS+=	-D_DARWIN_C_SOURCE=1
CFLAGS+=	-I${SRCTOP}/tools/build/cross-build/include/mac
# The macOS ar and ranlib don't understand all the flags supported by the
# FreeBSD and Linux ar/ranlib
ARFLAGS:=	-crs
RANLIBFLAGS:=

# to get libarchive (needed for elftoolchain)
# MacOS ships /usr/lib/libarchive.dylib but doesn't provide the headers
CFLAGS+=	-idirafter ${SRCTOP}/contrib/libarchive/libarchive
.else
.error Unsupported build OS: ${.MAKE.OS}
.endif
.endif # ${.MAKE.OS} != "FreeBSD"

.if ${.MAKE.OS} != "FreeBSD"
# Add the common compatibility headers after the OS-specific ones.
CFLAGS+=	-I${SRCTOP}/tools/build/cross-build/include/common
.endif

# we do not want to capture dependencies referring to the above
UPDATE_DEPENDFILE= no

# When building host tools we should never pull in headers from the source sys
# directory to avoid any ABI issues that might cause the built binary to crash.
# The only exceptions to this are sys/cddl/compat for dtrace bootstrap tools and
# sys/crypto for libmd bootstrap.
# We have to skip this check during make obj since bsd.crunchgen.mk will run
# make obj on every directory during the build-tools phase.
.if !make(obj)
.if !empty(CFLAGS:M*${SRCTOP}/sys)
.error Do not include $${SRCTOP}/sys when building bootstrap tools. \
    Copy the header to $${WORLDTMP}/legacy in tools/build/Makefile instead. \
    Error was caused by Makefile in ${.CURDIR}
.endif

# ${SRCTOP}/include should also never be used to avoid ABI issues
.if !empty(CFLAGS:M*${SRCTOP}/include*)
.error Do not include $${SRCTOP}/include when building bootstrap tools. \
    Copy the header to $${WORLDTMP}/legacy in tools/build/Makefile instead. \
    Error was caused by Makefile in ${.CURDIR}
.endif
.endif

# GCC doesn't allow silencing warn_unused_result calls with (void) casts.
CFLAGS.gcc+=-Wno-unused-result
