#	$Id: Makefile,v 1.5 1995/09/13 17:24:06 markm Exp $

PROG=	kadmin
SRCS=	kadmin.c kadmin_cmds.c
CLEANFILES+=	kadmin_cmds.c krb_err.c
CFLAGS+=	-DPOSIX -I${.CURDIR}/../../lib/libkadm
DPADD= ${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkrb -ldes
LDADD+=	-lss -lcom_err
MAN8=	kadmin.8

kadmin_cmds.c:	kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	mk_cmds kadmin_cmds.ct

.include <bsd.prog.mk>

kadmin.o: ${KRBOBJDIR}/krb_err.h
