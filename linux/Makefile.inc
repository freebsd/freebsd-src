#
# Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
# Copyright (c) 2002-2008 Atheros Communications, Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: Makefile.inc,v 1.9 2008/11/10 03:37:38 sam Exp $
#
HAL=	..
PUBLIC=	${HAL}/public
DEBUG=	${HAL}/debug
#
# Makefile for building the Atheros/MADWIFI HAL from source code on Linux.
#
# This file is intended to be called by the Makefile in the hal
# directory with TARGET set to the desired target platform.
#
# There is support for multiple "generations" of Atheros hardware devices:
#
# AH_SUPPORT_AR5210	802.11a only MAC
# AH_SUPPORT_AR5211	802.11a+802.11b MAC (also pure g, but not yet supported)
# AH_SUPPORT_AR5212	802.11a+802.11b+802.11g MAC
# AH_SUPPORT_AR5312     5212 MAC WiSoC (requires AH_SUPPORT_AR5212)
# AH_SUPPORT_AR5416     802.11a+802.11g+802.11n MAC (requires AH_SUPPORT_5212)
# AH_SUPPORT_AR9160     802.11a+802.11g+802.11n MAC (requires AH_SUPPORT_5416)
#
# AH_SUPPORT_2133	2133 RF support for 5416
# AH_SUPPORT_2316	2316 RF (Cobra) support for 5212 et. al.
# AH_SUPPORT_2317	2317 RF (Spyder) support for 5212 et. al.
# AH_SUPPORT_2413	2413 RF support for 5212 et. al.
# AH_SUPPORT_2417	2425 RF (Nala) support for 5212 et. al.
# AH_SUPPORT_2425	2425 RF (Swan) support for 5212 et. al.
# AH_SUPPORT_5111	5111 RF support for 5212 et. al.
# AH_SUPPORT_5112	5112 RF support for 5212 et. al.
# AH_SUPPORT_5413	5413 RF support for 5212 et. al.
#
# and numerous optional debugging facilities (typically named AH_DEBUG_*):
#
# AH_DEBUG		enables debugging stuff that's controlled by the sysctl:
# 			hw.ath.hal.debug
# AH_ASSERT 		enables assertions that panic or trap into the debugger.
# AH_DEBUG_ALQ 		enables support for tracing register reads+writes
#			via alq
# AH_DEBUG_COUNTRY	accept the CTR_DEBUG country code (for debugging
#			the regulatory domain support)
# AH_REGOPS_FUNC	arrange for OS_REG_READ and OS_REG_WRITE to be function
#			calls and not inline expanded to memory references
# AH_ENABLE_AP_SUPPORT	enables AP-specific support not publicly available
#			(e.g. don't cap turbo power limits)
# AH_DISABLE_WME	disable HAL_TXQ_USE_LOCKOUT_BKOFF_DIS support
#
# AH_PRIVATE_DIAG	enable private diagnostic support
# AH_WRITE_EEPROM	permit EEPROM writes (use with AH_PRIVATE_DIAG)
# AH_WRITE_REGDOMAIN	permit EEPROM writes of the regulatory domain through
#			ah_setRegulatoryDomain (implies AH_WRITE_EEPROM)
#

#
# Default settings...
#
AH_SUPPORT_AR5210?=1		# 5210 support
AH_SUPPORT_AR5211?=1		# 5211 support
AH_SUPPORT_AR5212?=1		# 5212 support
AH_SUPPORT_AR5312?=0		# 5312 support
AH_SUPPORT_AR5416?=1		# 5416 support
AH_SUPPORT_AR9160?=0		# 9160 support
AH_SUPPORT_2316?=0		# 2316 RF support
AH_SUPPORT_2317?=0		# 2317 RF support
AH_SUPPORT_2133?=0		# 2133 RF support
AH_SUPPORT_2413?=1		# 2413 RF support
AH_SUPPORT_2417?=1		# 2417 RF support
AH_SUPPORT_2425?=1		# 2425 RF support
AH_SUPPORT_5111?=1		# 5111 RF support
AH_SUPPORT_5112?=1		# 5112 RF support
AH_SUPPORT_5413?=1		# 5413 RF support

AH_EEPROM_V3?=0			# v3+ EEPROM support
AH_EEPROM_V14?=0		# v14+ EEPROM support

