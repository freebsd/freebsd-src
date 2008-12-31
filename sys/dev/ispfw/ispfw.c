/*-
 * ISP Firmware Modules for FreeBSD
 *
 * Copyright (c) 2000, 2001, 2006 by Matthew Jacob
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
__FBSDID("$FreeBSD: src/sys/dev/ispfw/ispfw.c,v 1.19.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker.h>
#include <sys/firmware.h>

#if	defined(ISP_ALL) || !defined(KLD_MODULE) 
#define	ISP_1040	1
#define	ISP_1040_IT	1
#define	ISP_1080	1
#define	ISP_1080_IT	1
#define	ISP_12160	1
#define	ISP_12160_IT	1
#define	ISP_2100	1
#define	ISP_2200	1
#define	ISP_2300	1
#define	ISP_2322	1
#define	ISP_2400	1
#ifdef __sparc64__
#define	ISP_1000	1
#endif
#define	MODULE_NAME	"ispfw"
#endif

#if	defined(ISP_1040) || defined(ISP_1040_IT)
#include <dev/ispfw/asm_1040.h>
#endif
#if	defined(ISP_1080) || defined(ISP_1080_IT)
#include <dev/ispfw/asm_1080.h>
#endif
#if	defined(ISP_12160) || defined(ISP_12160_IT)
#include <dev/ispfw/asm_12160.h>
#endif
#if	defined(ISP_2100)
#include <dev/ispfw/asm_2100.h>
#endif
#if	defined(ISP_2200)
#include <dev/ispfw/asm_2200.h>
#endif
#if	defined(ISP_2300)
#include <dev/ispfw/asm_2300.h>
#endif
#if	defined(ISP_2322)
#include <dev/ispfw/asm_2322.h>
#endif
#if	defined(ISP_2400)
#include <dev/ispfw/asm_2400.h>
#endif
#if	defined(ISP_1000)
#include <dev/ispfw/asm_1000.h>
#endif

#define	ISPFW_VERSION	1
#define	RMACRO(token)							\
	if (firmware_register(#token, token##_risc_code,		\
	    token##_risc_code [3] * sizeof token##_risc_code [3],	\
	    ISPFW_VERSION, NULL) == NULL) {				\
		printf("unable to register firmware '%s'\n", #token);	\
	} else {							\
		printf("registered firmware set <%s>\n", #token);	\
	}

#define	UMACRO(token)							\
	firmware_unregister(#token);					\
	printf("unregistered firmware set <%s>\n", #token);

static int
do_load_fw(void)
{
#if	defined(ISP_1000)
	RMACRO(isp_1000);
#endif
#if	defined(ISP_1040)
	RMACRO(isp_1040);
#endif
#if	defined(ISP_1040_IT)
	RMACRO(isp_1040_it);
#endif
#if	defined(ISP_1080)
	RMACRO(isp_1080);
#endif
#if	defined(ISP_1080_IT)
	RMACRO(isp_1080_it);
#endif
#if	defined(ISP_12160)
	RMACRO(isp_12160);
#endif
#if	defined(ISP_12160_IT)
	RMACRO(isp_12160_it);
#endif
#if	defined(ISP_2100)
	RMACRO(isp_2100);
#endif
#if	defined(ISP_2200)
	RMACRO(isp_2200);
#endif
#if	defined(ISP_2300)
	RMACRO(isp_2300);
#endif
#if	defined(ISP_2322)
	RMACRO(isp_2322);
#endif
#if	defined(ISP_2400)
	RMACRO(isp_2400);
#endif
	return (0);
}

static int
do_unload_fw(void)
{
#if	defined(ISP_1000)
	UMACRO(isp_1000);
#elif	defined(ISP_1040)
	UMACRO(isp_1040);
#elif	defined(ISP_1040_IT)
	UMACRO(isp_1040_it);
#elif	defined(ISP_1080)
	UMACRO(isp_1080);
#elif	defined(ISP_1080_IT)
	UMACRO(isp_1080_it);
#elif	defined(ISP_12160)
	UMACRO(isp_12160);
#elif	defined(ISP_12160_IT)
	UMACRO(isp_12160_it);
#elif	defined(ISP_2100)
	UMACRO(isp_2100);
#elif	defined(ISP_2200)
	UMACRO(isp_2200);
#elif	defined(ISP_2300)
	UMACRO(isp_2300);
#elif	defined(ISP_2322)
	UMACRO(isp_2322);
#elif	defined(ISP_2400)
	UMACRO(isp_2400);
#endif
	return (0);
}

static int
module_handler(module_t mod, int what, void *arg)
{
	int r;
	switch (what) {
	case MOD_LOAD:
		r = do_load_fw();
		break;
	case MOD_UNLOAD:
		r = do_unload_fw();
		break;
	default:
		r = EOPNOTSUPP;
		break;
	}
	return (r);
}
static moduledata_t ispfw_mod = {
	MODULE_NAME, module_handler, NULL
};
#ifndef	KLD_MODULE
DECLARE_MODULE(isp, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#else
#if	defined(ISP_1000)
DECLARE_MODULE(isp_1000, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_1040)
DECLARE_MODULE(isp_1040, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_1040_IT)
DECLARE_MODULE(isp_1040_it, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_1080)
DECLARE_MODULE(isp_1080, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_1080_IT)
DECLARE_MODULE(isp_1080_it, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_12160)
DECLARE_MODULE(isp_12160, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_12160_IT)
DECLARE_MODULE(isp_12160_IT, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_2100)
DECLARE_MODULE(isp_2100, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_2200)
DECLARE_MODULE(isp_2200, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_2300)
DECLARE_MODULE(isp_2300, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_2322)
DECLARE_MODULE(isp_2322, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#elif	defined(ISP_2400)
DECLARE_MODULE(isp_2400, ispfw_mod, SI_SUB_DRIVERS, SI_ORDER_THIRD);
#endif
#endif
