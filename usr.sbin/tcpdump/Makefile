#	@(#)Makefile	0.1 (RGrimes) 4/4/93
# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	tcpdump

.if ${MK_CHERI} != "no"
SUBDIR+=	cheri_tcpdump
.endif

.include <bsd.subdir.mk>
