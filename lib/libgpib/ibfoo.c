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
 * This file merely redirects to the file in <dev/ieee488/ugpib.h>
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <fcntl.h>

#include <dev/ieee488/ugpib.h>
#include <dev/ieee488/ibfoo_int.h>

int ibcnt, iberr;

static int fd = -1;

static int
__ibsubmit(struct ibfoo_iocarg *ap)
{
	int i;

	if (fd < 0)
		fd = open("/dev/gpib0ib", O_RDWR);
	if (fd < 0)
		err(1, "Could not open /dev/gpib0ib");
	i = ioctl(fd, GPIB_IBFOO, ap);
	if (i)
		err(1, "GPIB_IBFOO(%d, 0x%x) failed", ap->__ident, ap->__field);
	ibcnt = ap->__ibcnt;
	iberr = ap->__iberr;
	return (ap->__retval);
}

int
ibask (int handle, int option, int * retval)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBASK;
	io.handle = handle;
	io.option = option;
	io.retval = retval;
	io.__field = __F_HANDLE | __F_OPTION | __F_RETVAL;
	return (__ibsubmit(&io));
}

int
ibbna (int handle, char * bdname)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBBNA;
	io.handle = handle;
	io.bdname = bdname;
	io.__field = __F_HANDLE | __F_BDNAME;
	return (__ibsubmit(&io));
}

int
ibcac (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBCAC;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibclr (int handle)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBCLR;
	io.handle = handle;
	io.__field = __F_HANDLE;
	return (__ibsubmit(&io));
}

int
ibcmd (int handle, void * buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBCMD;
	io.handle = handle;
	io.buffer = buffer;
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibcmda (int handle, void * buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBCMDA;
	io.handle = handle;
	io.buffer = buffer;
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibconfig (int handle, int option, int value)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBCONFIG;
	io.handle = handle;
	io.option = option;
	io.value = value;
	io.__field = __F_HANDLE | __F_OPTION | __F_VALUE;
	return (__ibsubmit(&io));
}

int
ibdev (int boardID, int pad, int sad, int tmo, int eot, int eos)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBDEV;
	io.boardID = boardID;
	io.pad = pad;
	io.sad = sad;
	io.tmo = tmo;
	io.eot = eot;
	io.eos = eos;
	io.__field = __F_BOARDID | __F_PAD | __F_SAD | __F_TMO | __F_EOT | __F_EOS;
	return (__ibsubmit(&io));
}

int
ibdiag (int handle, void * buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBDIAG;
	io.handle = handle;
	io.buffer = buffer;
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibdma (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBDMA;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibeos (int handle, int eos)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBEOS;
	io.handle = handle;
	io.eos = eos;
	io.__field = __F_HANDLE | __F_EOS;
	return (__ibsubmit(&io));
}

int
ibeot (int handle, int eot)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBEOT;
	io.handle = handle;
	io.eot = eot;
	io.__field = __F_HANDLE | __F_EOT;
	return (__ibsubmit(&io));
}

int
ibevent (int handle, short * event)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBEVENT;
	io.handle = handle;
	io.event = event;
	io.__field = __F_HANDLE | __F_EVENT;
	return (__ibsubmit(&io));
}

int
ibfind (char * bdname)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBFIND;
	io.bdname = bdname;
	io.__field = __F_BDNAME;
	return (__ibsubmit(&io));
}

int
ibgts (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBGTS;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibist (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBIST;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
iblines (int handle, short * lines)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBLINES;
	io.handle = handle;
	io.lines = lines;
	io.__field = __F_HANDLE | __F_LINES;
	return (__ibsubmit(&io));
}

int
ibllo (int handle)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBLLO;
	io.handle = handle;
	io.__field = __F_HANDLE;
	return (__ibsubmit(&io));
}

int
ibln (int handle, int padval, int sadval, short * listenflag)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBLN;
	io.handle = handle;
	io.padval = padval;
	io.sadval = sadval;
	io.listenflag = listenflag;
	io.__field = __F_HANDLE | __F_PADVAL | __F_SADVAL | __F_LISTENFLAG;
	return (__ibsubmit(&io));
}

