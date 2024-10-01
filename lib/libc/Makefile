PACKAGE=	clibs
SHLIBDIR?= /lib

.include <src.opts.mk>

LIBC_SRCTOP?= ${.CURDIR}
LIBSYS_SRCTOP?=	${.CURDIR:H}/libsys

# Pick the current architecture directory for libc. In general, this is named
# MACHINE_CPUARCH, but some ABIs are different enough to require their own libc,
# so allow a directory named MACHINE_ARCH to override this (though treat
# powerpc64le and powerpc64 the same).
# Note: This is copied to msun/Makefile
M=${MACHINE_ARCH:S/powerpc64le/powerpc64/}
.if exists(${LIBC_SRCTOP}/${M})
LIBC_ARCH=${M}
.else
LIBC_ARCH=${MACHINE_CPUARCH}
.endif

CFLAGS+=-D_FORTIFY_SOURCE_read=_read

# All library objects contain FreeBSD revision strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, add -DSTRIP_FBSDID (see <sys/cdefs.h>) to CFLAGS
# below.  Note: there are no IDs for syscall stubs whose sources are generated.
# To include legacy CSRG SCCS ID strings, remove -DNO__SCCSID from CFLAGS.
# To include RCS ID strings from other BSD projects, remove -DNO__RCSID from CFLAGS.
CFLAGS+=-DNO__SCCSID -DNO__RCSID

LIB=c
SHLIB_MAJOR= 7
.if ${MK_SSP} != "no" && \
    (${LIBC_ARCH} == "i386" || ${LIBC_ARCH:Mpowerpc*} != "")
SHLIB_LDSCRIPT=libc.ldscript
.else
SHLIB_LDSCRIPT=libc_nossp.ldscript
.endif
SHLIB_LDSCRIPT_LINKS=libxnet.so
WARNS?=	2
CFLAGS+=-I${LIBC_SRCTOP}/include -I${SRCTOP}/include
CFLAGS+=-I${LIBC_SRCTOP}/${LIBC_ARCH}
CFLAGS+=-I${LIBSYS_SRCTOP}/${LIBC_ARCH}
.if ${MK_NLS} != "no"
CFLAGS+=-DNLS
.endif
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=
BUILD_NOSSP_PIC_ARCHIVE=
PRECIOUSLIB=

.ifndef NO_THREAD_STACK_UNWIND
CANCELPOINTS_CFLAGS=-fexceptions
CFLAGS+=${CANCELPOINTS_CFLAGS}
.endif

# Use a more efficient TLS model for libc since we can reasonably assume that
# it will be loaded during program startup.
CFLAGS+= -ftls-model=initial-exec

#
# Link with static libcompiler_rt.a.
#
LDFLAGS+= -nodefaultlibs
LIBADD+=	compiler_rt
LIBADD+=	sys
.if defined(COMPAT_libcompat)
# XXX: work around gcc -m32 + bfd ld path issue for DT_NEEDED symbols.
# https://sourceware.org/bugzilla/show_bug.cgi?id=31395
LDFLAGS+=	-Wl,--rpath=/usr/lib${COMPAT_libcompat}
.endif

LDFLAGS+=-Wl,--auxiliary,libsys.so.7

.if ${MK_SSP} != "no" && \
    (${LIBC_ARCH} == "i386" || ${LIBC_ARCH:Mpowerpc*} != "")
LIBADD+=	ssp_nonshared
.endif

# Extras that live in either libc.a or libc_nonshared.a
LIBC_NONSHARED_SRCS=

RTLD_ELF_DIR=${SRCTOP}/libexec/rtld-elf
.if exists(${RTLD_ELF_DIR}/${MACHINE_ARCH:S/powerpc64le/powerpc64/})
RTLD_ARCH=	${MACHINE_ARCH:S/powerpc64le/powerpc64/}
.else
RTLD_ARCH=	${MACHINE_CPUARCH}
.endif
RTLD_HDRS= -I${RTLD_ELF_DIR}/${RTLD_ARCH} -I${RTLD_ELF_DIR}

# Define (empty) variables so that make doesn't give substitution
# errors if the included makefiles don't change these:
MDSRCS=
MISRCS=
MDASM=
MIASM=
NOASM=

