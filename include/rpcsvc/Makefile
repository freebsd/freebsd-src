#	from: @(#)Makefile	2.3 88/08/11 4.0 RPCSRC
#	$Id: Makefile,v 1.7 1995/08/15 20:06:50 joerg Exp $

.SUFFIXES: .x

RPCCOM = rpcgen
BINMODE = 444

HDRS= klm_prot.h mount.h nfs_prot.h nlm_prot.h rex.h rnusers.h\
      rquota.h rstat.h rwall.h sm_inter.h spray.h yppasswd.h yp.h
XFILES= bootparam_prot.x klm_prot.x mount.x nfs_prot.x nlm_prot.x \
      rex.x rnusers.x rquota.x rstat.x rwall.x sm_inter.x spray.x \
      yppasswd.x yp.x
HFILES= yp_prot.h ypclnt.h

CLEANFILES+= ${HDRS}
RPCDIR= ${DESTDIR}/usr/include/rpcsvc
EVERYTHING= $(XFILES) $(HFILES)

all: ${HDRS}

install: all
	@${ECHO} "Installing RPC service header and definition files"
.for i in $(EVERYTHING)
	@cd ${.CURDIR}; cmp -s $i $(RPCDIR)/$i || \
		(${INSTALL} -c -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} \
			$i ${RPCDIR}; \
			$(ECHO) $i)
.endfor
.for i in $(HDRS)
	@cmp -s $i $(RPCDIR)/$i || \
		(${INSTALL} -c -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} \
			$i ${RPCDIR}; \
			$(ECHO) $i)
.endfor

.x.h:
	cd ${.CURDIR} ; ${RPCCOM} -h $*.x -o ${.OBJDIR}/$@

.include <bsd.prog.mk>




