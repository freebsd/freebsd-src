#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

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
