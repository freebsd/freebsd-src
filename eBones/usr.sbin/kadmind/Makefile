#	$Id: Makefile,v 1.5 1995/09/14 04:09:38 gibbs Exp $

PROG=	kadmind
SRCS=	admin_server.c kadm_funcs.c kadm_ser_wrap.c kadm_server.c
CFLAGS+=-DPOSIX -I${.CURDIR}/../../lib/libkadm
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb \
	-L${DESOBJDIR} -ldes -L${ACLOBJDIR} -lacl -lcom_err
MAN8=	kadmind.8

.include <bsd.prog.mk>
