#	$Id$

PROG=	kadmin
SRCS=	${KRBOBJDIR}/krb_err.h kadmin.c kadmin_cmds.c
CLEANFILES+=	kadmin_cmds.c krb_err.c
CFLAGS+=	-DPOSIX -I${.CURDIR}/../../lib/libkadm -I${KRBOBJDIR} -DKERBEROS
DPADD= ${LIBKRB} ${LIBDES}
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
LDADD+=	-lss -lcom_err
MAN8=	kadmin.8

kadmin_cmds.c:	kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	mk_cmds kadmin_cmds.ct

.include <bsd.prog.mk>
