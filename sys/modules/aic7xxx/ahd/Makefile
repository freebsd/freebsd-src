
# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/aic7xxx
KMOD=	ahd

BEFORE_DEPEND= aic79xx_{seq,reg}.h

../aicasm/aicasm:
	( cd ${.CURDIR}/../aicasm; ${MAKE} aicasm; )

aic79xx_{seq.h,reg.h,reg_print.c}:					\
		${.CURDIR}/../../../dev/aic7xxx/aic79xx.{reg,seq}	\
		${.CURDIR}/../../../cam/scsi/scsi_message.h ../aicasm/aicasm
	../aicasm/aicasm ${INCLUDES} -I${.CURDIR}/../../../cam/scsi	 \
			-I${.CURDIR}/../../../dev/aic7xxx		 \
			-o aic79xx_seq.h -r aic79xx_reg.h		 \
			-p aic79xx_reg_print.c				 \
			-i ${.CURDIR}/../../../dev/aic7xxx/aic79xx_osm.h \
			${.CURDIR}/../../../dev/aic7xxx/aic79xx.seq

GENSRCS= aic79xx_reg.h aic79xx_seq.h

SRCS= ${GENSRCS}
AHD_REG_PRETTY_PRINT=1
.ifdef AHD_REG_PRETTY_PRINT
SRCS+= aic79xx_reg_print.c
CFLAGS+= -DAHD_REG_PRETTY_PRINT=1
.endif
SRCS+= aic79xx.c aic79xx_osm.c aic79xx_pci.c ahd_pci.c
SRCS+= opt_scsi.h opt_aic79xx.h opt_cam.h opt_ddb.h
SRCS+= device_if.h bus_if.h pci_if.h

CLEANFILES= ${GENSRCS} aic79xx_reg_print.c

.include <bsd.kmod.mk>
