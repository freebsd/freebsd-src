/* $FreeBSD$ */
/*
 * ISP Firmware Helper Pseudo Device for FreeBSD
 *
 *---------------------------------------
 * Copyright (c) 2000, by Matthew Jacob
 * All rights reserved.
 *---------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/linker.h>

#include <dev/ispfw/asm_1040.h>
#include <dev/ispfw/asm_1080.h>
#include <dev/ispfw/asm_12160.h>
#include <dev/ispfw/asm_2100.h>
#include <dev/ispfw/asm_2200.h>

#define	ISPFW_VERSION	0

#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080
#define	PCI_PRODUCT_QLOGIC_ISP12160	0x1216
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#define	PCI_PRODUCT_QLOGIC_ISP2200	0x2200

typedef void ispfwfunc __P((int, int, int, const u_int16_t **));
extern ispfwfunc *isp_get_firmware_p;
static void isp_get_firmware __P((int, int, int, const u_int16_t **));

static int ncallers = 0;
static const u_int16_t ***callp = NULL;
static int addcaller(const u_int16_t **);

static int
addcaller(const u_int16_t **caller)
{
	const u_int16_t ***newcallp;
	int i;
	for (i = 0; i < ncallers; i++) {
		if (callp[i] == caller)
			return (1);
	}
	newcallp = malloc((ncallers + 1) * sizeof (const u_int16_t ***),
	    M_DEVBUF, M_NOWAIT);
	if (newcallp == NULL) {
		return (0);
	}
	for (i = 0; i < ncallers; i++) {
		newcallp[i] = callp[i];
	}
	newcallp[ncallers] = caller;
	if (ncallers++)
		free(callp, M_DEVBUF);
	callp = newcallp;
	return (1);
}

static void
isp_get_firmware(int version, int tgtmode, int devid, const u_int16_t **ptrp)
{
	const u_int16_t *rp = NULL;

	if (version == ISPFW_VERSION) {
		switch (devid) {
		case PCI_PRODUCT_QLOGIC_ISP1020:
			if (tgtmode)
				rp = isp_1040_risc_code_it;
			else
				rp = isp_1040_risc_code;
			break;
		case PCI_PRODUCT_QLOGIC_ISP1080:
		case PCI_PRODUCT_QLOGIC_ISP1240:
		case PCI_PRODUCT_QLOGIC_ISP1280:
			if (tgtmode)
				rp = isp_1080_risc_code_it;
			else
				rp = isp_1080_risc_code;
			break;
		case PCI_PRODUCT_QLOGIC_ISP12160:
			rp = isp_12160_risc_code;
			break;
		case PCI_PRODUCT_QLOGIC_ISP2100:
			rp = isp_2100_risc_code;
			break;
		case PCI_PRODUCT_QLOGIC_ISP2200:
			rp = isp_2200_risc_code;
		default:
			break;
		}
	}
	if (rp && addcaller(ptrp)) {
		*ptrp = rp;
	}
}

static int
isp_module_handler(module_t mod, int what, void *arg)
{
	switch (what) {
	case MOD_LOAD:
		isp_get_firmware_p = isp_get_firmware;
		break;
	case MOD_UNLOAD:
		isp_get_firmware_p = NULL;
		if (ncallers)  {
			int i;
			for (i = 0; i < ncallers; i++) {
				*callp[i] = NULL;
			}
			free(callp, M_DEVBUF);
		}
		break;
	default:
		break;
	}
	return (0);
}
static moduledata_t ispfw_mod = {
	"ispfw", isp_module_handler, NULL
};
DECLARE_MODULE(ispfw, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
