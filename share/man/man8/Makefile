#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/man/man8/Makefile,v 1.13 2000/01/25 20:26:51 archie Exp $

MAN8=	adding_user.8 crash.8 diskless.8 intro.8 ng_ether.8 rc.8 sticky.8

# XXX NOT IMPORTED:	man8.hp300 man8.tahoe man8.vax
.if ${MACHINE_ARCH} == "i386"
SUBDIR=	man8.i386
.elif ${MACHINE_ARCH} == "alpha"
SUBDIR=	man8.alpha
.endif

.if make(maninstall)
maninstall:: _SUBDIR
.endif

.include <bsd.prog.mk>
