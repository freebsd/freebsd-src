#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.10 1999/06/27 18:05:33 sheldonh Exp $

PROG=	inetd
MAN8=	inetd.8
MLINKS=	inetd.8 inetd.conf.5

COPTS+=	-Wall -DLOGIN_CAP
#COPTS+=	-DSANITY_CHECK
.ifdef REAL_IDENT
COPTS+= -DREAL_IDENT
.ifdef FAKEID
COPTS+= -DFAKEID
.endif
.endif

DPADD+=	${LIBUTIL} ${LIBWRAP}
LDADD+=	-lutil -lwrap

.include <bsd.prog.mk>
