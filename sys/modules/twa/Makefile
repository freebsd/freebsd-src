#
# Copyright (c) 2004 Applied Micro Circuits Corporation.
# All rights reserved.
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
#	$FreeBSD$
#

#
# 3ware driver for 9000 series storage controllers.
#
# Author: Vinod Kashyap
#



#
# In the line following this comment, a value of 0 on the right hand
# side will not cause firmware to be bundled.  Change this value to 1
# to bundle the firmware with the driver, which may be flashed onto
# the controller, if the firmware on the controller is older than the
# one bundled, and needs to be upgraded.  The size of the driver will
# increase significantly (to over 500kB) if this option is selected.
#
TWA_FLASH_FIRMWARE?=0

KMOD = twa
.PATH:	${.CURDIR}/../../dev/${KMOD}
SRCS = tw_osl_freebsd.c tw_osl_cam.c \
      tw_cl_init.c tw_cl_io.c tw_cl_intr.c tw_cl_misc.c \
      bus_if.h device_if.h pci_if.h opt_scsi.h opt_cam.h opt_twa.h

CFLAGS += -I${.CURDIR}/../../dev/${KMOD}

.if $(TWA_FLASH_FIRMWARE) != 0
CFLAGS += -DTWA_FLASH_FIRMWARE
SRCS += tw_cl_fwimg.c
.endif

#CFLAGS += -DTWA_DEBUG=0

.include <bsd.kmod.mk>

