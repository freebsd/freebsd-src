# $FreeBSD$


# BINDIR
.include "${.CURDIR}/../../Makefile.inc"
WARNS?=	2
NO_WERROR=
.include "${.CURDIR}/../Makefile.inc0"

.PATH: ${SRCDIR}/gas ${SRCDIR}/gas/config

PROG=	as
MAN=	doc/as.1
SRCS+=	app.c as.c atof-generic.c atof-ieee.c bignum-copy.c \
	cond.c dwarf2dbg.c ecoff.c expr.c flonum-copy.c flonum-konst.c \
	flonum-mult.c frags.c hash.c input-file.c \
	input-scrub.c listing.c literal.c macro.c messages.c \
	obj-elf.c output-file.c read.c sb.c stabs.c \
	subsegs.c symbols.c write.c depend.c ehopt.c dw2gencfi.c
# DEO: why not used?
#SRCS+=	itbl-ops.c

.if ${TARGET_ARCH} == "amd64"
SRCS+=	tc-i386.c
.elif ${TARGET_ARCH} == "powerpc"
SRCS+=	tc-ppc.c
.elif ${TARGET_ARCH} == "sparc64"
SRCS+=	tc-sparc.c
.else
SRCS+=	tc-${TARGET_ARCH}.c
.endif

.if ${TARGET_ARCH} == "sparc64"
CFLAGS+= -DDEFAULT_ARCH=\"v9-64\"
.else
CFLAGS+= -DDEFAULT_ARCH=\"${BINUTILS_ARCH}\"
.endif
CFLAGS+= -DTARGET_CPU=\"${BINUTILS_ARCH}\"
CFLAGS+= -DTARGET_CANONICAL=\"${TARGET_TUPLE}\"
CFLAGS+= -DTARGET_ALIAS=\"${TARGET_TUPLE}\"
CFLAGS+= -DVERSION=\"${VERSION}\"
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${SRCDIR}/gas -I${SRCDIR}/gas/config -I${SRCDIR}
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/${TARGET_ARCH}-freebsd

NOSHARED?=	yes

DPADD=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
DPADD+=	${RELTOP}/libopcodes/libopcodes.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
