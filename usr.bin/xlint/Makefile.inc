#	$NetBSD: Makefile.inc,v 1.8 2002/02/04 00:18:32 thorpej Exp $

.include <bsd.own.mk>

.PATH:		${.CURDIR}/../common

# On the m68000, MACHINE_CPU is "m68k", but we have different
# target parameters (namely, long double is a different size
# on the m68000), so we special case the directory name here.
.if ${MACHINE_ARCH} == "m68000"
ARCHSUBDIR=	m68000
.else
ARCHSUBDIR=	${MACHINE_CPU}
.endif

CPPFLAGS+=	-I${.CURDIR}/../arch/${ARCHSUBDIR}
CPPFLAGS+=	-I${.CURDIR}/../common

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
