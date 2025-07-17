#-
# Copyright (c) 2015 Luiz Otavio O Souza <loos@FreeBSD.org>
# Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
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
#

INTERFACE if_dwc;

#include <dev/dwc/dwc1000_reg.h>

CODE {
	static int
	if_dwc_default_init(device_t dev)
	{
		return (0);
	}

	static int
	if_dwc_default_mii_clk(device_t dev)
	{
		return (GMAC_MII_CLK_25_35M_DIV16);
	}

	static int
	if_dwc_default_set_speed(device_t dev, int speed)
	{
		return (0);
	}
};

HEADER {
};

#
# Initialize the SoC specific registers.
#
METHOD int init {
	device_t dev;
} DEFAULT if_dwc_default_init;

#
# Return the DWC MII clock for a specific hardware.
#
METHOD int mii_clk {
	device_t dev;
} DEFAULT if_dwc_default_mii_clk;

#
# Signal media change to a specific hardware
#
METHOD int set_speed {
	device_t dev;
	int speed;
} DEFAULT if_dwc_default_set_speed;
