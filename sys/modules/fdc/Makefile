# $FreeBSD$

.if ${MACHINE} == "pc98"
.PATH:  ${.CURDIR}/../../pc98/pc98
.else
.PATH:  ${.CURDIR}/../../isa
.endif

KMOD=	fdc
SRCS=	fd.c \
	opt_fdc.h card.h \
	bus_if.h card_if.h device_if.h isa_if.h

FDC_DEBUG?=	0	# 0/1
FDC_PCCARD?=	1	# 0/1  whether pccard support (i. e. Y-E DATA PCMCIA
#			# fdc) is desired

CLEANFILES=	card.h

opt_fdc.h:
	touch ${.TARGET}
.if ${FDC_DEBUG} > 0
	echo "#define FDC_DEBUG 1" >> ${.TARGET}
.endif

card.h:
	touch ${.TARGET}
.if ${FDC_PCCARD} > 0
	echo "#define NCARD 1" >> ${.TARGET}
.endif

.include <bsd.kmod.mk>
