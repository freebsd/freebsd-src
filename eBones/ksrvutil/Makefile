#	$Id$

PROG=	ksrvutil
SRCS=	ksrvutil.c
CFLAGS+=	-I${.CURDIR}/../include -I${.CURDIR}/../libkadm
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes \
	-lcom_err
NOMAN=	#man page installed by ../man

.include <bsd.prog.mk>
