# $FreeBSD: src/usr.sbin/rpc.ypupdated/Makefile,v 1.6 1999/08/28 01:19:43 peter Exp $

PROG=	rpc.ypupdated
SRCS=	ypupdate_prot_svc.c ypupdate_prot.h ypupdated_main.c \
	yp_error.c update.c ypupdated_server.c \
	yp_dblookup.c yp_dbwrite.c yp_dbdelete.c yp_dbupdate.c

.PATH: ${.CURDIR}/../ypserv ${.CURDIR}/../../libexec/ypxfr

RPCDIR=	${DESTDIR}/usr/include/rpcsvc

NOMAN= yes

CFLAGS+= -I${.CURDIR}/../ypserv -I.
CFLAGS+= -I${.CURDIR}/../../libexec/ypxfr

#CFLAGS+= -DYP

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CLEANFILES= ypupdate_prot_svc.c ypupdate_prot.h

RPCGEN= rpcgen -I -C

# We need to remove the 'static' keyword from _rpcsvcstate so that
# ypupdated_main.c can see it.
ypupdate_prot_svc.c: ${RPCDIR}/ypupdate_prot.x
	rm -f ${.TARGET}
	${RPCGEN} -m ${RPCDIR}/ypupdate_prot.x | \
	sed s/"static int _rpcsvcstate"/"int _rpcsvcstate"/g > ${.TARGET}

ypupdate_prot.h: ${RPCDIR}/ypupdate_prot.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/ypupdate_prot.x

.include <bsd.prog.mk>
