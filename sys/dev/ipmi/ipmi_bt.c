/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Yandex LLC
 * Copyright (c) 2023 Andrey V. Elsukov <ae@FreeBSD.org>
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/selinfo.h>
#include <machine/bus.h>

#include <sys/ipmi.h>
#include <dev/ipmi/ipmivars.h>

/*
 * BT interface
 */

#define	DMSG0(sc, fmt, ...)	do {				\
	device_printf((sc)->ipmi_dev, "BT: %s: " fmt "\n",	\
	    __func__, ## __VA_ARGS__);				\
} while (0)

#define	DMSGV(...)		if (bootverbose) {		\
	DMSG0(__VA_ARGS__);					\
}

#ifdef IPMI_BT_DEBUG
#define	DMSG(...)		DMSG0(__VA_ARGS__)
#else
#define	DMSG(...)
#endif

#define	BT_IO_BASE		0xe4

#define	BT_CTRL_REG		0
#define	  BT_C_CLR_WR_PTR	(1L << 0)
#define	  BT_C_CLR_RD_PTR	(1L << 1)
#define	  BT_C_H2B_ATN		(1L << 2)
#define	  BT_C_B2H_ATN		(1L << 3)
#define	  BT_C_SMS_ATN		(1L << 4)
#define	  BT_C_OEM0		(1L << 5)
#define	  BT_C_H_BUSY		(1L << 6)
#define	  BT_C_B_BUSY		(1L << 7)

#define	BT_CTRL_BITS		"\20\01CLR_WR_PTR\02CLR_RD_PTR\03H2B_ATN\04B2H_ATN"\
				"\05SMS_ATN\06OEM0\07H_BUSY\010B_BUSY"

#define	BT_DATA_REG		1
#define	 BTMSG_REQLEN		3
#define	 BTMSG_REPLEN		4

#define	BT_INTMASK_REG		2
#define	 BT_IM_B2H_IRQ_EN	(1L << 0)
#define	 BT_IM_B2H_IRQ		(1L << 1)
#define	 BT_IM_BMC_HWRST	(1L << 7)

static int bt_polled_request(struct ipmi_softc *, struct ipmi_request *);
static int bt_driver_request(struct ipmi_softc *, struct ipmi_request *);
static int bt_wait(struct ipmi_softc *, uint8_t, uint8_t);
static int bt_reset(struct ipmi_softc *);

static void bt_loop(void *);
static int bt_startup(struct ipmi_softc *);

#define	BT_DELAY_MIN	1
#define	BT_DELAY_MAX	256

static int
bt_wait(struct ipmi_softc *sc, uint8_t mask, uint8_t wanted)
{
	volatile uint8_t value;
	int delay = BT_DELAY_MIN;
	int count = 20000; /* about 5 seconds */

	while (count--) {
		value = INB(sc, BT_CTRL_REG);
		if ((value & mask) == wanted)
			return (value);
		/*
		 * The wait delay is increased exponentially to avoid putting
		 * significant load on I/O bus.
		 */
		DELAY(delay);
		if (delay < BT_DELAY_MAX)
			delay <<= 1;
	}
	DMSGV(sc, "failed: m=%b w=%b v=0x%02x\n",
	    mask, BT_CTRL_BITS, wanted, BT_CTRL_BITS, value);
	return (-1);

}

static int
bt_reset(struct ipmi_softc *sc)
{
	uint8_t v;

	v = INB(sc, BT_CTRL_REG);
	DMSG(sc, "ctrl: %b", v, BT_CTRL_BITS);
	v &= BT_C_H_BUSY; /* clear H_BUSY iff it set */
	v |= BT_C_CLR_WR_PTR | BT_C_CLR_RD_PTR | BT_C_B2H_ATN | BT_C_H2B_ATN;

	bt_wait(sc, BT_C_B_BUSY, 0);
	OUTB(sc, BT_CTRL_REG, v);

	v = BT_IM_B2H_IRQ | BT_IM_BMC_HWRST;
	OUTB(sc, BT_INTMASK_REG, v);

	return (0);
}

/*
 * Send a request message and collect the reply. Returns 1 if we
 * succeed.
 */
