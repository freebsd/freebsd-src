#	from: @(#)Makefile	2.3 88/08/11 4.0 RPCSRC
#	$Id: Makefile,v 1.1 1993/09/14 17:42:40 jtc Exp $

.SUFFIXES: .x

RPCCOM = rpcgen

HDRS= klm_prot.h mount.h nfs_prot.h nlm_prot.h rex.h rnusers.h\
      rquota.h rstat.h rwall.h sm_inter.h spray.h yppasswd.h yp.h
XFILES= bootparam_prot.x klm_prot.x mount.x nfs_prot.x nlm_prot.x \
      rex.x rnusers.x rquota.x rstat.x rwall.x sm_inter.x spray.x \
      yppasswd.x yp.x
HFILES= yp_prot.h ypclnt.h

CLEANFILES+= ${HDRS}

all: ${HDRS}

install: all
	@echo "Creating RPC service headers directory"
	@/bin/rm -rf ${DESTDIR}/usr/include/rpcsvc
	@-mkdir ${DESTDIR}/usr/include/rpcsvc
	@echo "Installing RPC service header and definition files"
	@for i in $(XFILES); do \
		(install -c -m 644 ${.CURDIR}/$$i ${DESTDIR}/usr/include/rpcsvc) done
	@for i in $(HDRS); do \
		(install ${COPY} -m 644 $$i ${DESTDIR}/usr/include/rpcsvc) done
	@for i in $(HFILES); do \
		(install -c -m 644 ${.CURDIR}/$$i ${DESTDIR}/usr/include/rpcsvc) done
	@chown -R ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include/rpcsvc
	@chmod -R a-w ${DESTDIR}/usr/include/rpcsvc

.x.h:
	@echo generating $@...
	@PWD=`pwd` ; cd ${.CURDIR} ; ${RPCCOM} -h $*.x -o $$PWD/$@

.include <bsd.prog.mk>
