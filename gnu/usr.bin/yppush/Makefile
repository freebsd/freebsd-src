#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	Makefile,v 1.7 1995/05/30 05:05:19 rgrimes Exp

PROG=	yppush
SRCS=	yp_clnt.c yppush.c

MAN8=	yppush.8
CLEANFILES= yp_clnt.c yp.h yp.x

yp_clnt.c: yp.h
	rm -f yp.x
	cp ${DESTDIR}/usr/include/rpcsvc/yp.x .
	rpcgen  -l -o yp_clnt.c yp.x

yp.h:
	rm -f yp.x
	cp ${DESTDIR}/usr/include/rpcsvc/yp.x .
	rpcgen  -h -o yp.h yp.x

.include <bsd.prog.mk>
