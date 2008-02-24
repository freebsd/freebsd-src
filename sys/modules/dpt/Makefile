# $FreeBSD: src/sys/modules/dpt/Makefile,v 1.2 2007/04/10 20:33:31 marius Exp $
#

.PATH:	${.CURDIR}/../../dev/dpt
KMOD=	dpt
SRCS=	dpt_scsi.c dpt.h \
	dpt_pci.c pci_if.h \
	opt_dpt.h opt_eisa.h \
	opt_cam.h opt_scsi.h \
	device_if.h bus_if.h

.if ${MACHINE} != "i386"
# Create an empty opt_eisa.h in order to keep kmod.mk from linking in an
# existing one from KERNBUILDDIR which possibly has DEV_EISA defined so
# dpt.ko is always built without EISA support.
opt_eisa.h:
	:> ${.TARGET}
.else
.if !defined(KERNBUILDDIR)
SRCS+=	dpt_eisa.c eisa_if.h
#SRCS+=	dpt_isa.c isa_if.h

opt_eisa.h:
	echo "#define DEV_EISA 1" > ${.TARGET}
.else
DEV_EISA!= sed -n '/DEV_EISA/p' ${KERNBUILDDIR}/opt_eisa.h
.if !empty(DEV_EISA)
SRCS+=	dpt_eisa.c eisa_if.h
.endif
#SRCS+=	dpt_isa.c isa_if.h
.endif
.endif

.include <bsd.kmod.mk>
