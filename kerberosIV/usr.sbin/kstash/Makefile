# $FreeBSD$

PROG=		kstash
MAN=		${KRB4DIR}/man/kstash.8
SRCS=		kstash.c protos.h
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/admin \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${.CURDIR}/../include \
		-I${.OBJDIR}
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -lcrypto -lcrypt \
		-lcom_err
DPADD=		${LIBROKEN} ${LIBKDB} ${LIBKRB} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBCOM_ERR}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/admin
