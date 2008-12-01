#
# Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer,
#    without modification.
# 2. Redistributions in binary form must reproduce at minimum a disclaimer
#    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
#    redistribution must be conditioned upon including a substantially
#    similar Disclaimer requirement for further binary redistribution.
#
# NO WARRANTY
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGES.
#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../dev/ath \
	${.CURDIR}/../../dev/ath/ath_hal \
	${.CURDIR}/../../dev/ath/ath_hal/ar5210 \
	${.CURDIR}/../../dev/ath/ath_hal/ar5211 \
	${.CURDIR}/../../dev/ath/ath_hal/ar5212 \
	${.CURDIR}/../../dev/ath/ath_hal/ar5416

AR5210_SRCS=ah_eeprom_v1.c \
	ar5210_attach.c ar5210_beacon.c ar5210_interrupts.c \
	ar5210_keycache.c ar5210_misc.c ar5210_phy.c ar5210_power.c \
	ar5210_recv.c ar5210_reset.c ar5210_xmit.c
AR5211_SRCS=ar5211_attach.c ar5211_beacon.c ar5211_interrupts.c \
	ar5211_keycache.c ar5211_misc.c	ar5211_phy.c ar5211_power.c \
	ar5211_recv.c ar5211_reset.c ar5211_xmit.c
AR5212_SRCS=ar5212_ani.c ar5212_attach.c ar5212_beacon.c ar5212_eeprom.c \
	ar5212_gpio.c ar5212_interrupts.c ar5212_keycache.c ar5212_misc.c \
	ar5212_phy.c ar5212_power.c ar5212_recv.c ar5212_reset.c \
	ar5212_rfgain.c ar5212_xmit.c \
	ar2413.c ar2425.c ar5111.c ar5112.c ar5413.c
AR5416_SRCS=ah_eeprom_v14.c \
	ar5416_ani.c ar5416_attach.c ar5416_beacon.c ar5416_cal.c \
	ar5416_cal_iq.c ar5416_cal_adcgain.c ar5416_cal_adcdc.c \
	ar5416_eeprom.c ar5416_gpio.c ar5416_interrupts.c ar5416_keycache.c \
	ar5416_misc.c ar5416_phy.c ar5416_power.c ar5416_recv.c \
	ar5416_reset.c ar5416_xmit.c \
	ar2133.c
AR9160_SRCS=ar9160_attach.c

KMOD=	if_ath
SRCS=	if_ath.c if_ath_pci.c
# NB: v3 eeprom support used by both AR5211 and AR5212; just include it
SRCS+=	ah_osdep.c ah.c ah_regdomain.c ah_eeprom_v3.c
SRCS+=	${AR5210_SRCS}
SRCS+=	${AR5211_SRCS}
SRCS+=	${AR5212_SRCS}
SRCS+=	${AR5416_SRCS}
SRCS+=	${AR9160_SRCS}
SRCS+=	device_if.h bus_if.h pci_if.h opt_inet.h opt_route.h opt_ath.h opt_ah.h

CFLAGS+=  -I. -I${.CURDIR}/../../dev/ath -I${.CURDIR}/../../dev/ath/ath_hal

opt_ah.h:
	echo '#define AH_SUPPORT_AR5416 1' > $@

.include <bsd.kmod.mk>
