#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.5.2.1 1999/05/03 16:32:54 obrien Exp $

PROG=	inetd
MAN8=	inetd.8
MLINKS=	inetd.8 inetd.conf.5

COPTS+=	-Wall -DLOGIN_CAP -DLIBWRAP
#COPTS+=	-DSANITY_CHECK

DPADD+=	${LIBUTIL} ${LIBWRAP}
LDADD+=	-lutil -lwrap

.include <bsd.prog.mk>
