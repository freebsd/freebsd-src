#	$Id: Makefile,v 1.2 1997/10/02 15:03:23 markm Exp $

PROG=		kstash
MAN8=		${KRB4DIR}/man/kstash.8
SRCS=		kstash.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/admin \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes -lcrypt
DPADD=		${LIBROKEN} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/admin
