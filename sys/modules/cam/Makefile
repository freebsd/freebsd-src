
S=	${SRCTOP}/sys

.PATH: $S/cam $S/cam/scsi $S/cam/ata $S/cam/nvme $S/dev/nvme

KMOD=	cam

# See sys/conf/options for the flags that go into the different opt_*.h files.
SRCS=	opt_cam.h
SRCS+=	opt_ada.h
SRCS+=	opt_scsi.h
SRCS+=	opt_cd.h
SRCS+=	opt_da.h
SRCS+=	opt_pt.h
SRCS+=	opt_sa.h
SRCS+=	opt_ses.h
SRCS+=	opt_ddb.h
SRCS+=	opt_printf.h
SRCS+=	bus_if.h
SRCS+=	device_if.h
SRCS+=	vnode_if.h
SRCS+=	ata_all.c
SRCS+=	ata_da.c
SRCS+=	ata_pmp.c
SRCS+=	ata_xpt.c
SRCS+=	cam.c
SRCS+=	cam_compat.c
SRCS+=	cam_iosched.c
SRCS+=	cam_periph.c
SRCS+=	cam_queue.c
SRCS+=	cam_sim.c
SRCS+=	cam_xpt.c
SRCS+=	nvme_all.c
SRCS+=	nvme_da.c
SRCS+=	nvme_util.c
SRCS+=	nvme_xpt.c
SRCS+=	scsi_all.c
SRCS+=	scsi_cd.c
SRCS+=	scsi_ch.c
SRCS+=	scsi_da.c
SRCS+=	scsi_enc.c
SRCS+=	scsi_enc_safte.c
SRCS+=	scsi_enc_ses.c
SRCS+=	scsi_pass.c
SRCS+=	scsi_pt.c
SRCS+=	scsi_sa.c
SRCS+=	scsi_sg.c
SRCS+=	scsi_targ_bh.c
SRCS+=	scsi_target.c
SRCS+=	scsi_xpt.c
SRCS+=	smp_all.c

EXPORT_SYMS=	YES	# XXX evaluate

.include <bsd.kmod.mk>