static int
bt_polled_request(struct ipmi_softc *sc, struct ipmi_request *req)
{
	uint8_t addr, cmd, seq, v;
	int i;

	IPMI_IO_LOCK(sc);

	/*
	 * Send the request:
	 *
	 * Byte 1 | Byte 2    | Byte 3 | Byte 4 | Byte 5:N
	 * -------+-----------+--------+--------+---------
	 * Length | NetFn/LUN | Seq    | Cmd    | Data
	 */

	if (bt_wait(sc, BT_C_B_BUSY | BT_C_H2B_ATN, 0) < 0) {
		DMSG(sc, "failed to start write transfer");
		goto fail;
	}
	DMSG(sc, "request: length=%d, addr=0x%02x, seq=%u, cmd=0x%02x",
	    (int)req->ir_requestlen, req->ir_addr, sc->ipmi_bt_seq, req->ir_command);
	OUTB(sc, BT_CTRL_REG, BT_C_CLR_WR_PTR);
	OUTB(sc, BT_DATA_REG, req->ir_requestlen + BTMSG_REQLEN);
	OUTB(sc, BT_DATA_REG, req->ir_addr);
	OUTB(sc, BT_DATA_REG, sc->ipmi_bt_seq);
	OUTB(sc, BT_DATA_REG, req->ir_command);
	for (i = 0; i < req->ir_requestlen; i++)
		OUTB(sc, BT_DATA_REG, req->ir_request[i]);
	OUTB(sc, BT_CTRL_REG, BT_C_H2B_ATN);

	if (bt_wait(sc, BT_C_B_BUSY | BT_C_H2B_ATN, 0) < 0) {
		DMSG(sc, "failed to finish write transfer");
		goto fail;
	}

	/*
	 * Read the reply:
	 *
	 * Byte 1 | Byte 2    | Byte 3 | Byte 4 | Byte 5          | Byte 6:N
	 * -------+-----------+--------+--------+-----------------+---------
	 * Length | NetFn/LUN | Seq    | Cmd    | Completion code | Data
	 */
	if (bt_wait(sc, BT_C_B2H_ATN, BT_C_B2H_ATN) < 0) {
		DMSG(sc, "got no reply from BMC");
		goto fail;
	}
	OUTB(sc, BT_CTRL_REG, BT_C_H_BUSY);
	OUTB(sc, BT_CTRL_REG, BT_C_B2H_ATN);
	OUTB(sc, BT_CTRL_REG, BT_C_CLR_RD_PTR);

	i = INB(sc, BT_DATA_REG);
	if (i < BTMSG_REPLEN) {
		DMSG(sc, "wrong data length: %d", i);
		goto fail;
	}
	req->ir_replylen = i - BTMSG_REPLEN;
	DMSG(sc, "data length: %d, frame length: %d", req->ir_replylen, i);

	addr = INB(sc, BT_DATA_REG);
	if (addr != IPMI_REPLY_ADDR(req->ir_addr)) {
		DMSGV(sc, "address doesn't match: addr=0x%02x vs. 0x%02x",
		    req->ir_addr, addr);
	}

	seq = INB(sc, BT_DATA_REG);
	if (seq != sc->ipmi_bt_seq) {
		DMSGV(sc, "seq number doesn't match: seq=0x%02x vs. 0x%02x",
		    sc->ipmi_bt_seq, seq);
	}

	cmd = INB(sc, BT_DATA_REG);
	if (cmd != req->ir_command) {
		DMSGV(sc, "command doesn't match: cmd=0x%02x vs. 0x%02x",
		    req->ir_command, cmd);
	}

	req->ir_compcode = INB(sc, BT_DATA_REG);
	for (i = 0; i < req->ir_replylen; i++) {
		v = INB(sc, BT_DATA_REG);
		if (i < req->ir_replybuflen)
			req->ir_reply[i] = v;
	}

	OUTB(sc, BT_CTRL_REG, BT_C_H_BUSY);
	IPMI_IO_UNLOCK(sc);
	DMSG(sc, "reply: length=%d, addr=0x%02x, seq=%u, cmd=0x%02x, code=0x%02x",
	    (int)req->ir_replylen, addr, seq, req->ir_command, req->ir_compcode);
	return (1);
fail:
	bt_reset(sc);
	IPMI_IO_UNLOCK(sc);
	return (0);
}

static void
bt_loop(void *arg)
{
	struct ipmi_softc *sc = arg;
	struct ipmi_request *req;

	IPMI_LOCK(sc);
	while ((req = ipmi_dequeue_request(sc)) != NULL) {
		IPMI_UNLOCK(sc);
		(void)bt_driver_request(sc, req);
		IPMI_LOCK(sc);
		sc->ipmi_bt_seq++;
		ipmi_complete_request(sc, req);
	}
	IPMI_UNLOCK(sc);
	kproc_exit(0);
}

static int
bt_startup(struct ipmi_softc *sc)
{

	return (kproc_create(bt_loop, sc, &sc->ipmi_kthread, 0, 0, "%s: bt",
	    device_get_nameunit(sc->ipmi_dev)));
}

static int
bt_driver_request(struct ipmi_softc *sc, struct ipmi_request *req)
{
	int i, ok;

	ok = 0;
	for (i = 0; i < 3 && !ok; i++)
		ok = bt_polled_request(sc, req);
	if (ok)
		req->ir_error = 0;
	else
		req->ir_error = EIO;
	return (req->ir_error);
}

int
ipmi_bt_attach(struct ipmi_softc *sc)
{
	/* Setup function pointers. */
	sc->ipmi_startup = bt_startup;
	sc->ipmi_enqueue_request = ipmi_polled_enqueue_request;
	sc->ipmi_driver_request = bt_driver_request;
	sc->ipmi_driver_requests_polled = 1;
	sc->ipmi_bt_seq = 1;

	return (bt_reset(sc));
}
