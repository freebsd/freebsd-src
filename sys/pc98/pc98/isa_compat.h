/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "vt.h"
#include "wdc.h"
#include "ar.h"
#include "cx.h"
#include "el.h"
#include "le.h"
#include "lnc.h"
#include "rdp.h"
#include "sr.h"
#include "wl.h"
#include "pcm.h"
#include "pas.h"
#include "sb.h"
#include "sbxvi.h"
#include "sbmidi.h"
#include "awe.h"
#include "gus.h"
#include "mss.h"
#include "css.h"
#include "sscape.h"
#include "trix.h"
#include "opl.h"
#include "mpu.h"
#include "uart.h"
#include "nss.h"
#include "mcd.h"
#include "scd.h"
#include "matcd.h"
#include "wt.h"
#include "ctx.h"
#include "spigot.h"
#include "gp.h"
#include "gsc.h"
#include "cy.h"
#include "dgb.h"
#include "dgm.h"
#include "labpc.h"
#include "rc.h"
#include "rp.h"
#include "tw.h"
#include "asc.h"
#include "stl.h"
#include "stli.h"
#include "loran.h"
#include "fla.h"
#ifdef PC98
#include "bs.h"
#endif

struct old_isa_driver {
	int			type;
	struct isa_driver	*driver;
};

extern struct isa_driver  vtdriver;
extern struct isa_driver wdcdriver;
extern struct isa_driver  ardriver;
extern struct isa_driver  cxdriver;
extern struct isa_driver  eldriver;
extern struct isa_driver  ledriver;
extern struct isa_driver lncdriver;
extern struct isa_driver rdpdriver;
extern struct isa_driver  srdriver;
extern struct isa_driver  wldriver;
extern struct isa_driver pasdriver;
extern struct isa_driver  sbdriver;
extern struct isa_driver sbxvidriver;
extern struct isa_driver sbmididriver;
extern struct isa_driver awedriver;
extern struct isa_driver gusdriver;
extern struct isa_driver mssdriver;
extern struct isa_driver cssdriver;
extern struct isa_driver sscapedriver;
extern struct isa_driver trixdriver;
extern struct isa_driver sscape_mssdriver;
extern struct isa_driver opldriver;
extern struct isa_driver mpudriver;
extern struct isa_driver uartdriver;
extern struct isa_driver nssdriver;
extern struct isa_driver mcddriver;
extern struct isa_driver scddriver;
extern struct isa_driver matcddriver;
extern struct isa_driver  wtdriver;
extern struct isa_driver ctxdriver;
extern struct isa_driver spigotdriver;
extern struct isa_driver  gpdriver;
extern struct isa_driver gscdriver;
extern struct isa_driver  cydriver;
extern struct isa_driver dgbdriver;
extern struct isa_driver dgmdriver;
extern struct isa_driver labpcdriver;
extern struct isa_driver  rcdriver;
extern struct isa_driver  rpdriver;
extern struct isa_driver  twdriver;
extern struct isa_driver ascdriver;
extern struct isa_driver stldriver;
extern struct isa_driver stlidriver;
extern struct isa_driver lorandriver;
#ifdef PC98
extern struct isa_driver bsdriver;
#endif


