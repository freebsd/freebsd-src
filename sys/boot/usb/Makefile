#
# $FreeBSD$
#
# Copyright (c) 2013 Hans Petter Selasky. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

T=${.CURDIR}/tools
S=${.CURDIR}/../..

.PATH: \
	${.CURDIR} \
	${S}/dev/usb \
	${S}/dev/usb/controller \
	${S}/dev/usb/serial \
	${S}/dev/usb/storage \
	${S}/dev/usb/template

LIB=		usbboot
INTERNALLIB=
OBJCOPY?=	objcopy
SYSCC?=		cc

CFLAGS+=	-DBOOTPROG=\"usbloader\"
CFLAGS+=	-DUSB_GLOBAL_INCLUDE_FILE=\"bsd_global.h\"
CFLAGS+=	-ffunction-sections -fdata-sections
CFLAGS+=	-ffreestanding
CFLAGS+=	-Wformat -Wall
CFLAGS+=	-I${S}
CFLAGS+=	-I${T}
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-g

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-march=i386
CFLAGS+=	-mpreferred-stack-boundary=2
.endif
.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-m32
.endif

#
# Single threaded BSD kernel
#
SRCS+=	bsd_kernel.c

#
# BUSSPACE implementation
#
SRCS+=	bsd_busspace.c

#
# BUSDMA implementation
#
SRCS+=	usb_busdma_loader.c

#
# USB controller drivers
#
SRCS+=	at91dci.c
SRCS+=	atmegadci.c
SRCS+=	avr32dci.c
SRCS+=	dwc_otg.c
SRCS+=	ehci.c
SRCS+=	musb_otg.c
SRCS+=	ohci.c
SRCS+=	uhci.c
SRCS+=	uss820dci.c
SRCS+=	xhci.c
SRCS+=	usb_controller.c

CFLAGS += -DUSB_PROBE_LIST="\"xhci\", \"ehci\", \"uhci\", \"ohci\""

#
# USB core and templates
#
SRCS+=	usb_core.c
SRCS+=	usb_debug.c
SRCS+=	usb_device.c
SRCS+=	usb_dynamic.c
SRCS+=	usb_error.c
SRCS+=	usb_handle_request.c
SRCS+=	usb_hid.c
SRCS+=	usb_hub.c
SRCS+=	usb_lookup.c
SRCS+=	usb_msctest.c
SRCS+=	usb_parse.c
SRCS+=	usb_request.c
SRCS+=	usb_transfer.c
SRCS+=	usb_util.c
SRCS+=	usb_template.c
SRCS+=	usb_template_cdce.c
SRCS+=	usb_template_msc.c
SRCS+=	usb_template_mtp.c
SRCS+=	usb_template_modem.c
SRCS+=	usb_template_mouse.c
SRCS+=	usb_template_kbd.c
SRCS+=	usb_template_audio.c
SRCS+=	sysinit_data.c
SRCS+=	sysuninit_data.c

CLEANFILES+= sysinit
CLEANFILES+= sysinit.bin
CLEANFILES+= sysinit_data.c
CLEANFILES+= sysuninit_data.c

CLEANFILES+= ${SRCS:C/\.c/.osys/g}

.include <bsd.lib.mk>

#
# SYSINIT() and SYSUNINIT() handling
#
sysinit: ${T}/sysinit.c
	${SYSCC} -Wall -o ${.TARGET} ${.ALLSRC}

sysinit_data.c: sysinit.bin sysinit
	${.OBJDIR}/sysinit -i sysinit.bin -o ${.TARGET} -k sysinit -s sysinit_data

sysuninit_data.c: sysinit.bin sysinit
	${.OBJDIR}/sysinit -i sysinit.bin -o ${.TARGET} -R -k sysuninit -s sysuninit_data

.for F in ${OBJS}
${F}sys: ${F}
	${OBJCOPY} -j ".debug.sysinit" -O binary ${F} ${.TARGET}
	[ -f ${.TARGET} ] || touch ${.TARGET}
.endfor

sysinit.bin: ${OBJS:C/\.o/.osys/g:C/sysinit_data.osys//g:C/sysuninit_data.osys//g}
	cat ${.ALLSRC} > sysinit.bin
