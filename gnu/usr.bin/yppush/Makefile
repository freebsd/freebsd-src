#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.7 1995/05/30 05:05:19 rgrimes Exp $

PROG=	yppush
SRCS=	yp_clnt.c yppush.c

MAN8=	yppush.8
CLEANFILES= yp_clnt.c yp.h

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/yp.x

yp_clnt.c: ${RPCSRC} yp.h
	rpcgen -l -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	rpcgen -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
