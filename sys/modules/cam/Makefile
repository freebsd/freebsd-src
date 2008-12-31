# $FreeBSD: src/sys/modules/cam/Makefile,v 1.15.6.1 2008/11/25 02:59:29 kensmith Exp $

S=	${.CURDIR}/../..

.PATH: $S/cam $S/cam/scsi

KMOD=	cam

# See sys/conf/options for the flags that go into the different opt_*.h files.
SRCS=	opt_cam.h
SRCS+=	opt_scsi.h
SRCS+=	opt_cd.h
SRCS+=	opt_pt.h
SRCS+=	opt_sa.h
SRCS+=	opt_ses.h
SRCS+=	device_if.h bus_if.h vnode_if.h
SRCS+=	cam.c cam_periph.c cam_queue.c
SRCS+=	cam_sim.c cam_xpt.c
SRCS+=	scsi_all.c scsi_cd.c scsi_ch.c
SRCS+=	scsi_da.c
SRCS+=	scsi_pass.c
SRCS+=	scsi_pt.c
SRCS+=	scsi_sa.c
SRCS+=	scsi_ses.c
SRCS+=	scsi_sg.c
SRCS+=	scsi_targ_bh.c scsi_target.c

EXPORT_SYMS=	YES	# XXX evaluate

.if !defined(KERNBUILDDIR)
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