AH_DEBUG?=0			# debugging (e.g. hw.ath.hal.debug)
AH_ASSERT?=0			# assertions (pretty lame)
AH_DEBUG_ALQ?=0			# ALQ register tracing support
AH_REGOPS_FUNC?=0		# use inline memory references
AH_WRITE_REGDOMAIN?=0		# support EEPROM writes of the regulatory domain
AH_DEBUG_COUNTRY?=0		# accept CTR_DEBUG country code
AH_DISABLE_WME?=0		# disable HAL_TXQ_USE_LOCKOUT_BKOFF_DIS
AH_ENABLE_AP_SUPPORT?=0		# enable AP-specific support
AH_WRITE_EEPROM?=0		# support EEPROM writes
AH_PRIVATE_DIAG?=0		# non-public diagnostic support
AH_NEED_DESC_SWAP?=0		# Need to swap TX descriptor
AH_ENABLE_FORCEBIAS?=0		# 2133 orientation sensitivity

#
# If 5312 support is enabled it's certain we're not going
# to use the 5210, 5211, or 5416 support so disable them.
#
ifeq ($(strip ${AH_SUPPORT_AR5312}),1)
AH_SUPPORT_AR5210=0
AH_SUPPORT_AR5211=0
AH_SUPPORT_AR5416=0
AH_SUPPORT_2133=0
AH_SUPPORT_2413=0
AH_SUPPORT_2417=0
AH_SUPPORT_2425=0
AH_SUPPORT_5413=0
AH_SUPPORT_AR5212=1		# force 5212 support
endif

#
# 5211 and 5212 require v3 EEPROM support; do it here
# before we enable 5212 to support 5416 (this way we don't
# pull in v3 support as a side effect).
#
ifeq ($(strip ${AH_SUPPORT_AR5212}),1)
AH_EEPROM_V3=1
endif
ifeq ($(strip ${AH_SUPPORT_AR5211}),1)
AH_EEPROM_V3=1
endif

#
# If 9160 support is enabled force 5416 on also.
#
ifeq ($(strip ${AH_SUPPORT_AR9160}),1)
AH_SUPPORT_AR5416=1
endif

#
# If 5416 support is enabled force 5212 on also.
#
ifeq ($(strip ${AH_SUPPORT_AR5416}),1)
AH_SUPPORT_AR5212=1
endif

#
# 5416 support presently =>'s 2133 and force bias support;
# do those automatically (but separately from 5212 for now).
#
ifeq ($(strip ${AH_SUPPORT_AR5416}),1)
AH_SUPPORT_2133=1
AH_ENABLE_FORCEBIAS=1
AH_EEPROM_V14=1
endif

#
# 2417 support presently requires 2425 support
#
ifeq ($(strip ${AH_SUPPORT_2417}),1)
AH_SUPPORT_2425=1
endif

#
# Basic build components.
#
OBJDIR=	obj/${TARGET}

WARNFLAGS=-Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes \
	-Wmissing-prototypes -Winline -Wcast-qual -std=c99
# NB: can't do this until we fix register r/w macros
WARNFLAGS_NOTYET=-Wpointer-arith
COPTS=	-Werror -g -O ${WARNFLAGS}
INCS=	-I${OBJDIR} -I. -I..
CLEANFILES=	${OBJDIR}/.depend

include ${PUBLIC}/${TARGET}.inc

CFLAGS=	${INCS} ${COPTS}

HAL_SRCS=${HAL}/ah.c ${HAL}/ah_regdomain.c
HAL_OBJS=${OBJDIR}/ah.o ${OBJDIR}/ah_regdomain.o

ifeq ($(strip ${AH_EEPROM_V3}),1)
HAL_SRCS+=${HAL}/ah_eeprom_v3.c
HAL_OBJS+=${OBJDIR}/ah_eeprom_v3.o
endif

ifeq ($(strip ${AH_EEPROM_V14}),1)
HAL_SRCS+=${HAL}/ah_eeprom_v14.c
HAL_OBJS+=${OBJDIR}/ah_eeprom_v14.o
endif

ifeq ($(strip ${AH_SUPPORT_AR5210}),1)
AR5210=	${HAL}/ar5210
HAL_SRCS+=${AR5210}/ar5210_attach.c ${AR5210}/ar5210_beacon.c \
	${AR5210}/ar5210_interrupts.c ${AR5210}/ar5210_keycache.c \
	${AR5210}/ar5210_misc.c ${AR5210}/ar5210_power.c \
	${AR5210}/ar5210_phy.c ${AR5210}/ar5210_recv.c \
	${AR5210}/ar5210_reset.c ${AR5210}/ar5210_xmit.c
