# $FreeBSD$ 

MAINTAINER=	n_hibma@freebsd.org
S=	${.CURDIR}/../..

.PATH: $S/cam $S/cam/scsi

KMOD=	cam

# See sys/conf/options for the flags that go into the different opt_*.h files.
SRCS=	opt_cam.h
SRCS+=	opt_scsi.h
SRCS+=	opt_cd.h
SRCS+=	opt_da.h
SRCS+=	opt_hw_wdog.h
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
SRCS+=	scsi_targ_bh.c scsi_target.c

EXPORT_SYMS=	YES	# XXX evaluate

opt_scsi.h:
	echo '#define SCSI_DELAY 15000' > opt_scsi.h

.include <bsd.kmod.mk>
