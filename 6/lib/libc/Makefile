#	@(#)Makefile	8.2 (Berkeley) 2/3/94
# $FreeBSD$
#
# All library objects contain FreeBSD revision strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, add -DSTRIP_FBSDID (see <sys/cdefs.h>) to CFLAGS
# below.  Note: there are no IDs for syscall stubs whose sources are generated.
# To include legacy CSRG sccsid strings, add -DLIBC_SCCS and -DSYSLIBC_SCCS
# to CFLAGS below.  -DSYSLIBC_SCCS affects just the system call stubs.
LIB=c
# If you bump SHLIB_MAJOR, remove the kluge from gen/gethostname.c.
# If you bump SHLIB_MAJOR, see standards/55112.
SHLIB_MAJOR= 6
SHLIBDIR?= /lib
WARNS?=	2
CFLAGS+=-I${.CURDIR}/include -I${.CURDIR}/../../include
CFLAGS+=-I${.CURDIR}/${MACHINE_ARCH}
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=
PRECIOUSLIB=

# Define (empty) variables so that make doesn't give substitution
# errors if the included makefiles don't change these:
MDSRCS=
MISRCS=
MDASM=
MIASM=
NOASM=

.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/compat-43/Makefile.inc"
.include "${.CURDIR}/gdtoa/Makefile.inc"
.include "${.CURDIR}/gen/Makefile.inc"
.include "${.CURDIR}/gmon/Makefile.inc"
.include "${.CURDIR}/inet/Makefile.inc"
.include "${.CURDIR}/isc/Makefile.inc"
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/nameser/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.include "${.CURDIR}/nls/Makefile.inc"
.include "${.CURDIR}/posix1e/Makefile.inc"
.if !defined(NO_QUAD)
.include "${.CURDIR}/quad/Makefile.inc"
.endif
.include "${.CURDIR}/regex/Makefile.inc"
.include "${.CURDIR}/resolv/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdlib/Makefile.inc"
.include "${.CURDIR}/stdtime/Makefile.inc"
.include "${.CURDIR}/string/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/rpc/Makefile.inc"
.include "${.CURDIR}/uuid/Makefile.inc"
.include "${.CURDIR}/xdr/Makefile.inc"
.if ${MACHINE_ARCH} == "arm"
.include "${.CURDIR}/softfloat/Makefile.inc"
.endif
.if !defined(NO_NIS)
CFLAGS+= -DYP
.include "${.CURDIR}/yp/Makefile.inc"
.endif
.if !defined(NO_HESIOD_LIBC)
CFLAGS+= -DHESIOD
.endif
.if defined(NO_FP_LIBC)
CFLAGS+= -DNO_FLOATING_POINT
.endif
.if defined(_FREEFALL_CONFIG)
CFLAGS+=-D_FREEFALL_CONFIG
.endif

# If there are no machine dependent sources, append all the
# machine-independent sources:
.if empty(MDSRCS)
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
KSRCS=	bcmp.c ffs.c ffsl.c fls.c flsl.c index.c mcount.c rindex.c \
	strcat.c strcmp.c strcpy.c strlen.c strncpy.c

libkern: libkern.gen libkern.${MACHINE_ARCH}

libkern.gen: ${KQSRCS} ${KSRCS}
	cp -p ${.CURDIR}/quad/quad.h ${.ALLSRC} ${DESTDIR}/sys/libkern

libkern.${MACHINE_ARCH}:: ${KMSRCS}
.if defined(KMSRCS) && !empty(KMSRCS)
	cp -p ${.ALLSRC} ${DESTDIR}/sys/libkern/${MACHINE_ARCH}
.endif

.include <bsd.lib.mk>

# Disable warnings in contributed sources.
CWARNFLAGS:=	${.IMPSRC:Ngdtoa_*.c:C/^.+$/${CWARNFLAGS}/}