HAL_OBJS+=${OBJDIR}/ar5210_attach.o ${OBJDIR}/ar5210_beacon.o \
	${OBJDIR}/ar5210_interrupts.o ${OBJDIR}/ar5210_keycache.o \
	${OBJDIR}/ar5210_misc.o ${OBJDIR}/ar5210_power.o \
	${OBJDIR}/ar5210_phy.o ${OBJDIR}/ar5210_recv.o \
	${OBJDIR}/ar5210_reset.o ${OBJDIR}/ar5210_xmit.o
endif

ifeq ($(strip ${AH_SUPPORT_AR5211}),1)
AR5211=	${HAL}/ar5211
HAL_SRCS+=${AR5211}/ar5211_attach.c ${AR5211}/ar5211_beacon.c \
	${AR5211}/ar5211_interrupts.c ${AR5211}/ar5211_keycache.c \
	${AR5211}/ar5211_misc.c ${AR5211}/ar5211_power.c \
	${AR5211}/ar5211_phy.c ${AR5211}/ar5211_recv.c \
	${AR5211}/ar5211_reset.c ${AR5211}/ar5211_xmit.c
HAL_OBJS+=${OBJDIR}/ar5211_attach.o ${OBJDIR}/ar5211_beacon.o \
	${OBJDIR}/ar5211_interrupts.o ${OBJDIR}/ar5211_keycache.o \
	${OBJDIR}/ar5211_misc.o ${OBJDIR}/ar5211_power.o \
	${OBJDIR}/ar5211_phy.o ${OBJDIR}/ar5211_recv.o \
	${OBJDIR}/ar5211_reset.o ${OBJDIR}/ar5211_xmit.o
endif

ifeq ($(strip ${AH_SUPPORT_AR5212}),1)
AR5212=	${HAL}/ar5212
HAL_SRCS+=${AR5212}/ar5212_attach.c ${AR5212}/ar5212_beacon.c \
	${AR5212}/ar5212_eeprom.c ${AR5212}/ar5212_gpio.c \
	${AR5212}/ar5212_interrupts.c ${AR5212}/ar5212_keycache.c \
	${AR5212}/ar5212_misc.c ${AR5212}/ar5212_power.c \
	${AR5212}/ar5212_phy.c ${AR5212}/ar5212_recv.c \
	${AR5212}/ar5212_reset.c ${AR5212}/ar5212_xmit.c \
	${AR5212}/ar5212_ani.c
HAL_OBJS+=${OBJDIR}/ar5212_attach.o ${OBJDIR}/ar5212_beacon.o \
	${OBJDIR}/ar5212_eeprom.o ${OBJDIR}/ar5212_gpio.o \
	${OBJDIR}/ar5212_interrupts.o ${OBJDIR}/ar5212_keycache.o \
	${OBJDIR}/ar5212_misc.o ${OBJDIR}/ar5212_power.o \
	${OBJDIR}/ar5212_phy.o ${OBJDIR}/ar5212_recv.o \
	${OBJDIR}/ar5212_reset.o ${OBJDIR}/ar5212_xmit.o \
	${OBJDIR}/ar5212_ani.o
endif

ifeq ($(strip ${AH_SUPPORT_AR5312}),1)
AR5312=	${HAL}/ar5312
HAL_SRCS+=${AR5312}/ar5312_attach.c ${AR5312}/ar5312_interrupts.c \
	${AR5312}/ar5312_eeprom.c \
	${AR5312}/ar5312_gpio.c ${AR5312}/ar5315_gpio.c \
	${AR5312}/ar5312_misc.c ${AR5312}/ar5312_power.c \
	${AR5312}/ar5312_reset.c
HAL_OBJS+=${OBJDIR}/ar5312_attach.o ${OBJDIR}/ar5312_interrupts.o \
	${OBJDIR}/ar5312_eeprom.o \
	${OBJDIR}/ar5312_gpio.o ${OBJDIR}/ar5315_gpio.o \
	${OBJDIR}/ar5312_misc.o ${OBJDIR}/ar5312_power.o \
	${OBJDIR}/ar5312_reset.o
endif

