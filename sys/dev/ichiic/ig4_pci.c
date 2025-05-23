/*
 * Copyright (c) 2014 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com> and was subsequently ported
 * to FreeBSD by Michael Gmelin <freebsd@grem.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * Intel fourth generation mobile cpus integrated I2C device.
 *
 * See ig4_reg.h for datasheet reference and notes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ichiic/ig4_reg.h>
#include <dev/ichiic/ig4_var.h>

static int ig4iic_pci_detach(device_t dev);

#define PCI_CHIP_BAYTRAIL_I2C_1 	0x0f418086
#define PCI_CHIP_BAYTRAIL_I2C_2 	0x0f428086
#define PCI_CHIP_BAYTRAIL_I2C_3 	0x0f438086
#define PCI_CHIP_BAYTRAIL_I2C_4 	0x0f448086
#define PCI_CHIP_BAYTRAIL_I2C_5 	0x0f458086
#define PCI_CHIP_BAYTRAIL_I2C_6 	0x0f468086
#define PCI_CHIP_BAYTRAIL_I2C_7 	0x0f478086
#define PCI_CHIP_LYNXPT_LP_I2C_1	0x9c618086
#define PCI_CHIP_LYNXPT_LP_I2C_2	0x9c628086
#define PCI_CHIP_BRASWELL_I2C_1 	0x22c18086
#define PCI_CHIP_BRASWELL_I2C_2 	0x22c28086
#define PCI_CHIP_BRASWELL_I2C_3 	0x22c38086
#define PCI_CHIP_BRASWELL_I2C_5 	0x22c58086
#define PCI_CHIP_BRASWELL_I2C_6 	0x22c68086
#define PCI_CHIP_BRASWELL_I2C_7 	0x22c78086
#define PCI_CHIP_SKYLAKE_I2C_0		0x9d608086
#define PCI_CHIP_SKYLAKE_I2C_1		0x9d618086
#define PCI_CHIP_SKYLAKE_I2C_2		0x9d628086
#define PCI_CHIP_SKYLAKE_I2C_3		0x9d638086
#define PCI_CHIP_SKYLAKE_I2C_4		0x9d648086
#define PCI_CHIP_SKYLAKE_I2C_5		0x9d658086
#define PCI_CHIP_KABYLAKE_I2C_0		0xa1608086
#define PCI_CHIP_KABYLAKE_I2C_1		0xa1618086
#define PCI_CHIP_APL_I2C_0		0x5aac8086
#define PCI_CHIP_APL_I2C_1		0x5aae8086
#define PCI_CHIP_APL_I2C_2		0x5ab08086
#define PCI_CHIP_APL_I2C_3		0x5ab28086
#define PCI_CHIP_APL_I2C_4		0x5ab48086
#define PCI_CHIP_APL_I2C_5		0x5ab68086
#define PCI_CHIP_APL_I2C_6		0x5ab88086
#define PCI_CHIP_APL_I2C_7		0x5aba8086
#define PCI_CHIP_CANNONLAKE_LP_I2C_0	0x9dc58086
#define PCI_CHIP_CANNONLAKE_LP_I2C_1	0x9dc68086
#define PCI_CHIP_CANNONLAKE_LP_I2C_2	0x9de88086
#define PCI_CHIP_CANNONLAKE_LP_I2C_3	0x9de98086
#define PCI_CHIP_CANNONLAKE_LP_I2C_4	0x9dea8086
#define PCI_CHIP_CANNONLAKE_LP_I2C_5	0x9deb8086
#define PCI_CHIP_CANNONLAKE_H_I2C_0	0xa3688086
#define PCI_CHIP_CANNONLAKE_H_I2C_1	0xa3698086
#define PCI_CHIP_CANNONLAKE_H_I2C_2	0xa36a8086
#define PCI_CHIP_CANNONLAKE_H_I2C_3	0xa36b8086
#define PCI_CHIP_COMETLAKE_LP_I2C_0	0x02e88086
#define PCI_CHIP_COMETLAKE_LP_I2C_1	0x02e98086
#define PCI_CHIP_COMETLAKE_LP_I2C_2	0x02ea8086
#define PCI_CHIP_COMETLAKE_LP_I2C_3	0x02eb8086
#define PCI_CHIP_COMETLAKE_LP_I2C_4	0x02c58086
#define PCI_CHIP_COMETLAKE_LP_I2C_5	0x02c68086
#define PCI_CHIP_COMETLAKE_H_I2C_0	0x06e88086
#define PCI_CHIP_COMETLAKE_H_I2C_1	0x06e98086
#define PCI_CHIP_COMETLAKE_H_I2C_2	0x06ea8086
#define PCI_CHIP_COMETLAKE_H_I2C_3	0x06eb8086
#define PCI_CHIP_COMETLAKE_V_I2C_0	0xa3e08086
#define PCI_CHIP_COMETLAKE_V_I2C_1	0xa3e18086
#define PCI_CHIP_COMETLAKE_V_I2C_2	0xa3e28086
#define PCI_CHIP_COMETLAKE_V_I2C_3	0xa3e38086
#define	PCI_CHIP_ICELAKE_LP_I2C_0	0x34e88086
#define	PCI_CHIP_ICELAKE_LP_I2C_1	0x34e98086
#define	PCI_CHIP_ICELAKE_LP_I2C_2	0x34ea8086
#define	PCI_CHIP_ICELAKE_LP_I2C_3	0x34eb8086
#define	PCI_CHIP_ICELAKE_LP_I2C_4	0x34c58086
#define	PCI_CHIP_ICELAKE_LP_I2C_5	0x34c68086
#define PCI_CHIP_TIGERLAKE_H_I2C_0	0x43d88086
#define PCI_CHIP_TIGERLAKE_H_I2C_1	0x43e88086
#define PCI_CHIP_TIGERLAKE_H_I2C_2	0x43e98086
#define PCI_CHIP_TIGERLAKE_H_I2C_3	0x43ea8086
#define PCI_CHIP_TIGERLAKE_H_I2C_4	0x43eb8086
#define PCI_CHIP_TIGERLAKE_H_I2C_5	0x43ad8086
#define PCI_CHIP_TIGERLAKE_H_I2C_6	0x43ae8086
#define PCI_CHIP_TIGERLAKE_LP_I2C_0	0xa0c58086
#define PCI_CHIP_TIGERLAKE_LP_I2C_1	0xa0c68086
#define PCI_CHIP_TIGERLAKE_LP_I2C_2	0xa0d88086
#define PCI_CHIP_TIGERLAKE_LP_I2C_3	0xa0d98086
#define PCI_CHIP_TIGERLAKE_LP_I2C_4	0xa0e88086
#define PCI_CHIP_TIGERLAKE_LP_I2C_5	0xa0e98086
#define PCI_CHIP_TIGERLAKE_LP_I2C_6	0xa0ea8086
#define PCI_CHIP_TIGERLAKE_LP_I2C_7	0xa0eb8086
#define PCI_CHIP_GEMINILAKE_I2C_0	0x31ac8086
#define PCI_CHIP_GEMINILAKE_I2C_1	0x31ae8086
#define PCI_CHIP_GEMINILAKE_I2C_2	0x31b08086
#define PCI_CHIP_GEMINILAKE_I2C_3	0x31b28086
#define PCI_CHIP_GEMINILAKE_I2C_4	0x31b48086
#define PCI_CHIP_GEMINILAKE_I2C_5	0x31b68086
#define PCI_CHIP_GEMINILAKE_I2C_6	0x31b88086
#define PCI_CHIP_GEMINILAKE_I2C_7	0x31ba8086
#define PCI_CHIP_JASPERLAKE_I2C_0	0x4de88086
#define PCI_CHIP_JASPERLAKE_I2C_1	0x4de98086
#define PCI_CHIP_JASPERLAKE_I2C_2	0x4dea8086
#define PCI_CHIP_JASPERLAKE_I2C_3	0x4deb8086
#define PCI_CHIP_JASPERLAKE_I2C_4	0x4dc58086
#define PCI_CHIP_JASPERLAKE_I2C_5	0x4dc68086
#define PCI_CHIP_ALDERLAKE_P_I2C_0	0x51e88086
#define PCI_CHIP_ALDERLAKE_P_I2C_1	0x51e98086
#define PCI_CHIP_ALDERLAKE_P_I2C_2	0x51ea8086
#define PCI_CHIP_ALDERLAKE_P_I2C_3	0x51eb8086
#define PCI_CHIP_ALDERLAKE_P_I2C_4	0x51c58086
#define PCI_CHIP_ALDERLAKE_P_I2C_5	0x51c68086
#define PCI_CHIP_ALDERLAKE_P_I2C_6	0x51d88086
#define PCI_CHIP_ALDERLAKE_P_I2C_7	0x51d98086
#define PCI_CHIP_ALDERLAKE_S_I2C_0	0x7acc8086
#define PCI_CHIP_ALDERLAKE_S_I2C_1	0x7acd8086
#define PCI_CHIP_ALDERLAKE_S_I2C_2	0x7ace8086
#define PCI_CHIP_ALDERLAKE_S_I2C_3	0x7acf8086
#define PCI_CHIP_ALDERLAKE_S_I2C_4	0x7afc8086
#define PCI_CHIP_ALDERLAKE_S_I2C_5	0x7afd8086
#define PCI_CHIP_ALDERLAKE_M_I2C_0	0x54e88086
#define PCI_CHIP_ALDERLAKE_M_I2C_1	0x54e98086
#define PCI_CHIP_ALDERLAKE_M_I2C_2	0x54ea8086
#define PCI_CHIP_ALDERLAKE_M_I2C_3	0x54eb8086
#define PCI_CHIP_ALDERLAKE_M_I2C_4	0x54c58086
#define PCI_CHIP_ALDERLAKE_M_I2C_5	0x54c68086
#define PCI_CHIP_RAPTORLAKE_S_I2C_0	0x7a4c8086
#define PCI_CHIP_RAPTORLAKE_S_I2C_1	0x7a4d8086
#define PCI_CHIP_RAPTORLAKE_S_I2C_2	0x7a4e8086
#define PCI_CHIP_RAPTORLAKE_S_I2C_3	0x7a4f8086
#define PCI_CHIP_RAPTORLAKE_S_I2C_4	0x7a7c8086
#define PCI_CHIP_RAPTORLAKE_S_I2C_5	0x7a7d8086
#define PCI_CHIP_METEORLAKE_M_I2C_0	0x7e788086
#define PCI_CHIP_METEORLAKE_M_I2C_1	0x7e798086
#define PCI_CHIP_METEORLAKE_M_I2C_2	0x7e508086
#define PCI_CHIP_METEORLAKE_M_I2C_3	0x7e518086
#define PCI_CHIP_METEORLAKE_M_I2C_4	0x7e7a8086
#define PCI_CHIP_METEORLAKE_M_I2C_5	0x7e7b8086

struct ig4iic_pci_device {
	uint32_t	devid;
	const char	*desc;
	enum ig4_vers	version;
};

static struct ig4iic_pci_device ig4iic_pci_devices[] = {
	{ PCI_CHIP_BAYTRAIL_I2C_1, "Intel BayTrail Serial I/O I2C Port 1", IG4_ATOM},
	{ PCI_CHIP_BAYTRAIL_I2C_2, "Intel BayTrail Serial I/O I2C Port 2", IG4_ATOM},
	{ PCI_CHIP_BAYTRAIL_I2C_3, "Intel BayTrail Serial I/O I2C Port 3", IG4_ATOM},
	{ PCI_CHIP_BAYTRAIL_I2C_4, "Intel BayTrail Serial I/O I2C Port 4", IG4_ATOM},
	{ PCI_CHIP_BAYTRAIL_I2C_5, "Intel BayTrail Serial I/O I2C Port 5", IG4_ATOM},
	{ PCI_CHIP_BAYTRAIL_I2C_6, "Intel BayTrail Serial I/O I2C Port 6", IG4_ATOM},
	{ PCI_CHIP_BAYTRAIL_I2C_7, "Intel BayTrail Serial I/O I2C Port 7", IG4_ATOM},
	{ PCI_CHIP_LYNXPT_LP_I2C_1, "Intel Lynx Point-LP I2C Controller-1", IG4_HASWELL},
	{ PCI_CHIP_LYNXPT_LP_I2C_2, "Intel Lynx Point-LP I2C Controller-2", IG4_HASWELL},
	{ PCI_CHIP_BRASWELL_I2C_1, "Intel Braswell Serial I/O I2C Port 1", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_2, "Intel Braswell Serial I/O I2C Port 2", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_3, "Intel Braswell Serial I/O I2C Port 3", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_5, "Intel Braswell Serial I/O I2C Port 5", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_6, "Intel Braswell Serial I/O I2C Port 6", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_7, "Intel Braswell Serial I/O I2C Port 7", IG4_ATOM},
	{ PCI_CHIP_SKYLAKE_I2C_0, "Intel Sunrise Point-LP I2C Controller-0", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_1, "Intel Sunrise Point-LP I2C Controller-1", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_2, "Intel Sunrise Point-LP I2C Controller-2", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_3, "Intel Sunrise Point-LP I2C Controller-3", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_4, "Intel Sunrise Point-LP I2C Controller-4", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_5, "Intel Sunrise Point-LP I2C Controller-5", IG4_SKYLAKE},
	{ PCI_CHIP_KABYLAKE_I2C_0, "Intel Sunrise Point-H I2C Controller-0", IG4_SKYLAKE},
	{ PCI_CHIP_KABYLAKE_I2C_1, "Intel Sunrise Point-H I2C Controller-1", IG4_SKYLAKE},
	{ PCI_CHIP_APL_I2C_0, "Intel Apollo Lake I2C Controller-0", IG4_APL},
	{ PCI_CHIP_APL_I2C_1, "Intel Apollo Lake I2C Controller-1", IG4_APL},
	{ PCI_CHIP_APL_I2C_2, "Intel Apollo Lake I2C Controller-2", IG4_APL},
	{ PCI_CHIP_APL_I2C_3, "Intel Apollo Lake I2C Controller-3", IG4_APL},
	{ PCI_CHIP_APL_I2C_4, "Intel Apollo Lake I2C Controller-4", IG4_APL},
	{ PCI_CHIP_APL_I2C_5, "Intel Apollo Lake I2C Controller-5", IG4_APL},
	{ PCI_CHIP_APL_I2C_6, "Intel Apollo Lake I2C Controller-6", IG4_APL},
	{ PCI_CHIP_APL_I2C_7, "Intel Apollo Lake I2C Controller-7", IG4_APL},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_0, "Intel Cannon Lake-LP I2C Controller-0", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_1, "Intel Cannon Lake-LP I2C Controller-1", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_2, "Intel Cannon Lake-LP I2C Controller-2", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_3, "Intel Cannon Lake-LP I2C Controller-3", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_4, "Intel Cannon Lake-LP I2C Controller-4", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_5, "Intel Cannon Lake-LP I2C Controller-5", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_0, "Intel Cannon Lake-H I2C Controller-0", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_1, "Intel Cannon Lake-H I2C Controller-1", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_2, "Intel Cannon Lake-H I2C Controller-2", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_3, "Intel Cannon Lake-H I2C Controller-3", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_LP_I2C_0, "Intel Comet Lake-LP I2C Controller-0", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_LP_I2C_1, "Intel Comet Lake-LP I2C Controller-1", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_LP_I2C_2, "Intel Comet Lake-LP I2C Controller-2", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_LP_I2C_3, "Intel Comet Lake-LP I2C Controller-3", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_LP_I2C_4, "Intel Comet Lake-LP I2C Controller-4", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_LP_I2C_5, "Intel Comet Lake-LP I2C Controller-5", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_H_I2C_0, "Intel Comet Lake-H I2C Controller-0", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_H_I2C_1, "Intel Comet Lake-H I2C Controller-1", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_H_I2C_2, "Intel Comet Lake-H I2C Controller-2", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_H_I2C_3, "Intel Comet Lake-H I2C Controller-3", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_V_I2C_0, "Intel Comet Lake-V I2C Controller-0", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_V_I2C_1, "Intel Comet Lake-V I2C Controller-1", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_V_I2C_2, "Intel Comet Lake-V I2C Controller-2", IG4_CANNONLAKE},
	{ PCI_CHIP_COMETLAKE_V_I2C_3, "Intel Comet Lake-V I2C Controller-3", IG4_CANNONLAKE},
	{ PCI_CHIP_ICELAKE_LP_I2C_0, "Intel Ice Lake-LP I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_ICELAKE_LP_I2C_1, "Intel Ice Lake-LP I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_ICELAKE_LP_I2C_2, "Intel Ice Lake-LP I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_ICELAKE_LP_I2C_3, "Intel Ice Lake-LP I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_ICELAKE_LP_I2C_4, "Intel Ice Lake-LP I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_ICELAKE_LP_I2C_5, "Intel Ice Lake-LP I2C Controller-5", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_H_I2C_0, "Intel Tiger Lake-H I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_H_I2C_1, "Intel Tiger Lake-H I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_H_I2C_2, "Intel Tiger Lake-H I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_H_I2C_3, "Intel Tiger Lake-H I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_H_I2C_4, "Intel Tiger Lake-H I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_H_I2C_5, "Intel Tiger Lake-H I2C Controller-5", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_H_I2C_6, "Intel Tiger Lake-H I2C Controller-6", IG4_TIGERLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_0, "Intel Tiger Lake-LP I2C Controller-0", IG4_SKYLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_1, "Intel Tiger Lake-LP I2C Controller-1", IG4_SKYLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_2, "Intel Tiger Lake-LP I2C Controller-2", IG4_SKYLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_3, "Intel Tiger Lake-LP I2C Controller-3", IG4_SKYLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_4, "Intel Tiger Lake-LP I2C Controller-4", IG4_SKYLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_5, "Intel Tiger Lake-LP I2C Controller-5", IG4_SKYLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_6, "Intel Tiger Lake-LP I2C Controller-6", IG4_SKYLAKE},
	{ PCI_CHIP_TIGERLAKE_LP_I2C_7, "Intel Tiger Lake-LP I2C Controller-7", IG4_SKYLAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_0, "Intel Gemini Lake I2C Controller-0", IG4_GEMINILAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_1, "Intel Gemini Lake I2C Controller-1", IG4_GEMINILAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_2, "Intel Gemini Lake I2C Controller-2", IG4_GEMINILAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_3, "Intel Gemini Lake I2C Controller-3", IG4_GEMINILAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_4, "Intel Gemini Lake I2C Controller-4", IG4_GEMINILAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_5, "Intel Gemini Lake I2C Controller-5", IG4_GEMINILAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_6, "Intel Gemini Lake I2C Controller-6", IG4_GEMINILAKE},
	{ PCI_CHIP_GEMINILAKE_I2C_7, "Intel Gemini Lake I2C Controller-7", IG4_GEMINILAKE},
	{ PCI_CHIP_JASPERLAKE_I2C_0, "Intel Jasper Lake I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_JASPERLAKE_I2C_1, "Intel Jasper Lake I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_JASPERLAKE_I2C_2, "Intel Jasper Lake I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_JASPERLAKE_I2C_3, "Intel Jasper Lake I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_JASPERLAKE_I2C_4, "Intel Jasper Lake I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_JASPERLAKE_I2C_5, "Intel Jasper Lake I2C Controller-5", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_0, "Intel Alder Lake-P I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_1, "Intel Alder Lake-P I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_2, "Intel Alder Lake-P I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_3, "Intel Alder Lake-P I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_4, "Intel Alder Lake-P I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_5, "Intel Alder Lake-P I2C Controller-5", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_6, "Intel Alder Lake-P I2C Controller-6", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_P_I2C_7, "Intel Alder Lake-P I2C Controller-7", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_S_I2C_0, "Intel Alder Lake-S I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_S_I2C_1, "Intel Alder Lake-S I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_S_I2C_2, "Intel Alder Lake-S I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_S_I2C_3, "Intel Alder Lake-S I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_S_I2C_4, "Intel Alder Lake-S I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_S_I2C_5, "Intel Alder Lake-S I2C Controller-5", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_M_I2C_0, "Intel Alder Lake-M I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_M_I2C_1, "Intel Alder Lake-M I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_M_I2C_2, "Intel Alder Lake-M I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_M_I2C_3, "Intel Alder Lake-M I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_M_I2C_4, "Intel Alder Lake-M I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_ALDERLAKE_M_I2C_5, "Intel Alder Lake-M I2C Controller-5", IG4_TIGERLAKE},
	{ PCI_CHIP_RAPTORLAKE_S_I2C_0, "Intel Raptor Lake-S I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_RAPTORLAKE_S_I2C_1, "Intel Raptor Lake-S I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_RAPTORLAKE_S_I2C_2, "Intel Raptor Lake-S I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_RAPTORLAKE_S_I2C_3, "Intel Raptor Lake-S I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_RAPTORLAKE_S_I2C_4, "Intel Raptor Lake-S I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_RAPTORLAKE_S_I2C_5, "Intel Raptor Lake-S I2C Controller-5", IG4_TIGERLAKE},
	{ PCI_CHIP_METEORLAKE_M_I2C_0, "Intel Meteor Lake-M I2C Controller-0", IG4_TIGERLAKE},
	{ PCI_CHIP_METEORLAKE_M_I2C_1, "Intel Meteor Lake-M I2C Controller-1", IG4_TIGERLAKE},
	{ PCI_CHIP_METEORLAKE_M_I2C_2, "Intel Meteor Lake-M I2C Controller-2", IG4_TIGERLAKE},
	{ PCI_CHIP_METEORLAKE_M_I2C_3, "Intel Meteor Lake-M I2C Controller-3", IG4_TIGERLAKE},
	{ PCI_CHIP_METEORLAKE_M_I2C_4, "Intel Meteor Lake-M I2C Controller-4", IG4_TIGERLAKE},
	{ PCI_CHIP_METEORLAKE_M_I2C_5, "Intel Meteor Lake-M I2C Controller-5", IG4_TIGERLAKE},
};

static int
ig4iic_pci_probe(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	uint32_t devid;
	int i;

	devid = pci_get_devid(dev);
	for (i = 0; i < nitems(ig4iic_pci_devices); i++) {
		if (ig4iic_pci_devices[i].devid == devid) {
			device_set_desc(dev, ig4iic_pci_devices[i].desc);
			sc->version = ig4iic_pci_devices[i].version;
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static int
ig4iic_pci_attach(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	sc->dev = dev;
	sc->regs_rid = PCIR_BAR(0);
	sc->regs_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
					  &sc->regs_rid, RF_ACTIVE);
	if (sc->regs_res == NULL) {
		device_printf(dev, "unable to map registers\n");
		ig4iic_pci_detach(dev);
		return (ENXIO);
	}
	sc->intr_rid = 0;
	if (pci_alloc_msi(dev, &sc->intr_rid)) {
		device_printf(dev, "Using MSI\n");
	}
	sc->intr_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
					  &sc->intr_rid, RF_SHAREABLE | RF_ACTIVE);
	if (sc->intr_res == NULL) {
		device_printf(dev, "unable to map interrupt\n");
		ig4iic_pci_detach(dev);
		return (ENXIO);
	}
	sc->platform_attached = true;

	error = ig4iic_attach(sc);
	if (error)
		ig4iic_pci_detach(dev);

	return (error);
}

static int
ig4iic_pci_detach(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	if (sc->platform_attached) {
		error = ig4iic_detach(sc);
		if (error)
			return (error);
		sc->platform_attached = false;
	}

	if (sc->intr_res) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->intr_rid, sc->intr_res);
		sc->intr_res = NULL;
	}
	if (sc->intr_rid != 0)
		pci_release_msi(dev);
	if (sc->regs_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->regs_rid, sc->regs_res);
		sc->regs_res = NULL;
	}

	return (0);
}

static int
ig4iic_pci_suspend(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);

	return (ig4iic_suspend(sc));
}

static int
ig4iic_pci_resume(device_t dev)
{
	ig4iic_softc_t *sc  = device_get_softc(dev);

	return (ig4iic_resume(sc));
}

static device_method_t ig4iic_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ig4iic_pci_probe),
	DEVMETHOD(device_attach, ig4iic_pci_attach),
	DEVMETHOD(device_detach, ig4iic_pci_detach),
	DEVMETHOD(device_suspend, ig4iic_pci_suspend),
	DEVMETHOD(device_resume, ig4iic_pci_resume),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr, bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr, bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource, bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource, bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource, bus_generic_adjust_resource),

	/* iicbus interface */
	DEVMETHOD(iicbus_transfer, ig4iic_transfer),
	DEVMETHOD(iicbus_reset, ig4iic_reset),
	DEVMETHOD(iicbus_callback, ig4iic_callback),

	DEVMETHOD_END
};

static driver_t ig4iic_pci_driver = {
	"ig4iic",
	ig4iic_pci_methods,
	sizeof(struct ig4iic_softc)
};

DRIVER_MODULE_ORDERED(ig4iic, pci, ig4iic_pci_driver, 0, 0, SI_ORDER_ANY);
MODULE_DEPEND(ig4iic, pci, 1, 1, 1);
MODULE_PNP_INFO("W32:vendor/device", pci, ig4iic, ig4iic_pci_devices,
    nitems(ig4iic_pci_devices));
