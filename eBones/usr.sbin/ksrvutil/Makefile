#	$Id: Makefile,v 1.5 1995/09/14 04:11:06 gibbs Exp $

PROG=	ksrvutil
SRCS=	ksrvutil.c
CFLAGS+=	-I${.CURDIR}/../../lib/libkadm
DPADD=	${LIBKRB} ${LIBDES}
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb \
	-L${DESOBJDIR} -ldes -lcom_err
MAN8=	ksrvutil.8

.include <bsd.prog.mk>
