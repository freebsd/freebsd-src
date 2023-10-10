#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
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

INTERFACE zynqmp_firmware;

METHOD int clock_enable {
	device_t dev;
	uint32_t clkid;
};

METHOD int clock_disable {
	device_t dev;
	uint32_t clkid;
};

METHOD int clock_getstate {
	device_t dev;
	uint32_t clkid;
	bool *enabled;
};

METHOD int clock_setdivider {
	device_t dev;
	uint32_t clkid;
	uint32_t div;
};

METHOD int clock_getdivider {
	device_t dev;
	uint32_t clkid;
	uint32_t *div;
};

METHOD int clock_setparent {
	device_t dev;
	uint32_t clkid;
	uint32_t parentid;
};

METHOD int clock_getparent {
	device_t dev;
	uint32_t clkid;
	uint32_t *parentid;
};

METHOD int pll_get_mode {
	device_t dev;
	uint32_t pllid;
	uint32_t *mode;
};

METHOD int pll_get_frac_data {
	device_t dev;
	uint32_t pllid;
	uint32_t *data;
};

METHOD int clock_get_fixedfactor {
	device_t dev;
	uint32_t clkid;
	uint32_t *mult;
	uint32_t *div;
};

METHOD int query_data {
	device_t dev;
	uint32_t qid;
	uint32_t arg1;
	uint32_t arg2;
	uint32_t arg3;
	uint32_t *data;
};

METHOD int reset_assert {
	device_t dev;
	uint32_t resetid;
	bool enable;
};

METHOD int reset_get_status {
	device_t dev;
	uint32_t resetid;
	bool *status;
};
