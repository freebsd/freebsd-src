#	@(#)Makefile	8.2 (Berkeley) 4/22/94

PROG=	amd
MAN8=	amd.0
OS=	bsd44
SRCS=	afs_ops.c am_ops.c clock.c util.c xutil.c \
	efs_ops.c mapc.c info_file.c info_hes.c \
	info_ndbm.c info_passwd.c info_nis.c \
	info_union.c map.c srvr_afs.c srvr_nfs.c \
	mntfs.c misc_rpc.c mount_fs.c mount_xdr.c \
	mtab.c mtab_bsd.c nfs_ops.c nfs_prot_svc.c \
	nfs_start.c nfs_subr.c nfs_prot_xdr.c opts.c \
	pfs_ops.c rpc_fwd.c sched.c sfs_ops.c amq_svc.c \
	amq_subr.c umount_fs.c host_ops.c nfsx_ops.c \
	ufs_ops.c ifs_ops.c amd.c get_args.c restart.c wire.c
OBJS+=	vers.${PROG}.o
LDADD+= -lrpc
CFLAGS+=-I${.CURDIR}/../rpcx
CFLAGS+=-I${.CURDIR}/../config
CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-DARCH_REP=\"${MACHINE}\"
CFLAGS+=-DOS_REP=\"${OS}\"
CFLAGS+=-DOS_HDR=\"os-${OS}.h\"
CFLAGS+=${CONFIG}
CLEANFILES+=vers.${PROG}.c vers.${PROG}.o version.amd

vers.${PROG}.c: ${SRCS:.c=.o}
	@d=${.CURDIR}/ sh ${.CURDIR}/../config/newvers.sh ${PROG} ${MACHINE} ${OS}

.PATH: ${.CURDIR}/../rpcx ${.CURDIR}/../config
.include "Makefile.config"
.include "../../Makefile.inc"
.include <bsd.prog.mk>