ifeq ($(strip ${AH_SUPPORT_AR5416}),1)
AR5416=	${HAL}/ar5416
HAL_SRCS+=${AR5416}/ar5416_attach.c ${AR5416}/ar5416_beacon.c \
	${AR5416}/ar5416_eeprom.c ${AR5416}/ar5416_gpio.c \
	${AR5416}/ar5416_interrupts.c ${AR5416}/ar5416_keycache.c \
	${AR5416}/ar5416_misc.c ${AR5416}/ar5416_power.c \
	${AR5416}/ar5416_phy.c ${AR5416}/ar5416_recv.c \
	${AR5416}/ar5416_reset.c ${AR5416}/ar5416_xmit.c
HAL_OBJS+=${OBJDIR}/ar5416_attach.o ${OBJDIR}/ar5416_beacon.o \
	${OBJDIR}/ar5416_eeprom.o ${OBJDIR}/ar5416_gpio.o \
	${OBJDIR}/ar5416_interrupts.o ${OBJDIR}/ar5416_keycache.o \
	${OBJDIR}/ar5416_misc.o ${OBJDIR}/ar5416_power.o \
	${OBJDIR}/ar5416_phy.o ${OBJDIR}/ar5416_recv.o \
	${OBJDIR}/ar5416_reset.o ${OBJDIR}/ar5416_xmit.o
endif

ifeq ($(strip ${AH_SUPPORT_AR9160}),1)
HAL_SRCS+=${AR5416}/ar9160_attach.c
HAL_OBJS+=${OBJDIR}/ar9160_attach.o
endif

ifeq ($(strip ${AH_SUPPORT_2316}),1)
HAL_SRCS+=${AR5212}/ar2316.c
HAL_OBJS+=${OBJDIR}/ar2316.o
endif

ifeq ($(strip ${AH_SUPPORT_2317}),1)
HAL_SRCS+=${AR5212}/ar2317.c
HAL_OBJS+=${OBJDIR}/ar2317.o
endif

ifeq ($(strip ${AH_SUPPORT_2133}),1)
HAL_SRCS+=${AR5416}/ar2133.c
HAL_OBJS+=${OBJDIR}/ar2133.o
endif

ifeq ($(strip ${AH_SUPPORT_2413}),1)
HAL_SRCS+=${AR5212}/ar2413.c
HAL_OBJS+=${OBJDIR}/ar2413.o
endif

ifeq ($(strip ${AH_SUPPORT_2425}),1)
HAL_SRCS+=${AR5212}/ar2425.c
HAL_OBJS+=${OBJDIR}/ar2425.o
endif

ifeq ($(strip ${AH_SUPPORT_5111}),1)
HAL_SRCS+=${AR5212}/ar5111.c
HAL_OBJS+=${OBJDIR}/ar5111.o
endif

ifeq ($(strip ${AH_SUPPORT_5112}),1)
HAL_SRCS+=${AR5212}/ar5112.c
HAL_OBJS+=${OBJDIR}/ar5112.o
endif

ifeq ($(strip ${AH_SUPPORT_5413}),1)
HAL_SRCS+=${AR5212}/ar5413.c
HAL_OBJS+=${OBJDIR}/ar5413.o
endif

SRCS+=	${HAL_SRCS}
CLEANFILES+= ${OBJDIR}/opt_ah.h ${HAL_OBJS}

#
# NB: These are typically overridden in the target-specific specification.
#
LD?=	ld
#
# These are specific to the host doing the build.
#
SED?=	sed
UUENCODE?=uuencode

ALL=	${OBJDIR}/hal.o

all:	${ALL}

#
# ARM binaries need their ELF header patched so they are marked
# as using "virtual floating point"; otherwise the resulting .o
# files will generate linker errors because they are marked as
# using hardware floating point.
#
ifneq (,$(findstring arm-elf,${TOOLPREFIX}))
${OBJDIR}/hal.o: ${HAL_OBJS} Makefile wackelf
	${LD} -o $@ -r ${LDOPTS} ${HAL_OBJS}
	./wackelf $@

# NB: use host tools; not cross-compilation tools
wackelf: ${PUBLIC}/wackelf.c
	cc -o wackelf ${PUBLIC}/wackelf.c

CLEANFILES+= wackelf
else
${OBJDIR}/hal.o: ${HAL_OBJS} Makefile
	${LD} -o $@ -r ${LDOPTS} ${HAL_OBJS}
endif

