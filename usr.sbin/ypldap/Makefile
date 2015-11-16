# $OpenBSD: Makefile,v 1.8 2015/09/09 15:33:18 deraadt Exp $
# $FreeBSD$

PROG=		ypldap
SRCS=		parse.y ypldap.c log.c	\
		ldapclient.c entries.c yp.c \
		aldap.c ber.c \
		ypldap_dns.c

MAN=		ypldap.8 ypldap.conf.5

DPADD=		${LIBEVENT} ${LIBUTIL} ${LIBRPCSVC}
LDADD=		-levent -lutil -lrpcsvc
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
