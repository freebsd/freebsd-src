#	$Id: Makefile,v 1.4 1995/09/13 17:24:34 markm Exp $

PROG=	ksrvutil
SRCS=	ksrvutil.c
CFLAGS+=	-I${.CURDIR}/../../lib/libkadm
DPADD=	${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkrb -ldes -lcom_err
MAN8=	ksrvutil.8

.include <bsd.prog.mk>
