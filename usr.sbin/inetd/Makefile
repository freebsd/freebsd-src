#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.13 1999/07/22 14:11:25 sheldonh Exp $

PROG=	inetd
SRCS=	inetd.c builtins.c
MAN8=	inetd.8
MLINKS=	inetd.8 inetd.conf.5
MAINTAINER=des@freebsd.org

COPTS+=	-Wall -DLOGIN_CAP
#COPTS+=	-DSANITY_CHECK

DPADD+=	${LIBUTIL} ${LIBWRAP}
LDADD+=	-lutil -lwrap

.include <bsd.prog.mk>
