/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/sbuf.h>

#include <machine/gdb_machdep.h>
#include <machine/kdb.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

SYSCTL_NODE(_debug, OID_AUTO, gdb, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "GDB settings");

static dbbe_init_f gdb_init;
static dbbe_trap_f gdb_trap;

KDB_BACKEND(gdb, gdb_init, NULL, NULL, gdb_trap);

static struct gdb_dbgport null_gdb_dbgport;
DATA_SET(gdb_dbgport_set, null_gdb_dbgport);
SET_DECLARE(gdb_dbgport_set, struct gdb_dbgport);

struct gdb_dbgport *gdb_cur = NULL;
int gdb_listening = 0;
bool gdb_ackmode = true;

static unsigned char gdb_bindata[64];

#ifdef DDB
bool gdb_return_to_ddb = false;
#endif

static int
gdb_init(void)
{
	struct gdb_dbgport *dp, **iter;
	int cur_pri, pri;

	gdb_cur = NULL;
	cur_pri = -1;
	SET_FOREACH(iter, gdb_dbgport_set) {
		dp = *iter;
		pri = (dp->gdb_probe != NULL) ? dp->gdb_probe() : -1;
		dp->gdb_active = (pri >= 0) ? 0 : -1;
		if (pri > cur_pri) {
			cur_pri = pri;
			gdb_cur = dp;
		}
	}
	if (gdb_cur != NULL) {
		printf("GDB: debug ports:");
		SET_FOREACH(iter, gdb_dbgport_set) {
			dp = *iter;
			if (dp->gdb_active == 0)
				printf(" %s", dp->gdb_name);
		}
		printf("\n");
	} else
		printf("GDB: no debug ports present\n");
	if (gdb_cur != NULL) {
		gdb_cur->gdb_init();
		printf("GDB: current port: %s\n", gdb_cur->gdb_name);
	}
	if (gdb_cur != NULL) {
		cur_pri = (boothowto & RB_GDB) ? 2 : 0;
		gdb_consinit();
	} else
		cur_pri = -1;
	return (cur_pri);
}

static void
gdb_do_mem_search(void)
{
	size_t patlen;
	intmax_t addr, size;
	const unsigned char *found;

	if (gdb_rx_varhex(&addr) || gdb_rx_char() != ';' ||
	    gdb_rx_varhex(&size) || gdb_rx_char() != ';' ||
	    gdb_rx_bindata(gdb_bindata, sizeof(gdb_bindata), &patlen)) {
		gdb_tx_err(EINVAL);
		return;
	}
	if (gdb_search_mem((char *)(uintptr_t)addr, size, gdb_bindata,
	    patlen, &found)) {
		if (found == 0ULL)
			gdb_tx_begin('0');
		else {
			gdb_tx_begin('1');
			gdb_tx_char(',');
			gdb_tx_hex((intmax_t)(uintptr_t)found, 8);
		}
		gdb_tx_end();
	} else
		gdb_tx_err(EIO);
}

static void
gdb_do_threadinfo(struct thread **thr_iter)
{
	static struct thread * const done_sentinel = (void *)(uintptr_t)1;
	static const size_t tidsz_hex = sizeof(lwpid_t) * 2;
	size_t tds_sent;

	if (*thr_iter == NULL) {
		gdb_tx_err(ENXIO);
		return;
	}

	if (*thr_iter == done_sentinel) {
		gdb_tx_begin('l');
		*thr_iter = NULL;
		goto sendit;
	}

	gdb_tx_begin('m');

	for (tds_sent = 0;
	    *thr_iter != NULL && gdb_txbuf_has_capacity(tidsz_hex + 1);
	    *thr_iter = kdb_thr_next(*thr_iter), tds_sent++) {
		if (tds_sent > 0)
			gdb_tx_char(',');
		gdb_tx_varhex((*thr_iter)->td_tid);
	}

	/*
	 * Can't send EOF and "some" in same packet, so set a sentinel to send
	 * EOF when GDB asks us next.
	 */
	if (*thr_iter == NULL && tds_sent > 0)
		*thr_iter = done_sentinel;

sendit:
	gdb_tx_end();
}

