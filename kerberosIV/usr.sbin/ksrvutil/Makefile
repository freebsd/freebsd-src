#	$Id: Makefile,v 1.4 1998/03/20 17:48:14 markm Exp $

PROG=		ksrvutil
MAN8=		${KRB4DIR}/man/ksrvutil.8
SRCS=		ksrvutil.c ksrvutil_get.c \
		${KADMOBJDIR}/kadm_err.h ${KRBOBJDIR}/krb_err.h
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/sl \
		-I${KRB4DIR}/lib/acl \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/admin \
		-I${KADMOBJDIR} \
		-I${KRBOBJDIR} \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken -L${KADMOBJDIR} -lkadm \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes -lcom_err \
		-lcrypt
DPADD=		${LIBROKEN} ${LIBKADM} ${LIBKDB} ${LIBKRB} \
		${LIBDES} ${LIBCOM_ERR} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/kadmin
