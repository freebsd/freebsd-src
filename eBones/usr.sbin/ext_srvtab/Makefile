#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.4 1995/09/07 20:50:29 mark Exp $

PROG=	ext_srvtab
CFLAGS+=-DKERBEROS -I${.CURDIR}/../../include  -Wall
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD+=	-lkdb -lkrb -ldes
MAN8=	ext_srvtab.8
BINDIR=	/usr/sbin

.include <bsd.prog.mk>
