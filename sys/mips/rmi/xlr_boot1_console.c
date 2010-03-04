/*-
 * Copyright (c) 2006 Wojciech A. Koszek <wkoszek@FreeBSD.org>
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
 * $Id: xlr_boot1_console.c,v 1.6 2008-07-16 20:22:49 jayachandranc Exp $
 */
/*
 *  Adapted for XLR bootloader
 *  RMi
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_comconsole.h"

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/tty.h>

#include <mips/rmi/xlrconfig.h>
#include <mips/rmi/shared_structs.h>
#include <mips/rmi/shared_structs_func.h>

#include <ddb/ddb.h>

#if 0
static cn_probe_t xlr_boot1_cnprobe;
static cn_init_t xlr_boot1_cninit;
static cn_term_t xlr_boot1_cnterm;
static cn_getc_t xlr_boot1_cngetc;
static cn_checkc_t xlr_boot1_cncheckc;
static cn_putc_t xlr_boot1_cnputc;

CONS_DRIVER(xlrboot, xlr_boot1_cnprobe, xlr_boot1_cninit, xlr_boot1_cnterm, xlr_boot1_cngetc,
    xlr_boot1_cncheckc, xlr_boot1_cnputc, NULL);

/*
 * Device gets probed. Firmwire should be checked here probably.
 */
static void
xlr_boot1_cnprobe(struct consdev *cp)
{
	cp->cn_pri = CN_NORMAL;
	cp->cn_tp = NULL;
	cp->cn_arg = NULL;	/* softc */
	cp->cn_unit = -1;	/* ? */
	cp->cn_flags = 0;
}

/*
 * Initialization.
 */
static void
xlr_boot1_cninit(struct consdev *cp)
{
	sprintf(cp->cn_name, "boot1");
}

static void
xlr_boot1_cnterm(struct consdev *cp)
{
	cp->cn_pri = CN_DEAD;
	cp->cn_flags = 0;
	return;
}

static int
xlr_boot1_cngetc(struct consdev *cp)
{
	return boot1_info_uart_getchar_func(&xlr_boot1_info);
}

static void
xlr_boot1_cnputc(struct consdev *cp, int c)
{
	boot1_info_uart_putchar_func(&xlr_boot1_info, c);
}

static int
xlr_boot1_cncheckc(struct consdev *cp)
{
	return 0;
}

#endif
