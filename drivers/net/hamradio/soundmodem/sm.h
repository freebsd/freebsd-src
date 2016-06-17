/*****************************************************************************/

/*
 *	sm.h  --  soundcard radio modem driver internal header.
 *
 *	Copyright (C) 1996-1999  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

#ifndef _SM_H
#define _SM_H

/* ---------------------------------------------------------------------- */

#include <linux/hdlcdrv.h>
#include <linux/soundmodem.h>
#include <asm/processor.h>
#include <linux/bitops.h>
#include <linux/parport.h>

#define SM_DEBUG

/* ---------------------------------------------------------------------- */
/*
 * Information that need to be kept for each board.
 */

struct sm_state {
	struct hdlcdrv_state hdrv;

	const struct modem_tx_info *mode_tx;
	const struct modem_rx_info *mode_rx;

	const struct hardware_info *hwdrv;

	struct pardevice *pardev;

	/*
	 * Hardware (soundcard) access routines state
	 */
	struct {
		void *ibuf;
		unsigned int ifragsz;
		unsigned int ifragptr;
		unsigned int i16bit;
		void *obuf;
		unsigned int ofragsz;
		unsigned int ofragptr;
		unsigned int o16bit;
		int ptt_cnt;
	} dma;

	union {
		long hw[32/sizeof(long)];
	} hw;

	/*
	 * state of the modem code
	 */
	union {
		long m[48/sizeof(long)];
	} m;
	union {
		long d[256/sizeof(long)];
	} d;

#define DIAGDATALEN 64
	struct diag_data {
		unsigned int mode;
		unsigned int flags;
		volatile int ptr;
		short data[DIAGDATALEN];
	} diag;


#ifdef SM_DEBUG
	struct debug_vals {
		unsigned long last_jiffies;
		unsigned cur_intcnt;
		unsigned last_intcnt;
		unsigned mod_cyc;
		unsigned demod_cyc;
		unsigned dma_residue;
	} debug_vals;
#endif /* SM_DEBUG */
};

/* ---------------------------------------------------------------------- */
/*
 * Mode definition structure
 */

struct modem_tx_info {
	const char *name;
	unsigned int loc_storage;
	int srate;
	int bitrate;
        void (*modulator_u8)(struct sm_state *, unsigned char *, unsigned int);
        void (*modulator_s16)(struct sm_state *, short *, unsigned int);
        void (*init)(struct sm_state *);
};

struct modem_rx_info {
	const char *name;
	unsigned int loc_storage;
	int srate;
	int bitrate;
	unsigned int overlap;
	unsigned int sperbit;
        void (*demodulator_u8)(struct sm_state *, const unsigned char *, unsigned int);
        void (*demodulator_s16)(struct sm_state *, const short *, unsigned int);
        void (*init)(struct sm_state *);
};

/* ---------------------------------------------------------------------- */
/*
 * Soundcard driver definition structure
 */

struct hardware_info {
	char *hw_name; /* used for request_{region,irq,dma} */
	unsigned int loc_storage;
	/*
	 * mode specific open/close
	 */
	int (*open)(struct net_device *, struct sm_state *);
	int (*close)(struct net_device *, struct sm_state *);
	int (*ioctl)(struct net_device *, struct sm_state *, struct ifreq *,
		     struct hdlcdrv_ioctl *, int);
	int (*sethw)(struct net_device *, struct sm_state *, char *);
};

/* --------------------------------------------------------------------- */

extern const char sm_drvname[];
extern const char sm_drvinfo[];

/* --------------------------------------------------------------------- */
/*
 * ===================== diagnostics stuff ===============================
 */

static inline void diag_trigger(struct sm_state *sm)
{
	if (sm->diag.ptr < 0)
		if (!(sm->diag.flags & SM_DIAGFLAG_DCDGATE) || sm->hdrv.hdlcrx.dcd)
			sm->diag.ptr = 0;
}

/* --------------------------------------------------------------------- */

