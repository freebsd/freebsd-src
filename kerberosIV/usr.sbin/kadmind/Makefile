# $FreeBSD$

PROG=		kadmind
MAN=		${KRB4DIR}/man/kadmind.8
SRCS=		kadm_server.c kadm_funcs.c admin_server.c kadm_ser_wrap.c \
		pw_check.c ${KADMOBJDIR}/kadm_err.h ${KRBOBJDIR}/krb_err.h
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/sl \
		-I${KRB4DIR}/lib/acl \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/kadmin \
		-I${KADMOBJDIR} \
		-I${KRBOBJDIR} \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken -L${SLOBJDIR} -lsl \
		-L${ACLOBJDIR} -lacl -L${KADMOBJDIR} -lkadm \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb \
		-lcrypto -lcom_err -lcrypt
DPADD=		${LIBROKEN} ${LIBSL} ${LIBACL} ${LIBKADM} ${LIBKDB} ${LIBKRB} \
		${LIBCRYPTO} ${LIBCOM_ERR} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/kadmin