#define	BIT(n)	(1ull << (n))
enum {
	GDB_MULTIPROCESS,
	GDB_SWBREAK,
	GDB_HWBREAK,
	GDB_QRELOCINSN,
	GDB_FORK_EVENTS,
	GDB_VFORK_EVENTS,
	GDB_EXEC_EVENTS,
	GDB_VCONT_SUPPORTED,
	GDB_QTHREADEVENTS,
	GDB_NO_RESUMED,
};
static const char * const gdb_feature_names[] = {
	[GDB_MULTIPROCESS] = "multiprocess",
	[GDB_SWBREAK] = "swbreak",
	[GDB_HWBREAK] = "hwbreak",
	[GDB_QRELOCINSN] = "qRelocInsn",
	[GDB_FORK_EVENTS] = "fork-events",
	[GDB_VFORK_EVENTS] = "vfork-events",
	[GDB_EXEC_EVENTS] = "exec-events",
	[GDB_VCONT_SUPPORTED] = "vContSupported",
	[GDB_QTHREADEVENTS] = "QThreadEvents",
	[GDB_NO_RESUMED] = "no-resumed",
};
static void
gdb_do_qsupported(uint32_t *feat)
{
	char *tok, *delim, ok;
	size_t i, toklen;

	/* Parse supported host features */
	*feat = 0;
	switch (gdb_rx_char()) {
	case ':':
		break;
	case EOF:
		goto nofeatures;
	default:
		goto error;
	}

	while (gdb_rxsz > 0) {
		tok = gdb_rxp;
		delim = strchrnul(gdb_rxp, ';');
		toklen = (delim - tok);

		gdb_rxp += toklen;
		gdb_rxsz -= toklen;
		if (*delim != '\0') {
			*delim = '\0';
			gdb_rxp += 1;
			gdb_rxsz -= 1;
		}

		if (toklen < 2)
			goto error;

		ok = tok[toklen - 1];
		if (ok != '-' && ok != '+') {
			/*
			 * GDB only has one KV-pair feature, and we don't
			 * support it, so ignore and move on.
			 */
			if (strchr(tok, '=') != NULL)
				continue;
			/* Not a KV-pair, and not a +/- flag?  Malformed. */
			goto error;
		}
		if (ok != '+')
			continue;
		tok[toklen - 1] = '\0';

		for (i = 0; i < nitems(gdb_feature_names); i++)
			if (strcmp(gdb_feature_names[i], tok) == 0)
				break;

		if (i == nitems(gdb_feature_names)) {
			/* Unknown GDB feature. */
			continue;
		}

		*feat |= BIT(i);
	}

nofeatures:
	/* Send a supported feature list back */
	gdb_tx_begin(0);

	gdb_tx_str("PacketSize");
	gdb_tx_char('=');
	/*
	 * We don't buffer framing bytes, but we do need to retain a byte for a
	 * trailing nul.
	 */
	gdb_tx_varhex(GDB_BUFSZ + strlen("$#nn") - 1);

	gdb_tx_str(";qXfer:threads:read+");

	/*
	 * If the debugport is a reliable transport, request No Ack mode from
	 * the server.  The server may or may not choose to enter No Ack mode.
	 * https://sourceware.org/gdb/onlinedocs/gdb/Packet-Acknowledgment.html
	 */
	if (gdb_cur->gdb_dbfeatures & GDB_DBGP_FEAT_RELIABLE)
		gdb_tx_str(";QStartNoAckMode+");

	/*
	 * Future consideration:
	 *   - vCont
	 *   - multiprocess
	 */
	gdb_tx_end();
	return;

error:
	*feat = 0;
	gdb_tx_err(EINVAL);
}

/*
 * A qXfer_context provides a vaguely generic way to generate a multi-packet
 * response on the fly, making some assumptions about the size of sbuf writes
 * vs actual packet length constraints.  A non-byzantine gdb host should allow
 * hundreds of bytes per packet or more.
 *
 * Upper layers are considered responsible for escaping the four forbidden
 * characters '# $ } *'.
 */
struct qXfer_context {
	struct sbuf sb;
	size_t last_offset;
	bool flushed;
	bool lastmessage;
	char xfer_buf[GDB_BUFSZ];
};

