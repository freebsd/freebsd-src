#	Makefile,v 1.2 1995/01/20 22:08:14 wollman Exp

PROG=	rkinit
SRCS=	${RKINITOBJDIR}/rkinit_err.h rkinit.c
CFLAGS+=	-I${KRBOBJDIR} -I${RKINITOBJDIR} -DKERBEROS
LDADD+=	-L${RKINITOBJDIR} -lrkinit -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
LDADD+=	-lss -lcom_err

MAN1=	rkinit.1

.include <bsd.prog.mk>
