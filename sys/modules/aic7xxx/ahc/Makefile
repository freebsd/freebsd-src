
# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/aic7xxx
KMOD=	ahc
SUBDIR=	ahc_eisa ahc_pci

BEFORE_DEPEND= aic7xxx_{seq,reg}.h

../aicasm/aicasm:
	( cd ${.CURDIR}/../aicasm; ${MAKE} aicasm; )

aic7xxx_{seq.h,reg.h,reg_print.c}:					\
		${.CURDIR}/../../../dev/aic7xxx/aic7xxx.{reg,seq}	\
		${.CURDIR}/../../../cam/scsi/scsi_message.h ../aicasm/aicasm
	../aicasm/aicasm ${INCLUDES} -I${.CURDIR}/../../../cam/scsi	 \
			-I${.CURDIR}/../../../dev/aic7xxx		 \
			-o aic7xxx_seq.h -r aic7xxx_reg.h		 \
			-p aic7xxx_reg_print.c				 \
			-i ${.CURDIR}/../../../dev/aic7xxx/aic7xxx_osm.h \
			${.CURDIR}/../../../dev/aic7xxx/aic7xxx.seq

GENSRCS= aic7xxx_reg.h aic7xxx_seq.h

AHC_REG_PRETTY_PRINT=1
SRCS= ${GENSRCS}
.ifdef AHC_REG_PRETTY_PRINT
SRCS+= aic7xxx_reg_print.c
CFLAGS+= -DAHC_REG_PRETTY_PRINT=1
.endif
SRCS+= aic7xxx.c aic7xxx_93cx6.c aic7xxx_osm.c
SRCS+= opt_scsi.h opt_aic7xxx.h opt_cam.h
SRCS+= device_if.h bus_if.h pci_if.h

CLEANFILES= ${GENSRCS} aic7xxx_reg_print.c

.include <bsd.kmod.mk>