static int
qXfer_drain(void *v, const char *buf, int len)
{
	struct qXfer_context *qx;

	if (len < 0)
		return (-EINVAL);

	qx = v;
	if (qx->flushed) {
		/*
		 * Overflow.  We lost some message.  Maybe the packet size is
		 * ridiculously small.
		 */
		printf("%s: Overflow in qXfer detected.\n", __func__);
		return (-ENOBUFS);
	}

	qx->last_offset += len;
	qx->flushed = true;

	if (qx->lastmessage)
		gdb_tx_begin('l');
	else
		gdb_tx_begin('m');

	memcpy(gdb_txp, buf, len);
	gdb_txp += len;

	gdb_tx_end();
	return (len);
}

static int
init_qXfer_ctx(struct qXfer_context *qx, uintmax_t len)
{

	/* Protocol (max) length field includes framing overhead. */
	if (len < sizeof("$m#nn"))
		return (ENOSPC);

	len -= 4;
	len = ummin(len, GDB_BUFSZ - 1);

	qx->last_offset = 0;
	qx->flushed = false;
	qx->lastmessage = false;
	sbuf_new(&qx->sb, qx->xfer_buf, len, SBUF_FIXEDLEN);
	sbuf_set_drain(&qx->sb, qXfer_drain, qx);
	return (0);
}

/*
 * Squashes special XML and GDB characters down to _.  Sorry.
 */
static void
qXfer_escape_xmlattr_str(char *dst, size_t dstlen, const char *src)
{
	static const char *forbidden = "#$}*";

	size_t i;
	char c;

	for (i = 0; i < dstlen - 1 && *src != 0; src++, i++) {
		c = *src;
		/* XML attr filter */
		if (c < 32)
			c = '_';
		/* We assume attributes will be "" quoted. */
		if (c == '<' || c == '&' || c == '"')
			c = '_';

		/* GDB escape. */
		if (strchr(forbidden, c) != NULL) {
			/*
			 * It would be nice to escape these properly, but to do
			 * it correctly we need to escape them in the transmit
			 * layer, potentially doubling our buffer requirements.
			 * For now, avoid breaking the protocol by squashing
			 * them to underscore.
			 */
#if 0
			*dst++ = '}';
			c ^= 0x20;
#endif
			c = '_';
		}
		*dst++ = c;
	}
	if (*src != 0)
		printf("XXX%s: overflow; API misuse\n", __func__);

	*dst = 0;
}

/*
 * Dynamically generate qXfer:threads document, one packet at a time.
 *
 * The format is loosely described[0], although it does not seem that the
 * <?xml?> mentioned on that page is required.
 *
 * [0]: https://sourceware.org/gdb/current/onlinedocs/gdb/Thread-List-Format.html
 */
