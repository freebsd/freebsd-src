#	$Id: Makefile,v 1.1 1995/07/18 16:40:09 mark Exp $

PROG=	ksrvutil
SRCS=	ksrvutil.c
CFLAGS+=	-I${.CURDIR}/../include -I${.CURDIR}/../libkadm
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes \
	-lcom_err
NOMAN=	#man page installed by ../man

.include <bsd.prog.mk>
