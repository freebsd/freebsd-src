#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG = amq
MAN8 = amq.8
SRCS = amq.c amq_clnt.c amq_xdr.c misc_rpc.c
CFLAGS+=-I${.OBJDIR}
CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../rpcx
CFLAGS+=-I${.CURDIR}/../config
CFLAGS+=-DARCH_REP=\"${MACHINE}\"
CFLAGS+=-DOS_REP=\"bsd44\"
CFLAGS+=-DOS_HDR=\"os-bsd44.h\"
.PATH: ${.CURDIR}/../rpcx ${.CURDIR}/../amd
CLEANFILES+=nfs_prot.h
RPCCOM = rpcgen
NFS_PROT_X= /usr/include/rpcsvc/nfs_prot.x
DPSRCS= nfs_prot.h

nfs_prot.h: ${NFS_PROT_X}
	${RPCCOM} -h ${NFS_PROT_X} -o ${.TARGET}

${OBJS} beforedepend: nfs_prot.h

.include "../../Makefile.inc"
.include <bsd.prog.mk>
