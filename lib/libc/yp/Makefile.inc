#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
# $FreeBSD: src/lib/libc/yp/Makefile.inc,v 1.10.12.1 2010/02/10 00:26:20 kensmith Exp $

# yp sources
.PATH: ${.CURDIR}/yp

SRCS+=	xdryp.c yp.h yp_xdr.c yplib.c
CLEANFILES+=	yp.h yp_xdr.c

SYM_MAPS+=	${.CURDIR}/yp/Symbol.map

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN=	rpcgen -C

yp_xdr.c: ${RPCSRC}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}
