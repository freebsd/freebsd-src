#	$Id$

BINDIR=	/usr/bin
PROG=	kadmin
SRCS=	kadmin.c kadmin_cmds.c krb_err.h
CLEANFILES+=	kadmin_cmds.c krb_err.c
CFLAGS+=	-DPOSIX -I${.CURDIR}/../../include \
		-I${.CURDIR}/../../lib/libkadm -Wall
DPADD= ${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkrb -ldes
LDADD+=	-lss -lcom_err
MAN8=	kadmin.8

kadmin_cmds.c:	kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	mk_cmds kadmin_cmds.ct

krb_err.h:
	test -e krb_err.et || ln -s ${.CURDIR}/../../lib/libkrb/krb_err.et .
	compile_et krb_err.et

.include <bsd.prog.mk>