#define SHRT_MAX ((short)(((unsigned short)(~0U))>>1))
#define SHRT_MIN (-SHRT_MAX-1)

static inline void diag_add(struct sm_state *sm, int valinp, int valdemod)
{
	int val;

	if ((sm->diag.mode != SM_DIAGMODE_INPUT &&
	     sm->diag.mode != SM_DIAGMODE_DEMOD) ||
	    sm->diag.ptr >= DIAGDATALEN || sm->diag.ptr < 0)
		return;
	val = (sm->diag.mode == SM_DIAGMODE_DEMOD) ? valdemod : valinp;
	/* clip */
	if (val > SHRT_MAX)
		val = SHRT_MAX;
	if (val < SHRT_MIN)
		val = SHRT_MIN;
	sm->diag.data[sm->diag.ptr++] = val;
}

/* --------------------------------------------------------------------- */

static inline void diag_add_one(struct sm_state *sm, int val)
{
	if ((sm->diag.mode != SM_DIAGMODE_INPUT &&
	     sm->diag.mode != SM_DIAGMODE_DEMOD) ||
	    sm->diag.ptr >= DIAGDATALEN || sm->diag.ptr < 0)
		return;
	/* clip */
	if (val > SHRT_MAX)
		val = SHRT_MAX;
	if (val < SHRT_MIN)
		val = SHRT_MIN;
	sm->diag.data[sm->diag.ptr++] = val;
}

/* --------------------------------------------------------------------- */

static inline void diag_add_constellation(struct sm_state *sm, int vali, int valq)
{
	if ((sm->diag.mode != SM_DIAGMODE_CONSTELLATION) ||
	    sm->diag.ptr >= DIAGDATALEN-1 || sm->diag.ptr < 0)
		return;
	/* clip */
	if (vali > SHRT_MAX)
		vali = SHRT_MAX;
	if (vali < SHRT_MIN)
		vali = SHRT_MIN;
	if (valq > SHRT_MAX)
		valq = SHRT_MAX;
	if (valq < SHRT_MIN)
		valq = SHRT_MIN;
	sm->diag.data[sm->diag.ptr++] = vali;
	sm->diag.data[sm->diag.ptr++] = valq;
}

/* --------------------------------------------------------------------- */
/*
 * ===================== utility functions ===============================
 */

#if 0
static inline unsigned int hweight32(unsigned int w)
	__attribute__ ((unused));
static inline unsigned int hweight16(unsigned short w)
	__attribute__ ((unused));
static inline unsigned int hweight8(unsigned char w)
        __attribute__ ((unused));

static inline unsigned int hweight32(unsigned int w)
{
        unsigned int res = (w & 0x55555555) + ((w >> 1) & 0x55555555);
        res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
        res = (res & 0x0F0F0F0F) + ((res >> 4) & 0x0F0F0F0F);
        res = (res & 0x00FF00FF) + ((res >> 8) & 0x00FF00FF);
        return (res & 0x0000FFFF) + ((res >> 16) & 0x0000FFFF);
}

static inline unsigned int hweight16(unsigned short w)
{
        unsigned short res = (w & 0x5555) + ((w >> 1) & 0x5555);
        res = (res & 0x3333) + ((res >> 2) & 0x3333);
        res = (res & 0x0F0F) + ((res >> 4) & 0x0F0F);
        return (res & 0x00FF) + ((res >> 8) & 0x00FF);
}

static inline unsigned int hweight8(unsigned char w)
{
        unsigned short res = (w & 0x55) + ((w >> 1) & 0x55);
        res = (res & 0x33) + ((res >> 2) & 0x33);
        return (res & 0x0F) + ((res >> 4) & 0x0F);
}

#endif

static inline unsigned int gcd(unsigned int x, unsigned int y)
	__attribute__ ((unused));
static inline unsigned int lcm(unsigned int x, unsigned int y)
	__attribute__ ((unused));