release: ${OBJDIR}/hal.o
	rm -f ${PUBLIC}/${TARGET}.hal.o.uu
	(${SED} -e '1,/^$$/d' ${HAL}/COPYRIGHT; \
	 ${SED} -n -e '/ATH_HAL_VERSION/p' ${HAL}/version.h; \
	 ${UUENCODE} ${OBJDIR}/hal.o hal.o) > ${PUBLIC}/${TARGET}.hal.o.uu
	cmp -s ${OBJDIR}/opt_ah.h ${PUBLIC}/${TARGET}.opt_ah.h || \
	    cp ${OBJDIR}/opt_ah.h ${PUBLIC}/${TARGET}.opt_ah.h

debug: ${OBJDIR}/hal.o
	rm -f ${DEBUG}/${TARGET}.hal.o.uu
	(${SED} -e '1,/^$$/d' ${HAL}/COPYRIGHT; \
	 ${SED} -n -e '/ATH_HAL_VERSION/p' ${HAL}/version.h; \
	 ${UUENCODE} ${OBJDIR}/hal.o hal.o) > ${DEBUG}/${TARGET}.hal.o.uu
	cmp -s ${OBJDIR}/opt_ah.h ${DEBUG}/${TARGET}.opt_ah.h || \
	    cp ${OBJDIR}/opt_ah.h ${DEBUG}/${TARGET}.opt_ah.h

${OBJDIR}/ah.o:	${HAL}/ah.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ah_eeprom_v3.o:	${HAL}/ah_eeprom_v3.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ah_eeprom_v14.o:	${HAL}/ah_eeprom_v14.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ah_regdomain.o:	${HAL}/ah_regdomain.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar5210_attach.o: ${AR5210}/ar5210_attach.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_beacon.o: ${AR5210}/ar5210_beacon.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_interrupts.o: ${AR5210}/ar5210_interrupts.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_keycache.o: ${AR5210}/ar5210_keycache.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_misc.o:	${AR5210}/ar5210_misc.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_power.o:	${AR5210}/ar5210_power.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_phy.o:	${AR5210}/ar5210_phy.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_recv.o:	${AR5210}/ar5210_recv.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_reset.o:	${AR5210}/ar5210_reset.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5210_xmit.o:	${AR5210}/ar5210_xmit.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar5211_attach.o: ${AR5211}/ar5211_attach.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_beacon.o: ${AR5211}/ar5211_beacon.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_interrupts.o: ${AR5211}/ar5211_interrupts.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_keycache.o: ${AR5211}/ar5211_keycache.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_misc.o:	${AR5211}/ar5211_misc.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_power.o:	${AR5211}/ar5211_power.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_phy.o:	${AR5211}/ar5211_phy.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_recv.o:	${AR5211}/ar5211_recv.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_reset.o:	${AR5211}/ar5211_reset.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5211_xmit.o:	${AR5211}/ar5211_xmit.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar5212_ani.o: ${AR5212}/ar5212_ani.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_attach.o: ${AR5212}/ar5212_attach.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_beacon.o: ${AR5212}/ar5212_beacon.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_eeprom.o: ${AR5212}/ar5212_eeprom.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_gpio.o: ${AR5212}/ar5212_gpio.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_interrupts.o: ${AR5212}/ar5212_interrupts.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_keycache.o: ${AR5212}/ar5212_keycache.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_misc.o:	${AR5212}/ar5212_misc.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_power.o:	${AR5212}/ar5212_power.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_phy.o:	${AR5212}/ar5212_phy.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_recv.o:	${AR5212}/ar5212_recv.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_reset.o:	${AR5212}/ar5212_reset.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5212_xmit.o:	${AR5212}/ar5212_xmit.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar5312_attach.o: ${AR5312}/ar5312_attach.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5312_eeprom.o: ${AR5312}/ar5312_eeprom.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5312_gpio.o: ${AR5312}/ar5312_gpio.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5315_gpio.o: ${AR5312}/ar5315_gpio.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5312_interrupts.o: ${AR5312}/ar5312_interrupts.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5312_misc.o:	${AR5312}/ar5312_misc.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5312_power.o: ${AR5312}/ar5312_power.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5312_reset.o:	${AR5312}/ar5312_reset.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar2316.o: ${AR5212}/ar2316.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar2317.o: ${AR5212}/ar2317.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar2413.o: ${AR5212}/ar2413.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar2425.o: ${AR5212}/ar2425.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5111.o: ${AR5212}/ar5111.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5112.o: ${AR5212}/ar5112.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5413.o: ${AR5212}/ar5413.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar5416_attach.o: ${AR5416}/ar5416_attach.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_beacon.o: ${AR5416}/ar5416_beacon.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_eeprom.o: ${AR5416}/ar5416_eeprom.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_gpio.o: ${AR5416}/ar5416_gpio.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_interrupts.o: ${AR5416}/ar5416_interrupts.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_keycache.o: ${AR5416}/ar5416_keycache.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_misc.o:	${AR5416}/ar5416_misc.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_power.o:	${AR5416}/ar5416_power.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_phy.o:	${AR5416}/ar5416_phy.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_recv.o:	${AR5416}/ar5416_recv.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_reset.o:	${AR5416}/ar5416_reset.c
	${CC} -c -o $@ ${CFLAGS} $<
