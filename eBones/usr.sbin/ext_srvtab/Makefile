#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/13 17:24:16 markm Exp $

PROG=	ext_srvtab
CFLAGS+=-DKERBEROS
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD+=	-lkdb -lkrb -ldes
MAN8=	ext_srvtab.8

.include <bsd.prog.mk>
