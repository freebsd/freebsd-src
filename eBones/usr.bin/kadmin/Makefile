#	$Id: Makefile,v 1.1 1995/07/18 16:36:53 mark Exp $

BINDIR=	/usr/bin
PROG=	kadmin
SRCS=	kadmin.c kadmin_cmds.c
CLEANFILES+=	kadmin_cmds.c
CFLAGS+=	-DPOSIX -I${.CURDIR}/../include -I${KRBOBJDIR}
CFLAGS+=	-I${.CURDIR}/../libkadm -Wall
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -ldes
LDADD+=	-lss -lcom_err
MAN8=	kadmin.8

kadmin_cmds.c:	kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	mk_cmds kadmin_cmds.ct

.include <bsd.prog.mk>