${OBJDIR}/ar5416_xmit.o:	${AR5416}/ar5416_xmit.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar9160_attach.o: ${AR5416}/ar9160_attach.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/ar2133.o: ${AR5416}/ar2133.c
	${CC} -c -o $@ ${CFLAGS} $<

${OBJDIR}/opt_ah.h:
	test -d ${OBJDIR} || mkdir -p ${OBJDIR}
ifeq ($(strip ${AH_SUPPORT_AR5210}),1)
	echo "#define AH_SUPPORT_AR5210	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_AR5211}),1)
	echo "#define AH_SUPPORT_AR5211	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_AR5212}),1)
	echo "#define AH_SUPPORT_AR5212	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_AR5312}),1)
	echo "#define AH_SUPPORT_AR5312	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_AR5416}),1)
	echo "#define AH_SUPPORT_AR5416	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_AR9160}),1)
	echo "#define AH_SUPPORT_AR9160	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_2316}),1)
	echo "#define AH_SUPPORT_2316	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_2317}),1)
	echo "#define AH_SUPPORT_2317	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_2133}),1)
	echo "#define AH_SUPPORT_2133	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_2413}),1)
	echo "#define AH_SUPPORT_2413	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_2417}),1)
	echo "#define AH_SUPPORT_2417	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_2425}),1)
	echo "#define AH_SUPPORT_2425	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_5111}),1)
	echo "#define AH_SUPPORT_5111	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_5112}),1)
	echo "#define AH_SUPPORT_5112	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_SUPPORT_5413}),1)
	echo "#define AH_SUPPORT_5413	1"	>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_ASSERT}),1)
	echo "#define AH_ASSERT	1"		>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_DEBUG_ALQ}),1)
	echo "#define AH_DEBUG_ALQ 1"		>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_DEBUG}),1)
	echo "#define AH_DEBUG	1"		>> ${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_WRITE_REGDOMAIN}),1)
	echo "#define AH_SUPPORT_WRITE_REGDOMAIN 1" >>${OBJDIR}/opt_ah.h
	echo "#define AH_SUPPORT_WRITE_EEPROM 1">>${OBJDIR}/opt_ah.h
else
ifeq ($(strip ${AH_WRITE_EEPROM}),1)
	echo "#define AH_SUPPORT_WRITE_EEPROM 1">>${OBJDIR}/opt_ah.h
endif
endif
ifeq ($(strip ${AH_DEBUG_COUNTRY}),1)
	echo "#define AH_DEBUG_COUNTRY 1"	>>${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_REGOPS_FUNC}),1)
	echo "#define AH_REGOPS_FUNC 1"		>>${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_DISABLE_WME}),1)
	echo "#define AH_DISABLE_WME 1"		>>${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_ENABLE_AP_SUPPORT}),1)
	echo "#define AH_ENABLE_AP_SUPPORT 1"	>>${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_PRIVATE_DIAG}),1)
	echo "#define AH_PRIVATE_DIAG 1"	>>${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_NEED_DESC_SWAP}),1)
	echo "#define AH_NEED_DESC_SWAP 1"	>>${OBJDIR}/opt_ah.h
endif
ifeq ($(strip ${AH_ENABLE_FORCEBIAS}),1)
	echo "#define AH_ENABLE_FORCEBIAS	1"	>>${OBJDIR}/opt_ah.h
endif

clean:
	rm -f ${ALL} ${OBJS} ${CLEANFILES}
	-rmdir -p ${OBJDIR}

depend: beforedepend .depend afterdepend
${OBJDIR}/.depend: ${SRCS} ${OBJDIR}/opt_ah.h
	rm -f ${OBJDIR}/.depend
	${CC} -M ${CFLAGS} ${SRCS} | \
		${SED} 's,\(.*\)\.o:,${OBJDIR}/\1.o:,g' > ${OBJDIR}/.depend
afterdepend:
-include ${OBJDIR}/.depend
