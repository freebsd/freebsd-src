# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/sn
KMOD	= if_sn
SRCS	= if_sn.c if_sn_isa.c if_sn_pccard.c device_if.h bus_if.h isa_if.h \
	  card_if.h

.include <bsd.kmod.mk>
