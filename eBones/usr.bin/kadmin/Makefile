#	$Id: Makefile,v 1.1 1995/01/20 02:47:48 wollman Exp $

BINDIR=	/usr/bin
PROG=	kadmin
SRCS=	kadmin.c kadmin_cmds.c
CLEANFILES+=	kadmin_cmds.c
CFLAGS+=	-DPOSIX -I${.CURDIR}/../include -I${KRBOBJDIR}
CFLAGS+=	-I${.CURDIR}/../libkadm
LDADD+=	-L${KADMOBJDIR} -lkadm -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
LDADD+=	-lss -lcom_err
NOMAN=		# man page is in ../man

kadmin_cmds.c:	kadmin_cmds.ct
	test -e kadmin_cmds.ct || ln -s ${.CURDIR}/kadmin_cmds.ct .
	mk_cmds kadmin_cmds.ct

.include <bsd.prog.mk>


