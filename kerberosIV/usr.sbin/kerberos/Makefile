#	$Id$

PROG=		kerberos
MAN1=		${KRB4DIR}/man/kerberos.1
MAN5=		${KRB4DIR}/man/krb.conf.5 ${KRB4DIR}/man/krb.realms.5 \
		${KRB4DIR}/man/krb.equiv.5
MAN8=		${KRB4DIR}/man/kerberos.8
SRCS=		kerberos.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KADMOBJDIR} \
		-I${KRBOBJDIR} \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb \
		-ldes
DPADD=		${LIBROKEN} ${LIBKDB} ${LIBKRB} ${LIBDES}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/server
