# $FreeBSD$

.PATH:  ${.CURDIR}/../../pccard
KMOD=   oldcard
SRCS=   pccard.c pcic.c pccard_beep.c pccard_nbk.c \
	opt_pcic.h opt_compat_oldisa.h \
	device_if.h bus_if.h isa_if.h \
	power_if.h card_if.h power_if.c card_if.c
NOMAN=

opt_pcic.h:
	touch $@

opt_compat_oldisa.h:
	touch $@

.include <bsd.kmod.mk>
