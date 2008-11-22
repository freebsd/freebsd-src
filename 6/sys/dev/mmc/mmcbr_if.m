#-
# Copyright (c) 2006 M. Warner Losh
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
# $FreeBSD$
#

#include <sys/types.h>
#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>

#
# This is the interface that a mmc bridge chip gives to the mmc bus
# that attaches to the mmc bridge.
#

INTERFACE mmcbr;

#
# Called by the mmcbus to setup the IO pins correctly, the voltage to use
# for the card, the type of selects, power modes and bus width.
#
METHOD int update_ios {
	device_t	brdev;
	device_t	reqdev;
};

#
# Called by the mmcbus or its children to schedule a mmc request.  These
# requests are queued.  Time passes.  The bridge then gets notification
# of the status of request, who then notifies the requesting device via
# the xfer_done mmcbus method.
#
METHOD int request {
	device_t	brdev;
	device_t	reqdev;
	struct mmc_request *req;
};

#
# Called by mmcbus to get the read only status bits.
#
METHOD int get_ro {
	device_t	brdev;
	device_t	reqdev;
};

#
# Claim the current bridge, blocking the current thread until the host
# is no longer busy.
#
METHOD int acquire_host {
	device_t	brdev;
	device_t	reqdev;
}

#
# Release the current bridge.
#
METHOD int release_host {
	device_t	brdev;
	device_t	reqdev;
}
