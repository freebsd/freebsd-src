#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/man/man8/Makefile,v 1.9.2.3 2000/05/04 17:58:16 archie Exp $

MAN8=	adding_user.8 diskless.8 intro.8 rc.8 sticky.8

# XXX NOT IMPORTED:	man8.hp300 man8.tahoe man8.vax
SUBDIR=	man8.i386

.if make(maninstall)
maninstall:: _SUBDIR
.endif

.include <bsd.prog.mk>
