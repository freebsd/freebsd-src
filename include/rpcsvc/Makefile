#	from: @(#)Makefile	2.3 88/08/11 4.0 RPCSRC
#	$Id: Makefile,v 1.8 1996/06/05 03:47:18 wpaul Exp $

.SUFFIXES: .x

RPCCOM = rpcgen
BINMODE = 444

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
	cd ${.CURDIR} && \
		${INSTALL} -C -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} \
		${HFILES} ${DESTDIR}/usr/include/rpcsvc
	cd ${.CURDIR} && \
		${INSTALL} -c -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} \
		${XFILES} ${DESTDIR}/usr/include/rpcsvc
	${INSTALL} -c -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} ${HDRS} \
		${DESTDIR}/usr/include/rpcsvc

.x.h:
	cd ${.CURDIR} ; ${RPCCOM} -h $*.x -o ${.OBJDIR}/$@

.include <bsd.prog.mk>