static struct old_isa_driver old_drivers[] = {

/* Sensitive TTY */

/* Sensitive BIO */

/* Sensitive NET */
#if NRDP > 0
	{ INTR_TYPE_NET, &rdpdriver },
#endif

/* Sensitive CAM */

/* TTY */

#if NVT > 0
	{ INTR_TYPE_TTY, &vtdriver },
#endif
#if NGP > 0
	{ INTR_TYPE_TTY, &gpdriver },
#endif
#if NGSC > 0
	{ INTR_TYPE_TTY, &gscdriver },
#endif
#if NCY > 0
	{ INTR_TYPE_TTY | INTR_TYPE_FAST, &cydriver },
#endif
#if NDGB > 0
	{ INTR_TYPE_TTY, &dgbdriver },
#endif
#if NDGM > 0
	{ INTR_TYPE_TTY, &dgmdriver },
#endif
#if NLABPC > 0
	{ INTR_TYPE_TTY, &labpcdriver },
#endif
#if NRC > 0
	{ INTR_TYPE_TTY, &rcdriver },
#endif
#if NRP > 0
	{ INTR_TYPE_TTY, &rpdriver },
#endif
#if NTW > 0
	{ INTR_TYPE_TTY, &twdriver },
#endif
#if NASC > 0
	{ INTR_TYPE_TTY, &ascdriver },
#endif
#if NSTL > 0
	{ INTR_TYPE_TTY, &stldriver },
#endif
#if NSTLI > 0
	{ INTR_TYPE_TTY, &stlidriver },
#endif
#if NLORAN > 0
	{ INTR_TYPE_TTY | INTR_TYPE_FAST, &lorandriver },
#endif

/* BIO */

#if NWDC > 0
	{ INTR_TYPE_BIO, &wdcdriver },
#endif
#if NMCD > 0
	{ INTR_TYPE_BIO, &mcddriver },
#endif
#if NSCD > 0
	{ INTR_TYPE_BIO, &scddriver },
#endif
#if NMATCD > 0
	{ INTR_TYPE_BIO, &matcddriver },
#endif
#if NWT > 0
	{ INTR_TYPE_BIO, &wtdriver },
#endif

/* NET */

#if NLE > 0
	{ INTR_TYPE_NET, &ledriver },
#endif
#if NLNC > 0
	{ INTR_TYPE_NET, &lncdriver },
#endif
#if NAR > 0
	{ INTR_TYPE_NET, &ardriver },
#endif
#if NCX > 0
	{ INTR_TYPE_NET, &cxdriver },
#endif
#if NEL > 0
	{ INTR_TYPE_NET, &eldriver },
#endif
#if NSR > 0
	{ INTR_TYPE_NET, &srdriver },
#endif
#if NWL > 0
	{ INTR_TYPE_NET, &wldriver },
#endif

/* CAM */

#ifdef PC98
#if NBS > 0
	{ INTR_TYPE_CAM, &bsdriver },
#endif
#endif

/* MISC */

#if NPAS > 0
	{ INTR_TYPE_MISC, &pasdriver },
#endif
#if NSB > 0
	{ INTR_TYPE_MISC, &sbdriver },
#endif
#if NSBXVI > 0
	{ INTR_TYPE_MISC, &sbxvidriver },
#endif
#if NSBMIDI > 0
	{ INTR_TYPE_MISC, &sbmididriver },
#endif
#if NAWE > 0
	{ INTR_TYPE_MISC, &awedriver },
#endif
#if NGUS > 0
	{ INTR_TYPE_MISC, &gusdriver },
#endif
#if NMSS > 0
	{ INTR_TYPE_MISC, &mssdriver },
#endif
#if NCSS > 0
	{ INTR_TYPE_MISC, &cssdriver },
#endif
#if NSSCAPE > 0
	{ INTR_TYPE_MISC, &sscapedriver },
#endif
#if NTRIX > 0
	{ INTR_TYPE_MISC, &trixdriver },
#endif
#if NSSCAPE > 0
	{ INTR_TYPE_MISC, &sscape_mssdriver },
#endif
#if NOPL > 0
	{ INTR_TYPE_MISC, &opldriver },
#endif
#if NMPU > 0
	{ INTR_TYPE_MISC, &mpudriver },
#endif
#if NUART > 0
	{ INTR_TYPE_MISC, &uartdriver },
#endif
#if NNSS > 0
	{ INTR_TYPE_MISC, &nssdriver },
#endif
#if NCTX > 0
	{ INTR_TYPE_MISC, &ctxdriver },
#endif
#if NSPIGOT > 0
	{ INTR_TYPE_MISC, &spigotdriver },
#endif

};

#define old_drivers_count (sizeof(old_drivers) / sizeof(old_drivers[0]))
