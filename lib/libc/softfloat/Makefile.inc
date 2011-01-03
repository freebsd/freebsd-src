#	$NetBSD: Makefile.inc,v 1.3 2003/05/06 08:58:20 rearnsha Exp $
# $FreeBSD$

SOFTFLOAT_BITS?=64
.PATH:		${LIBC_ARCH}/softfloat \
		${.CURDIR}/softfloat/bits${SOFTFLOAT_BITS} ${.CURDIR}/softfloat

CFLAGS+=	-I${.CURDIR}/${LIBC_ARCH}/softfloat -I${.CURDIR}/softfloat
CFLAGS+=	-DSOFTFLOAT_FOR_GCC

SRCS+=		softfloat.c

SRCS+=		fpgetround.c fpsetround.c fpgetmask.c fpsetmask.c \
		fpgetsticky.c

SRCS+=		eqsf2.c nesf2.c gtsf2.c gesf2.c ltsf2.c lesf2.c negsf2.c \
		eqdf2.c nedf2.c gtdf2.c gedf2.c ltdf2.c ledf2.c negdf2.c \
		unordsf2.c unorddf2.c

SYM_MAPS+=	${.CURDIR}/softfloat/Symbol.map
