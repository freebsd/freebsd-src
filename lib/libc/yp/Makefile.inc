#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
#	$Id: Makefile.inc,v 1.6 1997/05/23 08:23:59 asami Exp $

# yp sources
.PATH: ${.CURDIR}/../libc/yp

SRCS+=	xdryp.c yp.h yp_xdr.c yplib.c
CLEANFILES+=	yp.h yp_xdr.c

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN=	rpcgen -C

yp_xdr.c: ${RPCSRC}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}
