#	$Id: Makefile,v 1.2 1995/09/07 20:53:52 mark Exp $

PROG=	ksrvutil
SRCS=	ksrvutil.c
CFLAGS+=	-I${.CURDIR}/../../include -I${.CURDIR}/../../lib/libkadm -Wall
DPADD=	${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkrb -ldes -lcom_err
MAN8=	ksrvutil.8
BINDIR=	/usr/sbin

.include <bsd.prog.mk>
