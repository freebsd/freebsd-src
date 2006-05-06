/*-
 * Copyright (c) 2005 Poul-Henning Kamp
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
 * This file defines the ABI between the userland gpib library and the
 * kernel.  This file should not be used anywhere else.
 *
 * $FreeBSD: src/sys/dev/ieee488/ibfoo_int.h,v 1.2 2005/02/12 21:07:09 phk Exp $
 */

#include <sys/ioccom.h>

typedef void ibsrq_t(void);
enum ibfoo_id {
	__ID_INVALID = 0,
	__ID_IBASK,
	__ID_IBBNA,
	__ID_IBCAC,
	__ID_IBCLR,
	__ID_IBCMD,
	__ID_IBCMDA,
	__ID_IBCONFIG,
	__ID_IBDEV,
	__ID_IBDIAG,
	__ID_IBDMA,
	__ID_IBEOS,
	__ID_IBEOT,
	__ID_IBEVENT,
	__ID_IBFIND,
	__ID_IBGTS,
	__ID_IBIST,
	__ID_IBLINES,
	__ID_IBLLO,
	__ID_IBLN,
	__ID_IBLOC,
	__ID_IBONL,
	__ID_IBPAD,
	__ID_IBPCT,
	__ID_IBPOKE,
	__ID_IBPPC,
	__ID_IBRD,
	__ID_IBRDA,
	__ID_IBRDF,
	__ID_IBRDKEY,
	__ID_IBRPP,
	__ID_IBRSC,
	__ID_IBRSP,
	__ID_IBRSV,
	__ID_IBSAD,
	__ID_IBSGNL,
	__ID_IBSIC,
	__ID_IBSRE,
	__ID_IBSRQ,
	__ID_IBSTOP,
	__ID_IBTMO,
	__ID_IBTRAP,
	__ID_IBTRG,
	__ID_IBWAIT,
	__ID_IBWRT,
	__ID_IBWRTA,
	__ID_IBWRTF,
	__ID_IBWRTKEY,
	__ID_IBXTRC
};

#define __F_HANDLE              (1 << 0)
#define __F_SPR                 (1 << 1)
#define __F_BUFFER              (1 << 2)
#define __F_RETVAL              (1 << 3)
#define __F_BDNAME              (1 << 4)
#define __F_MASK                (1 << 5)
#define __F_PADVAL              (1 << 6)
#define __F_SADVAL              (1 << 7)
#define __F_CNT                 (1 << 8)
#define __F_TMO                 (1 << 9)
#define __F_EOS                 (1 << 10)
#define __F_PPR                 (1 << 11)
#define __F_EOT                 (1 << 12)
#define __F_V                   (1 << 13)
#define __F_VALUE               (1 << 14)
#define __F_SAD                 (1 << 15)
#define __F_BOARDID             (1 << 16)
#define __F_OPTION              (1 << 17)
#define __F_FLNAME              (1 << 18)
#define __F_FUNC                (1 << 19)
#define __F_LINES               (1 << 20)
#define __F_PAD                 (1 << 21)
#define __F_MODE                (1 << 22)
#define __F_LISTENFLAG          (1 << 23)
#define __F_EVENT               (1 << 24)

struct ibarg {
	enum ibfoo_id       __ident;
	unsigned int        __field;
	int		    __retval;
	int		    __ibsta;
	int		    __iberr;
	int		    __ibcnt;
	int                 handle;
	char *              spr;
	void *              buffer;
	int *               retval;
	char *              bdname;
	int                 mask;
	int                 padval;
	int                 sadval;
	long                cnt;
	int                 tmo;
	int                 eos;
	char *              ppr;
	int                 eot;
	int                 v;
	int                 value;
	int                 sad;
	int                 boardID;
	int                 option;
	char *              flname;
	ibsrq_t *           func;
	short *             lines;
	int                 pad;
	int                 mode;
	short *             listenflag;
	short *             event;
};

#define GPIB_IBFOO	_IOWR(4, 0, struct ibarg)