int
ibloc (int handle)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBLOC;
	io.handle = handle;
	io.__field = __F_HANDLE;
	return (__ibsubmit(&io));
}

int
ibonl (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBONL;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibpad (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBPAD;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibpct (int handle)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBPCT;
	io.handle = handle;
	io.__field = __F_HANDLE;
	return (__ibsubmit(&io));
}

int
ibpoke (int handle, int option, int value)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBPOKE;
	io.handle = handle;
	io.option = option;
	io.value = value;
	io.__field = __F_HANDLE | __F_OPTION | __F_VALUE;
	return (__ibsubmit(&io));
}

int
ibppc (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBPPC;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibrd (int handle, void * buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRD;
	io.handle = handle;
	io.buffer = buffer;
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibrda (int handle, void * buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRDA;
	io.handle = handle;
	io.buffer = buffer;
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibrdf (int handle, char * flname)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRDF;
	io.handle = handle;
	io.flname = flname;
	io.__field = __F_HANDLE | __F_FLNAME;
	return (__ibsubmit(&io));
}

int
ibrdkey (int handle, void * buffer, int cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRDKEY;
	io.handle = handle;
	io.buffer = buffer;
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibrpp (int handle, char * ppr)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRPP;
	io.handle = handle;
	io.ppr = ppr;
	io.__field = __F_HANDLE | __F_PPR;
	return (__ibsubmit(&io));
}

int
ibrsc (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRSC;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibrsp (int handle, char * spr)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRSP;
	io.handle = handle;
	io.spr = spr;
	io.__field = __F_HANDLE | __F_SPR;
	return (__ibsubmit(&io));
}

int
ibrsv (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBRSV;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibsad (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBSAD;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibsgnl (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBSGNL;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibsic (int handle)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBSIC;
	io.handle = handle;
	io.__field = __F_HANDLE;
	return (__ibsubmit(&io));
}

int
ibsre (int handle, int v)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBSRE;
	io.handle = handle;
	io.v = v;
	io.__field = __F_HANDLE | __F_V;
	return (__ibsubmit(&io));
}

int
ibsrq (ibsrq_t * func)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBSRQ;
	io.func = func;
	io.__field = __F_FUNC;
	return (__ibsubmit(&io));
}

int
ibstop (int handle)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBSTOP;
	io.handle = handle;
	io.__field = __F_HANDLE;
	return (__ibsubmit(&io));
}

int
ibtmo (int handle, int tmo)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBTMO;
	io.handle = handle;
	io.tmo = tmo;
	io.__field = __F_HANDLE | __F_TMO;
	return (__ibsubmit(&io));
}

int
ibtrap (int  mask, int mode)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBTRAP;
	io.mask = mask;
	io.mode = mode;
	io.__field = __F_MASK | __F_MODE;
	return (__ibsubmit(&io));
}

int
ibtrg (int handle)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBTRG;
	io.handle = handle;
	io.__field = __F_HANDLE;
	return (__ibsubmit(&io));
}

int
ibwait (int handle, int mask)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBWAIT;
	io.handle = handle;
	io.mask = mask;
	io.__field = __F_HANDLE | __F_MASK;
	return (__ibsubmit(&io));
}

int
ibwrt (int handle, const void *buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBWRT;
	io.handle = handle;
	io.buffer = __DECONST(void *, buffer);
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibwrta (int handle, const void * buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBWRTA;
	io.handle = handle;
	io.buffer = __DECONST(void *, buffer);
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibwrtf (int handle, const char *flname)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBWRTF;
	io.handle = handle;
	io.flname = __DECONST(void *, flname);
	io.__field = __F_HANDLE | __F_FLNAME;
	return (__ibsubmit(&io));
}

int
ibwrtkey (int handle, const void *buffer, int cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBWRTKEY;
	io.handle = handle;
	io.buffer = __DECONST(void *, buffer);
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

int
ibxtrc (int handle, void * buffer, long cnt)
{
	struct ibfoo_iocarg io;

	io.__ident = __ID_IBXTRC;
	io.handle = handle;
	io.buffer = buffer;
	io.cnt = cnt;
	io.__field = __F_HANDLE | __F_BUFFER | __F_CNT;
	return (__ibsubmit(&io));
}

