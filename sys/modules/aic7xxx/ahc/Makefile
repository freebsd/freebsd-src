
# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/aic7xxx
KMOD=	ahc
SUBDIR=	ahc_eisa ahc_pci

BEFORE_DEPEND= aic7xxx_{seq,reg}.h

../aicasm/aicasm:
	( cd ${.CURDIR}/../aicasm; ${MAKE} aicasm; )

aic7xxx_{seq,reg}.h: ${.CURDIR}/../../../dev/aic7xxx/aic7xxx.{reg,seq} \
		   ${.CURDIR}/../../../cam/scsi/scsi_message.h ../aicasm/aicasm
	${.CURDIR}/../aicasm/aicasm ${INCLUDES} -I${.CURDIR}/../../../cam/scsi \
	-I${.CURDIR}/../../../dev/aic7xxx -o aic7xxx_seq.h -r aic7xxx_reg.h \
	${.CURDIR}/../../../dev/aic7xxx/aic7xxx.seq

GENSRCS= aic7xxx_reg.h aic7xxx_seq.h

SRCS= ${GENSRCS}
SRCS+= aic7xxx.c aic7xxx_93cx6.c aic7xxx_osm.c
SRCS+= opt_scsi.h opt_aic7xxx.h opt_cam.h
SRCS+= device_if.h bus_if.h pci_if.h

CLEANFILES= ${GENSRCS}

.include <bsd.kmod.mk>
