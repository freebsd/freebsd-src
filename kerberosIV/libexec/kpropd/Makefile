# $FreeBSD$

PROG=		kpropd
MAN8=		${KRB4DIR}/man/kpropd.8
SRCS=		kpropd.c protos.h
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/slave \
		-I${INCLOBJDIR} \
		-I${.CURDIR}/../include \
		-I${.OBJDIR}
LDADD=		-L${ROKENOBJDIR} -lroken -L${KRBOBJDIR} -lkrb -ldes -lcrypt \
		-lcom_err
DPADD=		${LIBROKEN} ${LIBKRB} ${LIBDES} ${LIBCRYPT} ${LIBCOM_ERR}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/slave