static inline unsigned int gcd(unsigned int x, unsigned int y)
{
	for (;;) {
		if (!x)
			return y;
		if (!y)
			return x;
		if (x > y)
			x %= y;
		else
			y %= x;
	}
}

static inline unsigned int lcm(unsigned int x, unsigned int y)
{
	return x * y / gcd(x, y);
}

/* --------------------------------------------------------------------- */
/*
 * ===================== profiling =======================================
 */


#ifdef __i386__

#include <asm/msr.h>

/*
 * only do 32bit cycle counter arithmetic; we hope we won't overflow.
 * in fact, overflowing modems would require over 2THz CPU clock speeds :-)
 */

#define time_exec(var,cmd)                                              \
({                                                                      \
	if (cpu_has_tsc) {                                              \
		unsigned int cnt1, cnt2;                                \
		rdtscl(cnt1);                                           \
		cmd;                                                    \
		rdtscl(cnt2);                                           \
		var = cnt2-cnt1;                                        \
	} else {                                                        \
		cmd;                                                    \
	}                                                               \
})

#else /* __i386__ */

#define time_exec(var,cmd) cmd

#endif /* __i386__ */

/* --------------------------------------------------------------------- */

extern const struct modem_tx_info sm_afsk1200_tx;
extern const struct modem_tx_info sm_afsk2400_7_tx;
extern const struct modem_tx_info sm_afsk2400_8_tx;
extern const struct modem_tx_info sm_afsk2666_tx;
extern const struct modem_tx_info sm_psk4800_tx;
extern const struct modem_tx_info sm_hapn4800_8_tx;
extern const struct modem_tx_info sm_hapn4800_10_tx;
extern const struct modem_tx_info sm_hapn4800_pm8_tx;
extern const struct modem_tx_info sm_hapn4800_pm10_tx;
extern const struct modem_tx_info sm_fsk9600_4_tx;
extern const struct modem_tx_info sm_fsk9600_5_tx;

extern const struct modem_rx_info sm_afsk1200_rx;
extern const struct modem_rx_info sm_afsk2400_7_rx;
extern const struct modem_rx_info sm_afsk2400_8_rx;
extern const struct modem_rx_info sm_afsk2666_rx;
extern const struct modem_rx_info sm_psk4800_rx;
extern const struct modem_rx_info sm_hapn4800_8_rx;
extern const struct modem_rx_info sm_hapn4800_10_rx;
extern const struct modem_rx_info sm_hapn4800_pm8_rx;
extern const struct modem_rx_info sm_hapn4800_pm10_rx;
extern const struct modem_rx_info sm_fsk9600_4_rx;
extern const struct modem_rx_info sm_fsk9600_5_rx;

extern const struct hardware_info sm_hw_sbc;
extern const struct hardware_info sm_hw_sbcfdx;
extern const struct hardware_info sm_hw_wss;
extern const struct hardware_info sm_hw_wssfdx;

extern const struct modem_tx_info *sm_modem_tx_table[];
extern const struct modem_rx_info *sm_modem_rx_table[];
extern const struct hardware_info *sm_hardware_table[];

/* --------------------------------------------------------------------- */

void sm_output_status(struct sm_state *sm);
/*void sm_output_open(struct sm_state *sm);*/
/*void sm_output_close(struct sm_state *sm);*/

/* --------------------------------------------------------------------- */

extern void inline sm_int_freq(struct sm_state *sm)
{
#ifdef SM_DEBUG
	unsigned long cur_jiffies = jiffies;
	/*
	 * measure the interrupt frequency
	 */
	sm->debug_vals.cur_intcnt++;
	if ((cur_jiffies - sm->debug_vals.last_jiffies) >= HZ) {
		sm->debug_vals.last_jiffies = cur_jiffies;
		sm->debug_vals.last_intcnt = sm->debug_vals.cur_intcnt;
		sm->debug_vals.cur_intcnt = 0;
	}
#endif /* SM_DEBUG */
}

/* --------------------------------------------------------------------- */
#endif /* _SM_H */
