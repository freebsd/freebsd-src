#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

PROG=	inetd
MAN8=	inetd.8
MLINKS=	inetd.8 inetd.conf.5

COPTS+=	-Wall -DLOGIN_CAP -DLIBWRAP
#COPTS+=	-DSANITY_CHECK

DPADD+=	${LIBUTIL} ${LIBWRAP}
LDADD+=	-lutil -lwrap

.include <bsd.prog.mk>
