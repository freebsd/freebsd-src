# $FreeBSD$

PROG=		kadmin
MAN=		${KRB4DIR}/man/kadmin.8
SRCS=		kadmin.c new_pwd.c random_password.c \
		${KADMOBJDIR}/kadm_err.h ${KRBOBJDIR}/krb_err.h
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
		-lcrypto -lcrypt -lcom_err -lreadline -ltermcap
DPADD=		${LIBROKEN} ${LIBSL} ${LIBACL} ${LIBKADM} ${LIBKDB} ${LIBKRB} \
		${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LIBREADLINE} \
		${LIBTERMCAP}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/kadmin
