#	$Id: Makefile,v 1.2 1997/10/02 15:03:20 markm Exp $

PROG=		ext_srvtab
MAN8=		${KRB4DIR}/man/ext_srvtab.8
SRCS=		ext_srvtab.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/admin \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes -lcrypt
DPADD=		${LIBROKEN} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/admin
