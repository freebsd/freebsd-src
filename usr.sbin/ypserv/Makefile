# $FreeBSD$

RPCDIR=	${.CURDIR}/../../include/rpcsvc
.PATH: ${RPCDIR}

PROG=	ypserv
MAN=	ypserv.8 ypinit.8
SRCS=	yp_svc.c yp_server.c yp_dblookup.c yp_dnslookup.c \
	ypxfr_clnt.c yp.h yp_main.c yp_error.c yp_access.c yp_svc_udp.c

DPADD=	${LIBWRAP}
LDADD=	-lwrap

CFLAGS+= -I. -DDB_CACHE -DTCP_WRAPPER

CLEANFILES= yp_svc.c ypxfr_clnt.c yp.h

RPCGEN= rpcgen -I -C

# We need to remove the 'static' keyword from _rpcsvcstate so that
# yp_main.c can see it.
yp_svc.c: yp.x
	rm -f ${.TARGET}
	${RPCGEN} -DYPSERV_ONLY -m ${RPCDIR}/yp.x | \
	sed s/"static int _rpcsvcstate"/"int _rpcsvcstate"/g > ${.TARGET}

ypxfr_clnt.c: yp.x
	${RPCGEN} -DYPPUSH_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp.h: yp.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/Makefile.yp \
		${DESTDIR}/var/yp/Makefile.dist
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 555 \
		${.CURDIR}/ypinit.sh ${DESTDIR}${BINDIR}/ypinit
	@if [ ! -f ${DESTDIR}/var/yp/Makefile.dist ]; then \
		ln -s	${DESTDIR}/var/yp/Makefile.dist \
			${DESTDIR}/var/yp/Makefile; fi

.include <bsd.prog.mk>