.include "${LIBC_SRCTOP}/${LIBC_ARCH}/Makefile.inc"
.include "${LIBC_SRCTOP}/csu/Makefile.inc"
.include "${LIBC_SRCTOP}/db/Makefile.inc"
.include "${LIBC_SRCTOP}/compat-43/Makefile.inc"
.include "${LIBC_SRCTOP}/gdtoa/Makefile.inc"
.include "${LIBC_SRCTOP}/gen/Makefile.inc"
.include "${LIBC_SRCTOP}/gmon/Makefile.inc"
.if ${MK_ICONV} != "no"
.include "${LIBC_SRCTOP}/iconv/Makefile.inc"
.endif
.include "${LIBC_SRCTOP}/inet/Makefile.inc"
.include "${LIBC_SRCTOP}/isc/Makefile.inc"
.include "${LIBC_SRCTOP}/locale/Makefile.inc"
.include "${LIBC_SRCTOP}/md/Makefile.inc"
.include "${LIBC_SRCTOP}/nameser/Makefile.inc"
.include "${LIBC_SRCTOP}/net/Makefile.inc"
.include "${LIBC_SRCTOP}/nls/Makefile.inc"
.include "${LIBC_SRCTOP}/posix1e/Makefile.inc"
.if ${MACHINE_ABI:Mlong32}
.include "${LIBC_SRCTOP}/quad/Makefile.inc"
.endif
.include "${LIBC_SRCTOP}/regex/Makefile.inc"
.include "${LIBC_SRCTOP}/resolv/Makefile.inc"
.include "${LIBC_SRCTOP}/stdio/Makefile.inc"
.include "${LIBC_SRCTOP}/stdlib/Makefile.inc"
.include "${LIBC_SRCTOP}/stdtime/Makefile.inc"
.include "${LIBC_SRCTOP}/string/Makefile.inc"
.include "${LIBC_SRCTOP}/sys/Makefile.inc"
.include "${LIBC_SRCTOP}/secure/Makefile.inc"
.include "${LIBC_SRCTOP}/rpc/Makefile.inc"
.include "${LIBC_SRCTOP}/uuid/Makefile.inc"
.include "${LIBC_SRCTOP}/xdr/Makefile.inc"
.if (${LIBC_ARCH} == "arm" && (defined(CPUTYPE) && ${CPUTYPE:M*soft*}))
.include "${LIBC_SRCTOP}/softfloat/Makefile.inc"
.endif
.if ${LIBC_ARCH} == "i386" || ${LIBC_ARCH} == "amd64"
.include "${LIBC_SRCTOP}/x86/gen/Makefile.inc"
.endif
.if ${MK_NIS} != "no"
CFLAGS+= -DYP
.include "${LIBC_SRCTOP}/yp/Makefile.inc"
.endif
.include "${LIBC_SRCTOP}/capability/Makefile.inc"
.if ${MK_HESIOD} != "no"
CFLAGS+= -DHESIOD
.endif
.if ${MK_FP_LIBC} == "no"
CFLAGS+= -DNO_FLOATING_POINT
.endif
.if ${MK_NS_CACHING} != "no"
CFLAGS+= -DNS_CACHING
.endif
.if defined(_FREEFALL_CONFIG)
CFLAGS+=-D_FREEFALL_CONFIG
.endif

STATICOBJS+=${LIBC_NONSHARED_SRCS:S/.c$/.o/}

VERSION_DEF=${LIBC_SRCTOP}/Versions.def
SYMBOL_MAPS=${SYM_MAPS}

# If there are no machine dependent sources, append all the
# machine-independent sources:
.if empty(MDSRCS) || ${MK_MACHDEP_OPTIMIZATIONS} == no
SRCS+=	${MISRCS}
.else
# Append machine-dependent sources, then append machine-independent sources
# for which there is no machine-dependent variant.
SRCS+=	${MDSRCS}
.for _src in ${MISRCS}
.if ${MDSRCS:R:M${_src:R}} == ""
SRCS+=	${_src}
.endif
.endfor
.endif

KQSRCS=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
KSRCS=	bcmp.c ffs.c ffsl.c fls.c flsl.c mcount.c strcat.c strchr.c \
	strcmp.c strcpy.c strlen.c strncpy.c strrchr.c

libkern: libkern.gen libkern.${LIBC_ARCH}

libkern.gen: ${KQSRCS} ${KSRCS}
	${CP} ${LIBC_SRCTOP}/quad/quad.h ${.ALLSRC} ${DESTDIR}/sys/libkern

libkern.${LIBC_ARCH}:: ${KMSRCS}
.if defined(KMSRCS) && !empty(KMSRCS)
	${CP} ${.ALLSRC} ${DESTDIR}/sys/libkern/${LIBC_ARCH}
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>

.if (${LIBC_ARCH} == amd64 || ${LIBC_ARCH} == i386) && \
    ${.TARGETS:Mall} == all && \
    defined(LINKER_FEATURES) && ${LINKER_FEATURES:Mifunc} == ""
.error ${LIBC_ARCH} libc requires linker ifunc support
.endif

.if !defined(_SKIP_BUILD)
# We need libutil.h, get it directly to avoid
# recording a build dependency
CFLAGS+= -I${SRCTOP}/lib/libutil
# Same issue with libm
MSUN_ARCH_SUBDIR != ${MAKE} -B -C ${SRCTOP}/lib/msun -V ARCH_SUBDIR
# unfortunately msun/src contains both private and public headers
CFLAGS+= -I${SRCTOP}/lib/msun/${MSUN_ARCH_SUBDIR}
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS+= -I${SRCTOP}/lib/msun/x86
.endif
CFLAGS+= -I${SRCTOP}/lib/msun/src
# and we do not want to record a dependency on msun
.if ${.MAKE.LEVEL} > 0
GENDIRDEPS_FILTER+= N${RELDIR:H}/msun
.endif
.endif

# Disable warnings in contributed sources.
CWARNFLAGS:=	${.IMPSRC:Ngdtoa_*.c:C/^.+$/${CWARNFLAGS}/:C/^$/-w/}
# Disable stack protection for SSP symbols.
SSP_CFLAGS:=	${.IMPSRC:N*/libc_stack_protector.c:C/^.+$/${SSP_CFLAGS}/}
# Generate stack unwinding tables for cancellation points
CANCELPOINTS_CFLAGS:=	${.IMPSRC:Mcancelpoints_*:C/^.+$/${CANCELPOINTS_CFLAGS}/:C/^$//}
