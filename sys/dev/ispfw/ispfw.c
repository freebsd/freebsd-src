/*-
 * ISP Firmware Helper Pseudo Device for FreeBSD
 *
 * Copyright (c) 2000, 2001, by Matthew Jacob
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <dev/ispfw/asm_1040.h>
#include <dev/ispfw/asm_1080.h>
#include <dev/ispfw/asm_12160.h>
#include <dev/ispfw/asm_2100.h>
#include <dev/ispfw/asm_2200.h>
#include <dev/ispfw/asm_2300.h>
#ifdef __sparc64__
#include <dev/ispfw/asm_1000.h>
#endif

#define	ISPFW_VERSION	0

#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080
#define	PCI_PRODUCT_QLOGIC_ISP10160	0x1016
#define	PCI_PRODUCT_QLOGIC_ISP12160	0x1216
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#define	PCI_PRODUCT_QLOGIC_ISP2200	0x2200
#define	PCI_PRODUCT_QLOGIC_ISP2300	0x2300
#define	PCI_PRODUCT_QLOGIC_ISP2312	0x2312
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312
#ifdef __sparc64__
#define	SBUS_PRODUCT_QLOGIC_ISP1000	0x1000
#endif

typedef void ispfwfunc(int, int, int, const u_int16_t **);
extern ispfwfunc *isp_get_firmware_p;
static void isp_get_firmware(int, int, int, const u_int16_t **);

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
		case PCI_PRODUCT_QLOGIC_ISP10160:
		case PCI_PRODUCT_QLOGIC_ISP12160:
			if (tgtmode)
				rp = isp_12160_risc_code_it;
			else
				rp = isp_12160_risc_code;
			break;
		case PCI_PRODUCT_QLOGIC_ISP2100:
			rp = isp_2100_risc_code;
			break;
		case PCI_PRODUCT_QLOGIC_ISP2200:
			rp = isp_2200_risc_code;
			break;
		case PCI_PRODUCT_QLOGIC_ISP2300:
		case PCI_PRODUCT_QLOGIC_ISP2312:
		case PCI_PRODUCT_QLOGIC_ISP6312:
			rp = isp_2300_risc_code;
			break;
#ifdef __sparc64__
		case SBUS_PRODUCT_QLOGIC_ISP1000:
			if (tgtmode)
				break;
			rp = isp_1000_risc_code;
			break;
#endif
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
		return (EOPNOTSUPP);
		break;
	}
	return (0);
}
static moduledata_t ispfw_mod = {
	"ispfw", isp_module_handler, NULL
};
DECLARE_MODULE(ispfw, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
MODULE_VERSION(ispfw, ISPFW_VERSION);
MODULE_DEPEND(ispfw, isp, 1, 1, 1);
