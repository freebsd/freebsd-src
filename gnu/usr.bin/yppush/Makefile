#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.4 1995/03/02 21:59:51 wpaul Exp $

PROG=	yppush
SRCS=	yp_svc.c yp_clnt.c yppush.c

MAN8=	yppush.8
CLEANFILES= yp_svc.c yp_clnt.c yp.h yp.x
LDADD=/usr/src/lib/libc/obj/libc.a

yp_svc.c: yp.h
	rm -f yp.x
	cp ${DESTDIR}/usr/include/rpcsvc/yp.x .
	rpcgen  -m -o yp_svc.c yp.x

yp_clnt.c: yp.h
	rm -f yp.x
	cp ${DESTDIR}/usr/include/rpcsvc/yp.x .
	rpcgen  -l -o yp_clnt.c yp.x

yp.h: 
	rm -f yp.x
	cp ${DESTDIR}/usr/include/rpcsvc/yp.x .
	rpcgen  -h -o yp.h yp.x

.include <bsd.prog.mk>
