#	from: @(#)Makefile	2.3 88/08/11 4.0 RPCSRC
#	$Id: Makefile,v 1.9 1996/08/29 19:59:48 peter Exp $

.SUFFIXES: .x

RPCCOM = rpcgen

HDRS= klm_prot.h mount.h nfs_prot.h nlm_prot.h rex.h rnusers.h \
      rquota.h rstat.h rwall.h sm_inter.h spray.h yppasswd.h yp.h \
      ypxfrd.h
XFILES= bootparam_prot.x klm_prot.x mount.x nfs_prot.x nlm_prot.x \
      rex.x rnusers.x rquota.x rstat.x rwall.x sm_inter.x spray.x \
      yppasswd.x yp.x ypxfrd.x
HFILES= yp_prot.h ypclnt.h

CLEANFILES+= ${HDRS}

all: ${HDRS}

install: ${HDRS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HFILES:S;^;${.CURDIR}/;} \
		${XFILES:S;^;${.CURDIR}/;} \
		${HDRS} \
		${DESTDIR}/usr/include/rpcsvc

.x.h:
	${RPCCOM} -h ${.IMPSRC} -o ${.TARGET}

.include <bsd.prog.mk>
