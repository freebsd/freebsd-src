/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
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

#define	W(p)				(*(u_int16_t *)(p))
#define	vW(p)				(*(u_int16_t volatile *)(p))
#define	D(p)				(*(u_int32_t *)(p))
#define	vD(p)				(*(u_int32_t volatile *)(p))

#define	CE_OVERRUN			0
#define	CE_INTERRUPT_BUF_OVERFLOW	1
#define	CE_TTY_BUF_OVERFLOW		2
#define	CE_NTYPES			3
#define	CE_RECORD(com, errnum)		(++(com)->delta_error_counts[errnum])

#define	DEBUG

#ifdef DEBUG
extern unsigned digi_debug;
#define	DLOG(level, args)	if (digi_debug & (level)) device_printf args
#else
#define	DLOG(level, args)
#endif


struct digi_softc;

/* digiboard port structure */
struct digi_p {
	struct digi_softc *sc;

	int status;
#define ENABLED 1
#define DIGI_DTR_OFF 2
#define PAUSE_TX 8
#define PAUSE_RX 16

	int opencnt;
	ushort txbufsize;
	ushort rxbufsize;
	volatile struct board_chan *bc;
	struct tty *tp;

	dev_t dev[6];

	u_char *txbuf;
	u_char *rxbuf;
	u_char txwin;
	u_char rxwin;

	u_char pnum;		/* port number */

	u_char modemfake;	/* Modem values to be forced */
	u_char mstat;
	u_char modem;		/* Force values */

	int active_out;		/* nonzero if the callout device is open */
	int dtr_wait;		/* time to hold DTR down on close (* 1/hz) */
	u_int wopeners;		/* # processes waiting for DCD in open() */

	/*
	 * The high level of the driver never reads status registers directly
	 * because there would be too many side effects to handle conveniently.
	 * Instead, it reads copies of the registers stored here by the
	 * interrupt handler.
	 */
	u_char last_modem_status;	/* last MSR read by intr handler */
	u_char prev_modem_status;	/* last MSR handled by high level */


	/* Initial state. */
	struct termios it_in;		/* should be in struct tty */
	struct termios it_out;

	/* Lock state. */
	struct termios lt_in;		/* should be in struct tty */
	struct termios lt_out;

	u_int do_timestamp;
	u_int do_dcd_timestamp;
	struct timeval dcd_timestamp;

	u_long bytes_in, bytes_out;
	u_int delta_error_counts[CE_NTYPES];
	u_long error_counts;

	tcflag_t c_iflag;		/* hold true IXON/IXOFF/IXANY */
	int lcc, lostcc, lbuf;
	u_char send_ring;
};

/*
 * Map TIOCM_* values to digiboard values
 */
struct digi_control_signals {
	int rts;
	int cd;
	int dsr;
	int cts;
	int ri;
	int dtr;
};

enum digi_board_status {
	DIGI_STATUS_NOTINIT,
	DIGI_STATUS_ENABLED,
	DIGI_STATUS_DISABLED
};

/* Digiboard per-board structure */
struct digi_softc {
	/* struct board_info */
	device_t dev;

	const char *name;
	enum digi_board_status status;
	ushort numports;		/* number of ports on card */
	ushort port;			/* I/O port */
	ushort wport;			/* window select I/O port */

	struct {
		struct resource *mem;
		int mrid;
		struct resource *irq;
		int irqrid;
		struct resource *io;
		int iorid;
		void *irqHandler;
		int unit;
		dev_t ctldev;
	} res;

	u_char *vmem;			/* virtual memory address */
	u_char *memcmd;
	volatile u_char *memevent;
	long pmem;			/* physical memory address */

	struct {
		u_char *data;
		size_t size;
	} bios, fep, link;

#ifdef DIGI_INTERRUPT
	struct timeval intr_timestamp;
#endif

	struct digi_p *ports;	/* pointer to array of port descriptors */
	struct tty *ttys;	/* pointer to array of TTY structures */
	volatile struct global_data *gdata;
	u_char window;		/* saved window */
	int win_size;
	int win_bits;
	int mem_size;
	int mem_seg;
	digiModel_t model;
	const struct digi_control_signals *csigs;
	int opencnt;
	unsigned pcibus : 1;		/* On a PCI bus ? */

	struct callout_handle callout;	/* poll timeout handle */
	struct callout_handle inttest;	/* int test timeout handle */
	const char *module;
	
	u_char *(*setwin)(struct digi_softc *_sc, unsigned _addr);
	void	(*hidewin)(struct digi_softc *_sc);
	void	(*towin)(struct digi_softc *_sc, int _win);
#ifdef DEBUG
	int	intr_count;
#endif
};

extern devclass_t digi_devclass;
extern const struct digi_control_signals digi_xixe_signals;
extern const struct digi_control_signals digi_normal_signals;

const char	*digi_errortxt(int _id);
int		 digi_modhandler(module_t _mod, int _event, void *_arg);
int		 digi_attach(struct digi_softc *);
int		 digi_detach(device_t _dev);
int		 digi_shutdown(device_t _dev);