static void
do_qXfer_threads_read(void)
{
	/* Kludgy context */
	static struct {
		struct qXfer_context qXfer;
		/* Kludgy state machine */
		struct thread *iter;
		enum {
			XML_START_THREAD,	/* '<thread' */
			XML_THREAD_ID,		/* ' id="xxx"' */
			XML_THREAD_CORE,	/* ' core="yyy"' */
			XML_THREAD_NAME,	/* ' name="zzz"' */
			XML_THREAD_EXTRA,	/* '> ...' */
			XML_END_THREAD,		/* '</thread>' */
			XML_SENT_END_THREADS,	/* '</threads>' */
		} next_step;
	} ctx;
	static char td_name_escape[MAXCOMLEN * 2 + 1];

	const char *name_src;
	uintmax_t offset, len;
	int error;

	/* Annex part must be empty. */
	if (gdb_rx_char() != ':')
		goto misformed_request;

	if (gdb_rx_varhex(&offset) != 0 ||
	    gdb_rx_char() != ',' ||
	    gdb_rx_varhex(&len) != 0)
		goto misformed_request;

	/*
	 * Validate resume xfers.
	 */
	if (offset != 0) {
		if (offset != ctx.qXfer.last_offset) {
			printf("%s: Resumed offset %ju != expected %zu\n",
			    __func__, offset, ctx.qXfer.last_offset);
			error = ESPIPE;
			goto request_error;
		}
		ctx.qXfer.flushed = false;
	}

	if (offset == 0) {
		ctx.iter = kdb_thr_first();
		ctx.next_step = XML_START_THREAD;
		error = init_qXfer_ctx(&ctx.qXfer, len);
		if (error != 0)
			goto request_error;

		sbuf_cat(&ctx.qXfer.sb, "<threads>");
	}

	while (!ctx.qXfer.flushed && ctx.iter != NULL) {
		switch (ctx.next_step) {
		case XML_START_THREAD:
			ctx.next_step = XML_THREAD_ID;
			sbuf_cat(&ctx.qXfer.sb, "<thread");
			continue;

		case XML_THREAD_ID:
			ctx.next_step = XML_THREAD_CORE;
			sbuf_printf(&ctx.qXfer.sb, " id=\"%jx\"",
			    (uintmax_t)ctx.iter->td_tid);
			continue;

		case XML_THREAD_CORE:
			ctx.next_step = XML_THREAD_NAME;
			if (ctx.iter->td_oncpu != NOCPU) {
				sbuf_printf(&ctx.qXfer.sb, " core=\"%d\"",
				    ctx.iter->td_oncpu);
			}
			continue;

		case XML_THREAD_NAME:
			ctx.next_step = XML_THREAD_EXTRA;

			if (ctx.iter->td_name[0] != 0)
				name_src = ctx.iter->td_name;
			else if (ctx.iter->td_proc != NULL &&
			    ctx.iter->td_proc->p_comm[0] != 0)
				name_src = ctx.iter->td_proc->p_comm;
			else
				continue;

			qXfer_escape_xmlattr_str(td_name_escape,
			    sizeof(td_name_escape), name_src);
			sbuf_printf(&ctx.qXfer.sb, " name=\"%s\"",
			    td_name_escape);
			continue;

		case XML_THREAD_EXTRA:
			ctx.next_step = XML_END_THREAD;

			sbuf_putc(&ctx.qXfer.sb, '>');

			if (ctx.iter->td_state == TDS_RUNNING)
				sbuf_cat(&ctx.qXfer.sb, "Running");
			else if (ctx.iter->td_state == TDS_RUNQ)
				sbuf_cat(&ctx.qXfer.sb, "RunQ");
			else if (ctx.iter->td_state == TDS_CAN_RUN)
				sbuf_cat(&ctx.qXfer.sb, "CanRun");
			else if (TD_ON_LOCK(ctx.iter))
				sbuf_cat(&ctx.qXfer.sb, "Blocked");
			else if (TD_IS_SLEEPING(ctx.iter))
				sbuf_cat(&ctx.qXfer.sb, "Sleeping");
			else if (TD_IS_SWAPPED(ctx.iter))
				sbuf_cat(&ctx.qXfer.sb, "Swapped");
			else if (TD_AWAITING_INTR(ctx.iter))
				sbuf_cat(&ctx.qXfer.sb, "IthreadWait");
			else if (TD_IS_SUSPENDED(ctx.iter))
				sbuf_cat(&ctx.qXfer.sb, "Suspended");
			else
				sbuf_cat(&ctx.qXfer.sb, "???");
			continue;

		case XML_END_THREAD:
			ctx.next_step = XML_START_THREAD;
			sbuf_cat(&ctx.qXfer.sb, "</thread>");
			ctx.iter = kdb_thr_next(ctx.iter);
			continue;

		/*
		 * This one isn't part of the looping state machine,
		 * but GCC complains if you leave an enum value out of the
		 * select.
		 */
		case XML_SENT_END_THREADS:
			/* NOTREACHED */
			break;
		}
	}
	if (ctx.qXfer.flushed)
		return;

	if (ctx.next_step != XML_SENT_END_THREADS) {
		ctx.next_step = XML_SENT_END_THREADS;
		sbuf_cat(&ctx.qXfer.sb, "</threads>");
	}
	if (ctx.qXfer.flushed)
		return;

	ctx.qXfer.lastmessage = true;
	sbuf_finish(&ctx.qXfer.sb);
	sbuf_delete(&ctx.qXfer.sb);
	ctx.qXfer.last_offset = 0;
	return;

misformed_request:
	/*
	 * GDB "General-Query-Packets.html" qXfer-read anchor specifically
	 * documents an E00 code for malformed requests or invalid annex.
	 * Non-zero codes indicate invalid offset or "error reading the data."
	 */
	error = 0;
request_error:
	gdb_tx_err(error);
	return;
}

/*
 * A set of standardized transfers from "special data areas."
 *
 * We've already matched on "qXfer:" and advanced the rx packet buffer past
 * that bit.  Parse out the rest of the packet and generate an appropriate
 * response.
 */
