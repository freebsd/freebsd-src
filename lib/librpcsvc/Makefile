#	from: @(#)Makefile    5.10 (Berkeley) 6/24/90
#	$Id: Makefile,v 1.3 1994/08/28 17:48:43 bde Exp $

.PATH: ${DESTDIR}/usr/include/rpcsvc

LIB=    rpcsvc

RPCSRCS= klm_prot.x mount.x nfs_prot.x nlm_prot.x rex.x rnusers.x \
	rquota.x rstat.x rwall.x sm_inter.x spray.x yppasswd.x

RPCCOM = rpcgen

INCDIRS= -I/usr/include/rpcsvc
CFLAGS+= ${INCDIRS}

SRCS= ${RPCSRCS:R:S/$/_xdr.c/g}

CLEANFILES+= ${SRCS} ${RPCSRCS}

NOMAN= noman

.include <bsd.lib.mk>

.SUFFIXES: .x _xdr.c

.x_xdr.c:
	@${ECHO} generating ${.TARGET}...
	@PWD=`pwd` ; cd ${.CURDIR} ; \
		if cmp -s ${.IMPSRC} ${*F}.x > /dev/null 2>/dev/null; then \
			true; \
		else \
			cp -f ${.IMPSRC} $$PWD/${*F}.x ; \
		fi
	@${RPCCOM} -c ${*F}.x -o ${.TARGET}

OBJS= ${RPCSRCS:R:S/$/_xdr.o/g}
