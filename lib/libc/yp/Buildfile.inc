# $FreeBSD$

# yp sources
.PATH: ${.CURDIR}/yp

SRCS += \
	xdryp.c \
	yp.h \
	yp_xdr.c \
	yplib.c

SYM_MAPS += ${.CURDIR}/yp/Symbol.map

GENHDRS += yp.h
GENSRCS += yp_xdr.c

.if exists(${COMMONSTAGEDIR}/usr/include/rpcsvc/yp.x)
YPX = ${COMMONSTAGEDIR}/usr/include/rpcsvc/yp.x
.else
YPX = ${COMMONSHAREDSTAGEDIR}/usr/include/rpcsvc/yp.x
.endif

yp_xdr.c: ${YPX}
	${RPCGEN} -c -o ${.TARGET} ${.ALLSRC}

yp.h: ${YPX}
	${RPCGEN} -h -o ${.TARGET} ${.ALLSRC}