static void
do_qXfer(void)
{
	if (!gdb_rx_equal("threads:"))
		goto unrecognized;

	if (!gdb_rx_equal("read:"))
		goto unrecognized;

	do_qXfer_threads_read();
	return;

unrecognized:
	gdb_tx_empty();
	return;
}

static void
gdb_handle_detach(void)
{
	kdb_cpu_clear_singlestep();
	gdb_listening = 0;

	if (gdb_cur->gdb_dbfeatures & GDB_DBGP_FEAT_WANTTERM)
		gdb_cur->gdb_term();

#ifdef DDB
	if (!gdb_return_to_ddb)
		return;

	gdb_return_to_ddb = false;

	if (kdb_dbbe_select("ddb") != 0)
		printf("The ddb backend could not be selected.\n");
#endif
}

static int
gdb_trap(int type, int code)
{
	jmp_buf jb;
	struct thread *thr_iter;
	void *prev_jb;
	uint32_t host_features;

	prev_jb = kdb_jmpbuf(jb);
	if (setjmp(jb) != 0) {
		printf("%s bailing, hopefully back to ddb!\n", __func__);
		gdb_listening = 0;
		(void)kdb_jmpbuf(prev_jb);
		return (1);
	}

	gdb_listening = 0;
	gdb_ackmode = true;

	/*
	 * Send a T packet. We currently do not support watchpoints (the
	 * awatch, rwatch or watch elements).
	 */
	gdb_tx_begin('T');
	gdb_tx_hex(gdb_cpu_signal(type, code), 2);
	gdb_tx_varhex(GDB_REG_PC);
	gdb_tx_char(':');
	gdb_tx_reg(GDB_REG_PC);
	gdb_tx_char(';');
	gdb_tx_str("thread:");
	gdb_tx_varhex((long)kdb_thread->td_tid);
	gdb_tx_char(';');
	gdb_tx_end();			/* XXX check error condition. */

	thr_iter = NULL;
	while (gdb_rx_begin() == 0) {
		/* printf("GDB: got '%s'\n", gdb_rxp); */
		switch (gdb_rx_char()) {
		case '?':	/* Last signal. */
			gdb_tx_begin('T');
			gdb_tx_hex(gdb_cpu_signal(type, code), 2);
			gdb_tx_str("thread:");
			gdb_tx_varhex((long)kdb_thread->td_tid);
			gdb_tx_char(';');
			gdb_tx_end();
			break;
		case 'c': {	/* Continue. */
			uintmax_t addr;
			register_t pc;
			if (!gdb_rx_varhex(&addr)) {
				pc = addr;
				gdb_cpu_setreg(GDB_REG_PC, &pc);
			}
			kdb_cpu_clear_singlestep();
			gdb_listening = 1;
			return (1);
		}
		case 'C': {	/* Continue with signal. */
			uintmax_t addr, sig;
			register_t pc;
			if (!gdb_rx_varhex(&sig) && gdb_rx_char() == ';' &&
			    !gdb_rx_varhex(&addr)) {
				pc = addr;
				gdb_cpu_setreg(GDB_REG_PC, &pc);
			}
			kdb_cpu_clear_singlestep();
			gdb_listening = 1;
			return (1);
		}
		case 'D': {     /* Detach */
			gdb_tx_ok();
			gdb_handle_detach();
			return (1);
		}
		case 'g': {	/* Read registers. */
			size_t r;
			gdb_tx_begin(0);
			for (r = 0; r < GDB_NREGS; r++)
				gdb_tx_reg(r);
			gdb_tx_end();
			break;
		}
		case 'G':	/* Write registers. */
			gdb_tx_err(0);
			break;
		case 'H': {	/* Set thread. */
			intmax_t tid;
			struct thread *thr;

			/* Ignore 'g' (general) or 'c' (continue) flag. */
			(void) gdb_rx_char();

			if (gdb_rx_varhex(&tid)) {
				gdb_tx_err(EINVAL);
				break;
			}
			if (tid > 0) {
				thr = kdb_thr_lookup(tid);
				if (thr == NULL) {
					gdb_tx_err(ENOENT);
					break;
				}
				kdb_thr_select(thr);
			}
			gdb_tx_ok();
			break;
		}
		case 'k':	/* Kill request. */
			gdb_handle_detach();
			return (1);
		case 'm': {	/* Read memory. */
			uintmax_t addr, size;
			if (gdb_rx_varhex(&addr) || gdb_rx_char() != ',' ||
			    gdb_rx_varhex(&size)) {
				gdb_tx_err(EINVAL);
				break;
			}
			gdb_tx_begin(0);
			if (gdb_tx_mem((char *)(uintptr_t)addr, size))
				gdb_tx_end();
			else
				gdb_tx_err(EIO);
			break;
		}
		case 'M': {	/* Write memory. */
			uintmax_t addr, size;
			if (gdb_rx_varhex(&addr) || gdb_rx_char() != ',' ||
			    gdb_rx_varhex(&size) || gdb_rx_char() != ':') {
				gdb_tx_err(EINVAL);
				break;
			}
			if (gdb_rx_mem((char *)(uintptr_t)addr, size) == 0)
				gdb_tx_err(EIO);
			else
				gdb_tx_ok();
			break;
		}
		case 'P': {	/* Write register. */
			char *val;
			uintmax_t reg;
			val = gdb_rxp;
			if (gdb_rx_varhex(&reg) || gdb_rx_char() != '=' ||
			    !gdb_rx_mem(val, gdb_cpu_regsz(reg))) {
				gdb_tx_err(EINVAL);
				break;
			}
			gdb_cpu_setreg(reg, val);
			gdb_tx_ok();
			break;
		}
		case 'q':	/* General query. */
			if (gdb_rx_equal("C")) {
				gdb_tx_begin('Q');
				gdb_tx_char('C');
				gdb_tx_varhex((long)kdb_thread->td_tid);
				gdb_tx_end();
			} else if (gdb_rx_equal("Supported")) {
				gdb_do_qsupported(&host_features);
			} else if (gdb_rx_equal("fThreadInfo")) {
				thr_iter = kdb_thr_first();
				gdb_do_threadinfo(&thr_iter);
			} else if (gdb_rx_equal("sThreadInfo")) {
				gdb_do_threadinfo(&thr_iter);
			} else if (gdb_rx_equal("Xfer:")) {
				do_qXfer();
			} else if (gdb_rx_equal("Search:memory:")) {
				gdb_do_mem_search();
#ifdef __powerpc__
			} else if (gdb_rx_equal("Offsets")) {
				gdb_cpu_do_offsets();
#endif
			} else if (!gdb_cpu_query())
				gdb_tx_empty();
			break;
		case 'Q':
			if (gdb_rx_equal("StartNoAckMode")) {
				if ((gdb_cur->gdb_dbfeatures &
				    GDB_DBGP_FEAT_RELIABLE) == 0) {
					/*
					 * Shouldn't happen if we didn't
					 * advertise support.  Reject.
					 */
					gdb_tx_empty();
					break;
				}
				gdb_ackmode = false;
				gdb_tx_ok();
			} else
				gdb_tx_empty();
			break;
		case 's': {	/* Step. */
			uintmax_t addr;
			register_t pc;
			if (!gdb_rx_varhex(&addr)) {
				pc = addr;
				gdb_cpu_setreg(GDB_REG_PC, &pc);
			}
			kdb_cpu_set_singlestep();
			gdb_listening = 1;
			return (1);
		}
		case 'S': {	/* Step with signal. */
			uintmax_t addr, sig;
			register_t pc;
			if (!gdb_rx_varhex(&sig) && gdb_rx_char() == ';' &&
			    !gdb_rx_varhex(&addr)) {
				pc = addr;
				gdb_cpu_setreg(GDB_REG_PC, &pc);
			}
			kdb_cpu_set_singlestep();
			gdb_listening = 1;
			return (1);
		}
		case 'T': {	/* Thread alive. */
			intmax_t tid;
			if (gdb_rx_varhex(&tid)) {
				gdb_tx_err(EINVAL);
				break;
			}
			if (kdb_thr_lookup(tid) != NULL)
				gdb_tx_ok();
			else
				gdb_tx_err(ENOENT);
			break;
		}
		case EOF:
			/* Empty command. Treat as unknown command. */
			/* FALLTHROUGH */
		default:
			/* Unknown command. Send empty response. */
			gdb_tx_empty();
			break;
		}
	}
	(void)kdb_jmpbuf(prev_jb);
	return (0);
}
