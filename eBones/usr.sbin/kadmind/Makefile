#	$Id: Makefile,v 1.4 1995/09/13 17:24:18 markm Exp $

PROG=	kadmind
SRCS=	admin_server.c kadm_funcs.c kadm_ser_wrap.c kadm_server.c
CFLAGS+=-DPOSIX -I${.CURDIR}/../../lib/libkadm
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkdb -lkrb -ldes -lacl -lcom_err
MAN8=	kadmind.8

.include <bsd.prog.mk>
