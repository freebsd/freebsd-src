# $FreeBSD$

PROG=		kerberos
MAN=		${KRB4DIR}/man/kerberos.1
MAN+=		${KRB4DIR}/man/krb.conf.5 ${KRB4DIR}/man/krb.realms.5 \
		${KRB4DIR}/man/krb.equiv.5
MAN+=		${KRB4DIR}/man/kerberos.8
SRCS=		kerberos.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KADMOBJDIR} \
		-I${KRBOBJDIR} \
		-I${.CURDIR}/../include \
		-I${.OBJDIR}
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -lcrypto -lcrypt \
		-lcom_err
DPADD=		${LIBROKEN} ${LIBKDB} ${LIBKRB} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBCOM_ERR}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/server
