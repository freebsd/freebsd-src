#	$FreeBSD$

PROG=	ksrvutil
SRCS=	ksrvutil.c
CFLAGS+=	-I${.CURDIR}/../../lib/libkadm -DKERBEROS
DPADD=	${LIBKRB} ${LIBDES}
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb \
	-L${DESOBJDIR} -ldes -lcom_err
MAN8=	ksrvutil.8

.include <bsd.prog.mk>
