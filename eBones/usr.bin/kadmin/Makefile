#	$Id: Makefile,v 1.7 1995/09/14 18:16:18 gibbs Exp $

PROG=	kadmin
SRCS=	${KRBOBJDIR}/krb_err.h kadmin.c kadmin_cmds.c
CLEANFILES+=	kadmin_cmds.c krb_err.c
CFLAGS+=	-DPOSIX -I${.CURDIR}/../../lib/libkadm
DPADD= ${LIBKRB} ${LIBDES}
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
LDADD+=	-lss -lcom_err
MAN8=	kadmin.8

kadmin_cmds.c:	kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	mk_cmds kadmin_cmds.ct

.include <bsd.prog.mk>
