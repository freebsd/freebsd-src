#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.3 1995/07/18 16:35:54 mark Exp $

PROG=	ext_srvtab
CFLAGS+=-DKERBEROS -I${.CURDIR}/../include  -Wall
DPADD=	${LIBKDB} ${LIBKRB}
LDADD+=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes
MAN8=	ext_srvtab.8

.include <bsd.prog.mk>
