#	$Id: Makefile,v 1.1 1995/07/18 16:36:59 mark Exp $

PROG=	kadmind
SRCS=	admin_server.c kadm_funcs.c kadm_ser_wrap.c kadm_server.c
CFLAGS+=-DPOSIX -I${.CURDIR}/../include -I${KRBOBJDIR} \
	-I${.CURDIR}/../libkadm -I${KADMOBJDIR}
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb \
	-L${DESOBJDIR} -ldes -L${ACLOBJDIR} -lacl -lcom_err
NOMAN=	# man page in ../man

.include <bsd.prog.mk>
