#	@(#)Makefile	8.1 (Berkeley) 6/28/93

PROG=	mk-amd-map
CFLAGS+=-I${.OBJDIR}
CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../rpcx
CFLAGS+=-I${.CURDIR}/../config
CFLAGS+=-DOS_HDR=\"os-bsd44.h\"
MAN8=	mk-amd-map.8
CLEANFILES+=nfs_prot.h
RPCCOM = rpcgen
NFS_PROT_X= ${DESTDIR}/usr/include/rpcsvc/nfs_prot.x
DPSRCS= nfs_prot.h

nfs_prot.h: ${NFS_PROT_X}
	${RPCCOM} -h ${NFS_PROT_X} -o ${.TARGET}

.include "../../Makefile.inc"
.include <bsd.prog.mk>
