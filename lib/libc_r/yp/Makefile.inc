#	from: @(#)Makefile.inc	5.3 (Berkeley) 2/20/91
#	$FreeBSD$

# yp sources
.PATH: ${.CURDIR}/../libc/yp

SRCS+=		xdryp.c yp_xdr.c yplib.c
CLEANFILES+=	yp_xdr.c yp.h

RPCSRC= ${.DESTDIR}/usr/include/rpcsvc/yp.x
RPCGEN= rpcgen

yp_xdr.c: ${RPCSRC} yp.h
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC}

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}
