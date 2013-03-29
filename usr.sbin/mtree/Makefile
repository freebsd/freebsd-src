#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	fmtree
MAN=	fmtree.8 mtree.5
SRCS=	compare.c crc.c create.c excludes.c misc.c mtree.c spec.c verify.c
SRCS+=	specspec.c

CFLAGS+= -DMD5 -DSHA1 -DRMD160 -DSHA256
DPADD=	${LIBMD}
LDADD=	-lmd

.if ${MK_NMTREE} == "no"
LINKS=	${BINDIR}/fmtree ${BINDIR}/mtree
MLINKS=	fmtree.8 mtree.8
.endif

CLEANFILES+=	fmtree.8

fmtree.8: mtree.8
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
