# $FreeBSD$

S=	${.CURDIR}/../..

.PATH: $S/cam $S/cam/scsi $S/cam/ata $S/${MACHINE}/${MACHINE}

KMOD=	cam

# See sys/conf/options for the flags that go into the different opt_*.h files.
SRCS=	opt_cam.h
SRCS+=	opt_ada.h
SRCS+=	opt_scsi.h
SRCS+=	opt_cd.h
SRCS+=	opt_pt.h
SRCS+=	opt_sa.h
SRCS+=	opt_ses.h
SRCS+=	device_if.h bus_if.h vnode_if.h
SRCS+=	cam.c
SRCS+=	cam_compat.c
.if exists($S/${MACHINE}/${MACHINE}/cam_machdep.c)
SRCS+=	cam_machdep.c
.endif
SRCS+=	cam_periph.c cam_queue.c cam_sim.c cam_xpt.c
SRCS+=	scsi_all.c scsi_cd.c scsi_ch.c
SRCS+=	scsi_da.c
SRCS+=	scsi_pass.c
SRCS+=	scsi_pt.c
SRCS+=	scsi_sa.c
SRCS+=	scsi_enc.c
SRCS+=	scsi_enc_ses.c
SRCS+=	scsi_enc_safte.c
SRCS+=	scsi_sg.c
SRCS+=	scsi_targ_bh.c scsi_target.c
SRCS+=	scsi_xpt.c
SRCS+=	smp_all.c
SRCS+=	ata_all.c
SRCS+=	ata_xpt.c
SRCS+=	ata_da.c
.if exists($S/${MACHINE}/${MACHINE}/ata_machdep.c)
SRCS+=	ata_machdep.c
.endif
SRCS+=	ata_pmp.c

EXPORT_SYMS=	YES	# XXX evaluate

.if !defined(KERNBUILDDIR)
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
