
# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/aic7xxx
KMOD=	ahd

BEFORE_DEPEND= aic79xx_{seq,reg}.h

../aicasm/aicasm:
	( cd ${.CURDIR}/../aicasm; ${MAKE} aicasm; )

aic79xx_{seq,reg}.h: ${.CURDIR}/../../../dev/aic7xxx/aic79xx.{reg,seq} \
		   ${.CURDIR}/../../../cam/scsi/scsi_message.h ../aicasm/aicasm
	${.CURDIR}/../aicasm/aicasm ${INCLUDES} -I${.CURDIR}/../../../cam/scsi \
	-I${.CURDIR}/../../../dev/aic7xxx -o aic79xx_seq.h -r aic79xx_reg.h \
	${.CURDIR}/../../../dev/aic7xxx/aic79xx.seq

GENSRCS= aic79xx_reg.h aic79xx_seq.h

SRCS= ${GENSRCS}
SRCS+= aic79xx.c aic79xx_osm.c aic79xx_pci.c ahd_pci.c
SRCS+= opt_scsi.h opt_aic79xx.h opt_cam.h opt_ddb.h
SRCS+= device_if.h bus_if.h pci_if.h

CLEANFILES= ${GENSRCS}

.include <bsd.kmod.mk>
