#
# Copyright (c) 2000,2001 Jonathan Chen.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions, and the following disclaimer,
#    without modification, immediately at the beginning of the file.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

#include <sys/bus.h>

INTERFACE pccbb;

METHOD int power_socket {
	device_t	dev;
	int		command;
};

METHOD int detect_card {
	device_t	dev;
};

METHOD int reset {
	device_t	dev;
};

HEADER {
/* result of detect_card */
	#define CARD_UKN_CARD	0x00
	#define CARD_5V_CARD	0x01
	#define CARD_3V_CARD	0x02
	#define CARD_XV_CARD	0x04
	#define CARD_YV_CARD	0x08

/* for power_socket */
	#define CARD_VCC_UC	0x0000
	#define CARD_VCC_3V	0x0001
	#define CARD_VCC_XV	0x0002
	#define CARD_VCC_YV	0x0003
	#define CARD_VCC_0V	0x0004
	#define CARD_VCC_5V	0x0005
	#define CARD_VCCMASK	0x000f
	#define CARD_VPP_UC	0x0000
	#define CARD_VPP_VCC	0x0010
	#define CARD_VPP_12V	0x0030
	#define CARD_VPP_0V	0x0040
	#define CARD_VPPMASK	0x00f0
};
