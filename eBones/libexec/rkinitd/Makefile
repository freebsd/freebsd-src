#	@(#)Makefile	8.1 (Berkeley) 6/4/93

PROG=	rkinitd
SRCS= 	${RKINITOBJDIR}/rkinit_err.h rkinitd.c util.c rpc.c krb.c
CFLAGS+=-I${KRBOBJDIR} -I${RKINITOBJDIR} -DKERBEROS
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-L${RKINITOBJDIR} -lrkinit -L${KRBOBJDIR} -lkrb \
	-L${DESOBJDIR} -ldes

MAN8=	rkinitd.8

.include <bsd.prog.mk>
