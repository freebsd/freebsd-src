#	from: @(#)Makefile    5.10 (Berkeley) 6/24/90
#	$Id: Makefile,v 1.6 1996/08/30 02:02:09 peter Exp $

.PATH: ${DESTDIR}/usr/include/rpcsvc

LIB=    rpcsvc

RPCSRCS= klm_prot.x mount.x nfs_prot.x nlm_prot.x rex.x rnusers.x \
	rquota.x rstat.x rwall.x sm_inter.x spray.x yppasswd.x ypxfrd.x \
	ypupdate_prot.x

OTHERSRCS=  rnusers.c rstat.c rwall.c yp_passwd.c yp_update.c
SECRPCSRCS= publickey.c secretkey.c xcrypt.c

RPCCOM = rpcgen -C

INCDIRS= -I${DESTDIR}/usr/include/rpcsvc -I${DESTDIR}/usr/include

CFLAGS+= -DYP ${INCDIRS}

GENSRCS= ${RPCSRCS:R:S/$/_xdr.c/g}
SRCS+= ${GENSRCS} ${OTHERSRCS} ${SECRPCSRCS}

CLEANFILES+= ${GENSRCS} ${RPCSRCS}

NOMAN= noman

.include <bsd.lib.mk>

.SUFFIXES: .x _xdr.c

.x_xdr.c:
	${RPCCOM} -c ${.IMPSRC} -o ${.TARGET}

OBJS=	${RPCSRCS:R:S/$/_xdr.o/g} ${SECRPCSRCS:R:S/$/.o/g} \
	${OTHERSRCS:R:S/$/.o/g}
