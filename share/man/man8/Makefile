#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.9 1998/01/01 10:50:53 bde Exp $

MAN8=	adding_user.8 diskless.8 intro.8 rc.8 sticky.8

# XXX NOT IMPORTED:	man8.hp300 man8.tahoe man8.vax
SUBDIR=	man8.i386

.if make(maninstall)
maninstall:: _SUBDIR
.endif

.include <bsd.prog.mk>
