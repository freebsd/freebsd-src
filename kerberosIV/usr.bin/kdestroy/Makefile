# $FreeBSD$

PROG=		kdestroy
MAN=		${KRB4DIR}/man/kdestroy.1
SRCS=		kdestroy.c protos.h
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kafs \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/kuser \
		-I${.OBJDIR}
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KAFSOBJDIR} -lkafs -L${KRBOBJDIR} -lkrb -lcrypto -lcrypt \
		-lcom_err
DPADD=		${LIBROKEN} ${LIBKAFS} ${LIBKRB} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBCOM_ERR}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/kuser
