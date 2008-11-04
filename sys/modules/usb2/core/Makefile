#
# $FreeBSD$
#
# Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

S=     ${.CURDIR}/../../..

.PATH: $S/dev/usb2/core

KMOD=  usb2_core
SRCS= 
SRCS+= bus_if.h usb2_if.h device_if.h vnode_if.h opt_usb.h opt_bus.h
SRCS+= usb2_if.c
SRCS+= usb2_busdma.c
SRCS+= usb2_compat_linux.c
SRCS+= usb2_config_td.c
SRCS+= usb2_core.c
SRCS+= usb2_debug.c
SRCS+= usb2_dev.c
SRCS+= usb2_device.c
SRCS+= usb2_dynamic.c
SRCS+= usb2_error.c
SRCS+= usb2_generic.c
SRCS+= usb2_handle_request.c
SRCS+= usb2_hid.c
SRCS+= usb2_hub.c
SRCS+= usb2_lookup.c
SRCS+= usb2_mbuf.c
SRCS+= usb2_msctest.c
SRCS+= usb2_parse.c
SRCS+= usb2_process.c
SRCS+= usb2_request.c
SRCS+= usb2_sw_transfer.c
SRCS+= usb2_transfer.c
SRCS+= usb2_util.c

.include <bsd.kmod.mk>

