#	from: @(#)Makefile    5.10 (Berkeley) 6/24/90
#	$Id: Makefile,v 1.5 1996/06/05 03:54:43 wpaul Exp $

.PATH: ${DESTDIR}/usr/include/rpcsvc

LIB=    rpcsvc

RPCSRCS= klm_prot.x mount.x nfs_prot.x nlm_prot.x rex.x rnusers.x \
	rquota.x rstat.x rwall.x sm_inter.x spray.x yppasswd.x ypxfrd.x

RPCCOM = rpcgen

INCDIRS= -I/usr/include/rpcsvc
CFLAGS+= ${INCDIRS}

SRCS= ${RPCSRCS:R:S/$/_xdr.c/g}

CLEANFILES+= ${SRCS} ${RPCSRCS}

NOMAN= noman

.include <bsd.lib.mk>

.SUFFIXES: .x _xdr.c

.x_xdr.c:
	${RPCCOM} -c ${.IMPSRC} -o ${.TARGET}

OBJS= ${RPCSRCS:R:S/$/_xdr.o/g}
