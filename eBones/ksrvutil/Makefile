#	$Id: Makefile,v 1.1 1995/07/18 16:40:09 mark Exp $

PROG=	ksrvutil
SRCS=	ksrvutil.c
CFLAGS+=	-I${.CURDIR}/../include -I${.CURDIR}/../libkadm -Wall
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -ldes -lcom_err
MAN8=	ksrvutil.8

.include <bsd.prog.mk>
