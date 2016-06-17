/* $Id: amd7930.c,v 1.28 2001/10/13 01:47:29 davem Exp $
 * drivers/sbus/audio/amd7930.c
 *
 * Copyright (C) 1996,1997 Thomas K. Dyas (tdyas@eden.rutgers.edu)
 *
 * This is the lowlevel driver for the AMD7930 audio chip found on all
 * sun4c machines and some sun4m machines.
 *
 * The amd7930 is actually an ISDN chip which has a very simple
 * integrated audio encoder/decoder. When Sun decided on what chip to
 * use for audio, they had the brilliant idea of using the amd7930 and
 * only connecting the audio encoder/decoder pins.
 *
 * Thanks to the AMD engineer who was able to get us the AMD79C30
 * databook which has all the programming information and gain tables.
 *
 * Advanced Micro Devices' Am79C30A is an ISDN/audio chip used in the
 * SparcStation 1+.  The chip provides microphone and speaker interfaces
 * which provide mono-channel audio at 8K samples per second via either
 * 8-bit A-law or 8-bit mu-law encoding.  Also, the chip features an
 * ISDN BRI Line Interface Unit (LIU), I.430 S/T physical interface,
 * which performs basic D channel LAPD processing and provides raw
 * B channel data.  The digital audio channel, the two ISDN B channels,
 * and two 64 Kbps channels to the microprocessor are all interconnected
 * via a multiplexer.
 *
 * This driver interfaces to the Linux HiSax ISDN driver, which performs
 * all high-level Q.921 and Q.931 ISDN functions.  The file is not
 * itself a hardware driver; rather it uses functions exported by
 * the AMD7930 driver in the sparcaudio subsystem (drivers/sbus/audio),
 * allowing the chip to be simultaneously used for both audio and ISDN data.
 * The hardware driver does _no_ buffering, but provides several callbacks
 * which are called during interrupt service and should therefore run quickly.
 *
 * D channel transmission is performed by passing the hardware driver the
 * address and size of an skb's data area, then waiting for a callback
 * to signal successful transmission of the packet.  A task is then
 * queued to notify the HiSax driver that another packet may be transmitted.
 *
 * D channel reception is quite simple, mainly because of:
 *   1) the slow speed of the D channel - 16 kbps, and
 *   2) the presence of an 8- or 32-byte (depending on chip version) FIFO
 *      to buffer the D channel data on the chip
 * Worst case scenario of back-to-back packets with the 8 byte buffer
 * at 16 kbps yields an service time of 4 ms - long enough to preclude
 * the need for fancy buffering.  We queue a background task that copies
 * data out of the receive buffer into an skb, and the hardware driver
 * simply does nothing until we're done with the receive buffer and
 * reset it for a new packet.
 *
 * B channel processing is more complex, because of:
 *   1) the faster speed - 64 kbps,
 *   2) the lack of any on-chip buffering (it interrupts for every byte), and
 *   3) the lack of any chip support for HDLC encapsulation
 *
 * The HiSax driver can put each B channel into one of three modes -
 * L1_MODE_NULL (channel disabled), L1_MODE_TRANS (transparent data relay),
 * and L1_MODE_HDLC (HDLC encapsulation by low-level driver).
 * L1_MODE_HDLC is the most common, used for almost all "pure" digital
 * data sessions.  L1_MODE_TRANS is used for ISDN audio.
 *
 * HDLC B channel transmission is performed via a large buffer into
 * which the skb is copied while performing HDLC bit-stuffing.  A CRC
 * is computed and attached to the end of the buffer, which is then
 * passed to the low-level routines for raw transmission.  Once
 * transmission is complete, the hardware driver is set to enter HDLC
 * idle by successive transmission of mark (all 1) bytes, waiting for
 * the ISDN driver to prepare another packet for transmission and
 * deliver it.
 *
 * HDLC B channel reception is performed via an X-byte ring buffer
 * divided into N sections of X/N bytes each.  Defaults: X=256 bytes, N=4.
 * As the hardware driver notifies us that each section is full, we
 * hand it the next section and schedule a background task to peruse
 * the received section, bit-by-bit, with an HDLC decoder.  As
 * packets are detected, they are copied into a large buffer while
 * decoding HDLC bit-stuffing.  The ending CRC is verified, and if
 * it is correct, we alloc a new skb of the correct length (which we
 * now know), copy the packet into it, and hand it to the upper layers.
 * Optimization: for large packets, we hand the buffer (which also
 * happens to be an skb) directly to the upper layer after an skb_trim,
 * and alloc a new large buffer for future packets, thus avoiding a copy.
 * Then we return to HDLC processing; state is saved between calls.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/soundcard.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/sbus.h>

#include <asm/audioio.h>
#include "amd7930.h"

static __u8  bilinear2mulaw(__u8 data);
static __u8  mulaw2bilinear(__u8 data);
static __u8  linear2mulaw(__u16 data);
static __u16 mulaw2linear(__u8 data);

#if defined (AMD79C30_ISDN)
#include "../../isdn/hisax/hisax.h"
#include "../../isdn/hisax/isdnl1.h"
#include "../../isdn/hisax/foreign.h"
#endif

#define MAX_DRIVERS 1

static struct sparcaudio_driver drivers[MAX_DRIVERS];
static int num_drivers;

/* Each amd7930 chip has two bi-directional B channels and a D
 * channel available to the uproc.  This structure handles all
 * the buffering needed to transmit and receive via a single channel.
 */

#define CHANNEL_AVAILABLE	0x00
#define CHANNEL_INUSE_AUDIO_IN	0x01
#define CHANNEL_INUSE_AUDIO_OUT	0x02
#define CHANNEL_INUSE_ISDN_B1	0x04
#define CHANNEL_INUSE_ISDN_B2	0x08
#define CHANNEL_INUSE		0xff

struct amd7930_channel {
	/* Channel status */
	u8 channel_status;

	/* Current buffer that the driver is playing on channel */
	volatile __u8 * output_ptr;
	volatile u32 output_count;
	u8 xmit_idle_char;

	/* Callback routine (and argument) when output is done on */
	void (*output_callback)(void *, unsigned char);
	void * output_callback_arg;

	/* Current buffer that the driver is recording on channel */
	volatile __u8 * input_ptr;
	volatile u32 input_count;
	volatile u32 input_limit;

	/* Callback routine (and argument) when input is done on */
	void (*input_callback)(void *, unsigned char, unsigned long);
	void * input_callback_arg;

	int input_format;
	int output_format;
};

/* Private information we store for each amd7930 chip. */
struct amd7930_info {
	struct amd7930_channel D;
	struct amd7930_channel Bb;
	struct amd7930_channel Bc;

	/* Pointers to which B channels are being used for what
	 * These three fields (Baudio, Bisdn[0], and Bisdn[1]) will either
	 * be NULL or point to one of the Bb/Bc structures above.
	 */
	struct amd7930_channel *Baudio;
	struct amd7930_channel *Bisdn[2];

	/* Device registers information. */
	unsigned long regs;
	unsigned long regs_size;
	struct amd7930_map map;

	/* Volume information. */
	int pgain, rgain, mgain;

	/* Device interrupt information. */
	int irq;
	volatile int ints_on;

	/* Format type */
	int format_type;

	/* Someone to signal when the ISDN LIU state changes */
	int liu_state;
	void (*liu_callback)(void *);
	void *liu_callback_arg;
};

/* Output a 16-bit quantity in the order that the amd7930 expects. */
static __inline__ void amd7930_out16(unsigned long regs, u16 val)
{
	sbus_writeb(val & 0xff, regs + DR);
	sbus_writeb(val >> 8, regs + DR);
}

/* gx, gr & stg gains.  this table must contain 256 elements with
 * the 0th being "infinity" (the magic value 9008).  The remaining
 * elements match sun's gain curve (but with higher resolution):
 * -18 to 0dB in .16dB steps then 0 to 12dB in .08dB steps.
 */
static __const__ __u16 gx_coeff[256] = {
	0x9008, 0x8b7c, 0x8b51, 0x8b45, 0x8b42, 0x8b3b, 0x8b36, 0x8b33,
	0x8b32, 0x8b2a, 0x8b2b, 0x8b2c, 0x8b25, 0x8b23, 0x8b22, 0x8b22,
	0x9122, 0x8b1a, 0x8aa3, 0x8aa3, 0x8b1c, 0x8aa6, 0x912d, 0x912b,
	0x8aab, 0x8b12, 0x8aaa, 0x8ab2, 0x9132, 0x8ab4, 0x913c, 0x8abb,
	0x9142, 0x9144, 0x9151, 0x8ad5, 0x8aeb, 0x8a79, 0x8a5a, 0x8a4a,
	0x8b03, 0x91c2, 0x91bb, 0x8a3f, 0x8a33, 0x91b2, 0x9212, 0x9213,
	0x8a2c, 0x921d, 0x8a23, 0x921a, 0x9222, 0x9223, 0x922d, 0x9231,
	0x9234, 0x9242, 0x925b, 0x92dd, 0x92c1, 0x92b3, 0x92ab, 0x92a4,
	0x92a2, 0x932b, 0x9341, 0x93d3, 0x93b2, 0x93a2, 0x943c, 0x94b2,
	0x953a, 0x9653, 0x9782, 0x9e21, 0x9d23, 0x9cd2, 0x9c23, 0x9baa,
	0x9bde, 0x9b33, 0x9b22, 0x9b1d, 0x9ab2, 0xa142, 0xa1e5, 0x9a3b,
	0xa213, 0xa1a2, 0xa231, 0xa2eb, 0xa313, 0xa334, 0xa421, 0xa54b,
	0xada4, 0xac23, 0xab3b, 0xaaab, 0xaa5c, 0xb1a3, 0xb2ca, 0xb3bd,
	0xbe24, 0xbb2b, 0xba33, 0xc32b, 0xcb5a, 0xd2a2, 0xe31d, 0x0808,
	0x72ba, 0x62c2, 0x5c32, 0x52db, 0x513e, 0x4cce, 0x43b2, 0x4243,
	0x41b4, 0x3b12, 0x3bc3, 0x3df2, 0x34bd, 0x3334, 0x32c2, 0x3224,
	0x31aa, 0x2a7b, 0x2aaa, 0x2b23, 0x2bba, 0x2c42, 0x2e23, 0x25bb,
	0x242b, 0x240f, 0x231a, 0x22bb, 0x2241, 0x2223, 0x221f, 0x1a33,
	0x1a4a, 0x1acd, 0x2132, 0x1b1b, 0x1b2c, 0x1b62, 0x1c12, 0x1c32,
	0x1d1b, 0x1e71, 0x16b1, 0x1522, 0x1434, 0x1412, 0x1352, 0x1323,
	0x1315, 0x12bc, 0x127a, 0x1235, 0x1226, 0x11a2, 0x1216, 0x0a2a,
	0x11bc, 0x11d1, 0x1163, 0x0ac2, 0x0ab2, 0x0aab, 0x0b1b, 0x0b23,
	0x0b33, 0x0c0f, 0x0bb3, 0x0c1b, 0x0c3e, 0x0cb1, 0x0d4c, 0x0ec1,
	0x079a, 0x0614, 0x0521, 0x047c, 0x0422, 0x03b1, 0x03e3, 0x0333,
	0x0322, 0x031c, 0x02aa, 0x02ba, 0x02f2, 0x0242, 0x0232, 0x0227,
	0x0222, 0x021b, 0x01ad, 0x0212, 0x01b2, 0x01bb, 0x01cb, 0x01f6,
	0x0152, 0x013a, 0x0133, 0x0131, 0x012c, 0x0123, 0x0122, 0x00a2,
	0x011b, 0x011e, 0x0114, 0x00b1, 0x00aa, 0x00b3, 0x00bd, 0x00ba,
	0x00c5, 0x00d3, 0x00f3, 0x0062, 0x0051, 0x0042, 0x003b, 0x0033,
	0x0032, 0x002a, 0x002c, 0x0025, 0x0023, 0x0022, 0x001a, 0x0021,
	0x001b, 0x001b, 0x001d, 0x0015, 0x0013, 0x0013, 0x0012, 0x0012,
	0x000a, 0x000a, 0x0011, 0x0011, 0x000b, 0x000b, 0x000c, 0x000e,
};

static __const__ __u16 ger_coeff[] = {
	0x431f, /* 5. dB */
	0x331f, /* 5.5 dB */
	0x40dd, /* 6. dB */
	0x11dd, /* 6.5 dB */
	0x440f, /* 7. dB */
	0x411f, /* 7.5 dB */
	0x311f, /* 8. dB */
	0x5520, /* 8.5 dB */
	0x10dd, /* 9. dB */
	0x4211, /* 9.5 dB */
	0x410f, /* 10. dB */
	0x111f, /* 10.5 dB */
	0x600b, /* 11. dB */
	0x00dd, /* 11.5 dB */
	0x4210, /* 12. dB */
	0x110f, /* 13. dB */
	0x7200, /* 14. dB */
	0x2110, /* 15. dB */
	0x2200, /* 15.9 dB */
	0x000b, /* 16.9 dB */
	0x000f  /* 18. dB */
};
#define NR_GER_COEFFS (sizeof(ger_coeff) / sizeof(ger_coeff[0]))

/* Enable amd7930 interrupts atomically. */
static void amd7930_enable_ints(struct amd7930_info *info)
{
	unsigned long flags;

	save_and_cli(flags);
	if (!info->ints_on) {
		sbus_writeb(AMR_INIT, info->regs + CR);
		sbus_writeb(AM_INIT_ACTIVE, info->regs + DR);
		info->ints_on = 1;
	}
	restore_flags(flags);
}

/* Disable amd7930 interrupts atomically. */
static __inline__ void amd7930_disable_ints(struct amd7930_info *info)
{
	unsigned long flags;

	save_and_cli(flags);
	if (info->ints_on) {
		sbus_writeb(AMR_INIT, info->regs + CR);
		sbus_writeb(AM_INIT_ACTIVE | AM_INIT_DISABLE_INTS,
			    info->regs + DR);
		info->ints_on = 0;
	}
	restore_flags(flags);

}  

/* Idle amd7930 (no interrupts, no audio, no data) */
static __inline__ void amd7930_idle(struct amd7930_info *info)
{
	unsigned long flags;

	save_and_cli(flags);
	if (info->ints_on) {
		sbus_writeb(AMR_INIT, info->regs + CR);
		sbus_writeb(0, info->regs + DR);
		info->ints_on = 0;
	}
	restore_flags(flags);
}  

/* Commit the local copy of the MAP registers to the amd7930. */
static void amd7930_write_map(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;
	unsigned long        regs = info->regs;
	struct amd7930_map  *map  = &info->map;
	unsigned long flags;

	save_and_cli(flags);

	sbus_writeb(AMR_MAP_GX, regs + CR);
	amd7930_out16(regs, map->gx);

	sbus_writeb(AMR_MAP_GR, regs + CR);
	amd7930_out16(regs, map->gr);

	sbus_writeb(AMR_MAP_STGR, regs + CR);
	amd7930_out16(regs, map->stgr);

	sbus_writeb(AMR_MAP_GER, regs + CR);
	amd7930_out16(regs, map->ger);

	sbus_writeb(AMR_MAP_MMR1, regs + CR);
	sbus_writeb(map->mmr1, regs + DR);

	sbus_writeb(AMR_MAP_MMR2, regs + CR);
	sbus_writeb(map->mmr2, regs + DR);

	restore_flags(flags);
}

/* Update the MAP registers with new settings. */
static void amd7930_update_map(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;
	struct amd7930_map  *map  = &info->map;
	int level;

	map->gx = gx_coeff[info->rgain];
	map->stgr = gx_coeff[info->mgain];

	level = (info->pgain * (256 + NR_GER_COEFFS)) >> 8;
	if (level >= 256) {
		map->ger = ger_coeff[level - 256];
		map->gr = gx_coeff[255];
	} else {
		map->ger = ger_coeff[0];
		map->gr = gx_coeff[level];
	}

	amd7930_write_map(drv);
}

/* Bit of a hack here - if the HISAX ISDN driver has got INTSTAT debugging
 * turned on, we send debugging characters to the ISDN driver:
 *
 *   i# - Interrupt received - number from 0 to 7 is low three bits of IR
 *   >  - Loaded a single char into the Dchan xmit FIFO
 *   +  - Finished loading an xmit packet into the Dchan xmit FIFO
 *   <  - Read a single char from the Dchan recv FIFO
 *   !  - Finished reading a packet from the Dchan recv FIFO
 *
 * This code needs to be removed if anything other than HISAX uses the ISDN
 * driver, since D.output_callback_arg is assumed to be a certain struct ptr
 */

#ifdef L2FRAME_DEBUG

inline void debug_info(struct amd7930_info *info, char c)
{
	struct IsdnCardState *cs;

	if (!info || !info->D.output_callback_arg)
		return;

	cs = (struct IsdnCardState *) info->D.output_callback_arg;

	if (!cs || !cs->status_write)
		return;

	if (cs->debug & L1_DEB_INTSTAT) {
		*(cs->status_write++) = c;
		if (cs->status_write > cs->status_end)
			cs->status_write = cs->status_buf;
	}
}

#else

#define debug_info(info,c)

#endif

static void fill_D_xmit_fifo(struct amd7930_info *info)
{
	/* Send next byte(s) of outgoing data. */
	while (info->D.output_ptr && info->D.output_count > 0 &&
               (sbus_readb(info->regs + DSR2) & AMR_DSR2_TBE)) {
		u8 byte = *(info->D.output_ptr);

		/* Send the next byte and advance buffer pointer. */
		sbus_writeb(byte, info->regs + DCTB);
		info->D.output_ptr++;
		info->D.output_count--;

		debug_info(info, '>');
        }
}

static void transceive_Dchannel(struct amd7930_info *info)
{
	__u8 dummy;

#define D_XMIT_ERRORS (AMR_DER_COLLISION | AMR_DER_UNRN)
#define D_RECV_ERRORS (AMR_DER_RABRT | AMR_DER_RFRAME | AMR_DER_FCS | \
			AMR_DER_OVFL | AMR_DER_UNFL | AMR_DER_OVRN)

	/* Transmit if we can */
	fill_D_xmit_fifo(info);

	/* Done with the xmit buffer? Notify the midlevel driver. */
	if (info->D.output_ptr != NULL && info->D.output_count == 0) {
		info->D.output_ptr = NULL;
		info->D.output_count = 0;
		debug_info(info, '+');
		if (info->D.output_callback)
			(*info->D.output_callback)
				(info->D.output_callback_arg,
				 sbus_readb(info->regs + DER));
				 /* sbus_readb(info->regs + DER) & D_XMIT_ERRORS); */
	}

	/* Read the next byte(s) of incoming data. */

	while (sbus_readb(info->regs + DSR2) & AMR_DSR2_RBA) {
		if (info->D.input_ptr &&
		    (info->D.input_count < info->D.input_limit)) {
			/* Get the next byte and advance buffer pointer. */
			*(info->D.input_ptr) = sbus_readb(info->regs + DCRB);
			info->D.input_ptr++;
			info->D.input_count++;
		} else {
			/* Overflow - should be detected by chip via RBLR
			 * so we'll just consume data until we see LBRP
			 */
			dummy = sbus_readb(info->regs + DCRB);
		}

		debug_info(info, '<');

		if (sbus_readb(info->regs + DSR2) & AMR_DSR2_LBRP) {
			__u8 der;

			/* End of recv packet? Notify the midlevel driver. */
			debug_info(info, '!');
			info->D.input_ptr = NULL;
			der = sbus_readb(info->regs + DER) & D_RECV_ERRORS;

			/* Read receive byte count - advances FIFOs */
			sbus_writeb(AMR_DLC_DRCR, info->regs + CR);
			dummy = sbus_readb(info->regs + DR);
			dummy = sbus_readb(info->regs + DR);

			if (info->D.input_callback)
				(*info->D.input_callback)
					(info->D.input_callback_arg, der,
					 info->D.input_count);
		}

	}
}

long amd7930_xmit_idles = 0;

static void transceive_Bchannel(struct amd7930_channel *channel,
				unsigned long reg)
{
	/* Send the next byte of outgoing data. */
	if (channel->output_ptr && channel->output_count > 0) {
		u8 byte;

		/* Send the next byte and advance buffer pointer. */
		switch(channel->output_format) {
		case AUDIO_ENCODING_ULAW:
		case AUDIO_ENCODING_ALAW:
			byte = *(channel->output_ptr);
			sbus_writeb(byte, reg);
			break;
		case AUDIO_ENCODING_LINEAR8:
			byte = bilinear2mulaw(*(channel->output_ptr));
			sbus_writeb(byte, reg);
			break;
		case AUDIO_ENCODING_LINEAR:
			if (channel->output_count >= 2) {
				u16 val = channel->output_ptr[0] << 8;

				val |= channel->output_ptr[1];
				byte = linear2mulaw(val);
				sbus_writeb(byte, reg);
				channel->output_ptr++;
				channel->output_count--;
			};
		};
		channel->output_ptr++;
		channel->output_count--;


		/* Done with the buffer? Notify the midlevel driver. */
		if (channel->output_count == 0) {
			channel->output_ptr = NULL;
			channel->output_count = 0;
			if (channel->output_callback)
				(*channel->output_callback)
					(channel->output_callback_arg,1);
		}
	} else {
		sbus_writeb(channel->xmit_idle_char, reg);
		amd7930_xmit_idles++;
        }

	/* Read the next byte of incoming data. */
	if (channel->input_ptr && channel->input_count > 0) {
		/* Get the next byte and advance buffer pointer. */
		switch(channel->input_format) {
		case AUDIO_ENCODING_ULAW:
		case AUDIO_ENCODING_ALAW:
			*(channel->input_ptr) = sbus_readb(reg);
			break;
		case AUDIO_ENCODING_LINEAR8:
			*(channel->input_ptr) = mulaw2bilinear(sbus_readb(reg));
			break;
		case AUDIO_ENCODING_LINEAR:
			if (channel->input_count >= 2) {
				u16 val = mulaw2linear(sbus_readb(reg));
				channel->input_ptr[0] = val >> 8;
				channel->input_ptr[1] = val & 0xff;
				channel->input_ptr++;
				channel->input_count--;
			} else {
				*(channel->input_ptr) = 0;
			}
		};
		channel->input_ptr++;
		channel->input_count--;

		/* Done with the buffer? Notify the midlevel driver. */
		if (channel->input_count == 0) {
			channel->input_ptr = NULL;
			channel->input_count = 0;
			if (channel->input_callback)
				(*channel->input_callback)
					(channel->input_callback_arg, 1, 0);
		}
	}
}

/* Interrupt handler (The chip takes only one byte per interrupt. Grrr!) */
static void amd7930_interrupt(int irq, void *dev_id, struct pt_regs *intr_regs)
{
	struct sparcaudio_driver *drv = (struct sparcaudio_driver *) dev_id;
	struct amd7930_info *info = (struct amd7930_info *) drv->private;
	unsigned long regs = info->regs;
	__u8 ir;

	/* Clear the interrupt. */
	ir = sbus_readb(regs + IR);

	if (ir & AMR_IR_BBUF) {
		if (info->Bb.channel_status == CHANNEL_INUSE)
			transceive_Bchannel(&info->Bb, info->regs + BBTB);
		if (info->Bc.channel_status == CHANNEL_INUSE)
			transceive_Bchannel(&info->Bc, info->regs + BCTB);
	}

	if (ir & (AMR_IR_DRTHRSH | AMR_IR_DTTHRSH | AMR_IR_DSRI)) {
		debug_info(info, 'i');
		debug_info(info, '0' + (ir&7));
		transceive_Dchannel(info);
	}

	if (ir & AMR_IR_LSRI) {
		__u8 lsr;

		sbus_writeb(AMR_LIU_LSR, regs + CR);
		lsr = sbus_readb(regs + DR);

                info->liu_state = (lsr & 0x7) + 2;

                if (info->liu_callback)
			(*info->liu_callback)(info->liu_callback_arg);
        }
}

static int amd7930_open(struct inode * inode, struct file * file,
			struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	switch(MINOR(inode->i_rdev) & 0xf) {
	case SPARCAUDIO_AUDIO_MINOR:
		info->format_type = AUDIO_ENCODING_ULAW;
		break;
	case SPARCAUDIO_DSP_MINOR:
		info->format_type = AUDIO_ENCODING_LINEAR8;
		break;
	case SPARCAUDIO_DSP16_MINOR:
		info->format_type = AUDIO_ENCODING_LINEAR;
		break;
	};

	MOD_INC_USE_COUNT;
	return 0;
}

static void amd7930_release(struct inode * inode, struct file * file,
			    struct sparcaudio_driver *drv)
{
	/* amd7930_disable_ints(drv->private); */
	MOD_DEC_USE_COUNT;
}

static void request_Baudio(struct amd7930_info *info)
{
	if (info->Bb.channel_status == CHANNEL_AVAILABLE) {
		info->Bb.channel_status = CHANNEL_INUSE;
		info->Baudio = &info->Bb;

		/* Multiplexor map - audio (Ba) to Bb */
		sbus_writeb(AMR_MUX_MCR1, info->regs + CR);
		sbus_writeb(AM_MUX_CHANNEL_Ba | (AM_MUX_CHANNEL_Bb << 4),
			    info->regs + DR);

		/* Enable B channel interrupts */
		sbus_writeb(AMR_MUX_MCR4, info->regs + CR);
		sbus_writeb(AM_MUX_MCR4_ENABLE_INTS, info->regs + DR);
	} else if (info->Bc.channel_status == CHANNEL_AVAILABLE) {
		info->Bc.channel_status = CHANNEL_INUSE;
		info->Baudio = &info->Bc;

		/* Multiplexor map - audio (Ba) to Bc */
		sbus_writeb(AMR_MUX_MCR1, info->regs + CR);
		sbus_writeb(AM_MUX_CHANNEL_Ba | (AM_MUX_CHANNEL_Bc << 4),
			    info->regs + DR);

		/* Enable B channel interrupts */
		sbus_writeb(AMR_MUX_MCR4, info->regs + CR);
		sbus_writeb(AM_MUX_MCR4_ENABLE_INTS, info->regs + DR);
	}
}

static void release_Baudio(struct amd7930_info *info)
{
	if (info->Baudio) {
		info->Baudio->channel_status = CHANNEL_AVAILABLE;
		sbus_writeb(AMR_MUX_MCR1, info->regs + CR);
		sbus_writeb(0, info->regs + DR);
		info->Baudio = NULL;

		if (info->Bb.channel_status == CHANNEL_AVAILABLE &&
		    info->Bc.channel_status == CHANNEL_AVAILABLE) {
			/* Disable B channel interrupts */
			sbus_writeb(AMR_MUX_MCR4, info->regs + CR);
			sbus_writeb(0, info->regs + DR);
		}
	}
}

static void amd7930_start_output(struct sparcaudio_driver *drv,
				 __u8 * buffer, unsigned long count)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	if (! info->Baudio)
		request_Baudio(info);

	if (info->Baudio) {
		info->Baudio->output_ptr = buffer;
		info->Baudio->output_count = count;
		info->Baudio->output_format = info->format_type;
		info->Baudio->output_callback = (void *) &sparcaudio_output_done;
		info->Baudio->output_callback_arg = (void *) drv;
		info->Baudio->xmit_idle_char = 0;
	}
}

static void amd7930_stop_output(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	if (info->Baudio) {
		info->Baudio->output_ptr = NULL;
		info->Baudio->output_count = 0;
		if (! info->Baudio->input_ptr)
			release_Baudio(info);
	}
}

static void amd7930_start_input(struct sparcaudio_driver *drv,
				__u8 * buffer, unsigned long count)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	if (! info->Baudio)
		request_Baudio(info);

	if (info->Baudio) {
		info->Baudio->input_ptr = buffer;
		info->Baudio->input_count = count;
		info->Baudio->input_format = info->format_type;
		info->Baudio->input_callback = (void *) &sparcaudio_input_done;
		info->Baudio->input_callback_arg = (void *) drv;
	}
}

static void amd7930_stop_input(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	if (info->Baudio) {
		info->Baudio->input_ptr = NULL;
		info->Baudio->input_count = 0;
		if (! info->Baudio->output_ptr)
			release_Baudio(info);
	}

}

static void amd7930_sunaudio_getdev(struct sparcaudio_driver *drv,
				 audio_device_t * audinfo)
{
	strncpy(audinfo->name, "SUNW,am79c30", sizeof(audinfo->name) - 1);
	strncpy(audinfo->version, "a", sizeof(audinfo->version) - 1);
	strncpy(audinfo->config, "onboard1", sizeof(audinfo->config) - 1);
}

static int amd7930_sunaudio_getdev_sunos(struct sparcaudio_driver *drv)
{
	return AUDIO_DEV_AMD;
}

static int amd7930_get_formats(struct sparcaudio_driver *drv)
{
      return (AFMT_MU_LAW | AFMT_A_LAW | AFMT_U8 | AFMT_S16_BE);
}

static int amd7930_get_output_ports(struct sparcaudio_driver *drv)
{
      return (AUDIO_SPEAKER | AUDIO_HEADPHONE);
}

static int amd7930_get_input_ports(struct sparcaudio_driver *drv)
{
      return (AUDIO_MICROPHONE);
}

static int amd7930_set_output_volume(struct sparcaudio_driver *drv, int vol)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	info->pgain = vol;
	amd7930_update_map(drv);
	return 0;
}

static int amd7930_get_output_volume(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	return info->pgain;
}

static int amd7930_set_input_volume(struct sparcaudio_driver *drv, int vol)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	info->rgain = vol;
	amd7930_update_map(drv);
	return 0;
}

static int amd7930_get_input_volume(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	return info->rgain;
}

static int amd7930_set_monitor_volume(struct sparcaudio_driver *drv, int vol)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	info->mgain = vol;
	amd7930_update_map(drv);
	return 0;
}

static int amd7930_get_monitor_volume(struct sparcaudio_driver *drv)
{
      struct amd7930_info *info = (struct amd7930_info *) drv->private;

      return info->mgain;
}

/* Cheats. The amd has the minimum capabilities we support */
static int amd7930_get_output_balance(struct sparcaudio_driver *drv)
{
	return AUDIO_MID_BALANCE;
}

static int amd7930_get_input_balance(struct sparcaudio_driver *drv)
{
	return AUDIO_MID_BALANCE;
}

static int amd7930_get_output_channels(struct sparcaudio_driver *drv)
{
	return AUDIO_MIN_PLAY_CHANNELS;
}

static int amd7930_set_output_channels(struct sparcaudio_driver *drv, 
				       int value)
{
  return (value == AUDIO_MIN_PLAY_CHANNELS) ? 0 : -EINVAL;
}

static int amd7930_get_input_channels(struct sparcaudio_driver *drv)
{
	return AUDIO_MIN_REC_CHANNELS;
}

static int 
amd7930_set_input_channels(struct sparcaudio_driver *drv, int value)
{
	return (value == AUDIO_MIN_REC_CHANNELS) ? 0 : -EINVAL;
}

static int amd7930_get_output_precision(struct sparcaudio_driver *drv)
{
	return AUDIO_MIN_PLAY_PRECISION;
}

static int 
amd7930_set_output_precision(struct sparcaudio_driver *drv, int value)
{
	return (value == AUDIO_MIN_PLAY_PRECISION) ? 0 : -EINVAL;
}

static int amd7930_get_input_precision(struct sparcaudio_driver *drv)
{
	return AUDIO_MIN_REC_PRECISION;
}

static int 
amd7930_set_input_precision(struct sparcaudio_driver *drv, int value)
{
	return (value == AUDIO_MIN_REC_PRECISION) ? 0 : -EINVAL;
}

static int amd7930_get_output_port(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	if (info->map.mmr2 & AM_MAP_MMR2_LS)
		return AUDIO_SPEAKER; 

	return AUDIO_HEADPHONE;
}

static int amd7930_set_output_port(struct sparcaudio_driver *drv, int value)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	switch (value) {
	case AUDIO_HEADPHONE:
		info->map.mmr2 &= ~AM_MAP_MMR2_LS;
		break;
	case AUDIO_SPEAKER:
		info->map.mmr2 |= AM_MAP_MMR2_LS;
		break;
	default:
		return -EINVAL;
	};

	amd7930_update_map(drv);
	return 0;
}

/* Only a microphone here, so no troubles */
static int amd7930_get_input_port(struct sparcaudio_driver *drv)
{
	return AUDIO_MICROPHONE;
}

static int amd7930_get_encoding(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	if ((info->map.mmr1 & AM_MAP_MMR1_ALAW) && 
	    (info->format_type == AUDIO_ENCODING_ALAW))
		return AUDIO_ENCODING_ALAW;

	return info->format_type;
}

static int 
amd7930_set_encoding(struct sparcaudio_driver *drv, int value)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	switch (value) {
	case AUDIO_ENCODING_ALAW:
		info->map.mmr1 |= AM_MAP_MMR1_ALAW;
		break;
	case AUDIO_ENCODING_LINEAR8:
	case AUDIO_ENCODING_LINEAR:
	case AUDIO_ENCODING_ULAW:
		info->map.mmr1 &= ~AM_MAP_MMR1_ALAW;
		break;
	default:
		return -EINVAL;
	};

	info->format_type = value;

	amd7930_update_map(drv);
	return 0;
}

/* This is what you get. Take it or leave it */
static int amd7930_get_output_rate(struct sparcaudio_driver *drv)
{
	return AMD7930_RATE;
}

static int 
amd7930_set_output_rate(struct sparcaudio_driver *drv, int value)
{
	return (value == AMD7930_RATE) ? 0 : -EINVAL;
}

static int amd7930_get_input_rate(struct sparcaudio_driver *drv)
{
	return AMD7930_RATE;
}

static int
amd7930_set_input_rate(struct sparcaudio_driver *drv, int value)
{
	return (value == AMD7930_RATE) ? 0 : -EINVAL;
}

static int amd7930_get_output_muted(struct sparcaudio_driver *drv)
{
      return 0;
}

static void amd7930_loopback(struct sparcaudio_driver *drv, unsigned int value)
{
	struct amd7930_info *info = (struct amd7930_info *) drv->private;

	if (value)
		info->map.mmr1 |= AM_MAP_MMR1_LOOPBACK;
	else
		info->map.mmr1 &= ~AM_MAP_MMR1_LOOPBACK;
	amd7930_update_map(drv);
}

static int amd7930_ioctl(struct inode * inode, struct file * file,
                         unsigned int cmd, unsigned long arg, 
                         struct sparcaudio_driver *drv)
{
	int retval = 0;
  
	switch (cmd) {
	case AUDIO_DIAG_LOOPBACK:
		amd7930_loopback(drv, (unsigned int)arg);
		break;
	default:
		retval = -EINVAL;
	};

	return retval;
}


/*
 *       ISDN operations
 *
 * Many of these routines take an "int dev" argument, which is simply
 * an index into the drivers[] array.  Currently, we only support a
 * single AMD 7930 chip, so the value should always be 0.  B channel
 * operations require an "int chan", which should be 0 for channel B1
 * and 1 for channel B2
 *
 * int amd7930_get_irqnum(int dev)
 *
 *   returns the interrupt number being used by the chip.  ISDN4linux
 *   uses this number to watch the interrupt during initialization and
 *   make sure something is happening.
 *
 * int amd7930_get_liu_state(int dev)
 *
 *   returns the current state of the ISDN Line Interface Unit (LIU)
 *   as a number between 2 (state F2) and 7 (state F7).  0 may also be
 *   returned if the chip doesn't exist or the LIU hasn't been
 *   activated.  The meanings of the states are defined in I.430, ISDN
 *   BRI Physical Layer Interface.  The most important two states are
 *   F3 (shutdown) and F7 (syncronized).
 *
 * void amd7930_liu_init(int dev, void (*callback)(), void *callback_arg)
 *
 *   initializes the LIU and optionally registers a callback to be
 *   signaled upon a change of LIU state.  The callback will be called
 *   with a single opaque callback_arg Once the callback has been
 *   triggered, amd7930_get_liu_state can be used to determine the LIU
 *   current state.
 *
 * void amd7930_liu_activate(int dev, int priority)
 *
 *   requests LIU activation at a given D-channel priority.
 *   Successful activatation is achieved upon entering state F7, which
 *   will trigger any callback previously registered with
 *   amd7930_liu_init.
 *
 * void amd7930_liu_deactivate(int dev)
 *
 *   deactivates LIU.  Outstanding D and B channel transactions are
 *   terminated rudely and without callback notification.  LIU change
 *   of state callback will be triggered, however.
 *
 * void amd7930_dxmit(int dev, __u8 *buffer, unsigned int count,
 *               void (*callback)(void *, int), void *callback_arg)
 *
 *   transmits a packet - specified with buffer, count - over the D-channel
 *   interface.  Buffer should begin with the LAPD address field and
 *   end with the information field.  FCS and flag sequences should not
 *   be included, nor is bit-stuffing required - all these functions are
 *   performed by the chip.  The callback function will be called
 *   DURING THE TOP HALF OF AN INTERRUPT HANDLER and will be passed
 *   both the arbitrary callback_arg and an integer error indication:
 *
 *       0 - successful transmission; ready for next packet
 *   non-0 - error value from chip's DER (D-Channel Error Register):
 *       4 - collision detect
 *     128 - underrun; irq routine didn't service chip fast enough
 *
 *   The callback routine should defer any time-consuming operations
 *   to a bottom-half handler; however, amd7930_dxmit may be called
 *   from within the callback to request back-to-back transmission of
 *   a second packet (without repeating the priority/collision mechanism)
 *
 *   A comment about the "collision detect" error, which is signalled
 *   whenever the echoed D-channel data didn't match the transmitted
 *   data.  This is part of ISDN's normal multi-drop T-interface
 *   operation, indicating that another device has attempted simultaneous
 *   transmission, but can also result from line noise.  An immediate
 *   requeue via amd7930_dxmit is suggested, but repeated collision
 *   errors may indicate a more serious problem.
 *
 * void amd7930_drecv(int dev, __u8 *buffer, unsigned int size,
 *               void (*callback)(void *, int, unsigned int),
 *               void *callback_arg)
 *
 *   register a buffer - buffer, size - into which a D-channel packet
 *   can be received.  The callback function will be called DURING
 *   THE TOP HALF OF AN INTERRUPT HANDLER and will be passed an
 *   arbitrary callback_arg, an integer error indication and the length
 *   of the received packet, which will start with the address field,
 *   end with the information field, and not contain flag or FCS
 *   bytes.  Bit-stuffing will already have been corrected for.
 *   Possible values of second callback argument "error":
 *
 *       0 - successful reception
 *   non-0 - error value from chip's DER (D-Channel Error Register):
 *       1 - received packet abort
 *       2 - framing error; non-integer number of bytes received
 *       8 - FCS error; CRC sequence indicated corrupted data
 *      16 - overflow error; packet exceeded size of buffer
 *      32 - underflow error; packet smaller than required five bytes
 *      64 - overrun error; irq routine didn't service chip fast enough
 *
 * int amd7930_bopen(int dev, int chan, u_char xmit_idle_char)
 *
 *   This function should be called before any other operations on a B
 *   channel.  In addition to arranging for interrupt handling and
 *   channel multiplexing, it sets the xmit_idle_char which is
 *   transmitted on the interface when no data buffer is available.
 *   Suggested values are: 0 for ISDN audio; FF for HDLC mark idle; 7E
 *   for HDLC flag idle.  Returns 0 on a successful open; -1 on error,
 *   which is quite possible if audio and the other ISDN channel are
 *   already in use, since the Am7930 can only send two of the three
 *   channels to the processor
 *
 * void amd7930_bclose(int dev, int chan)
 *
 *   Shuts down a B channel when no longer in use.
 *
 * void amd7930_bxmit(int dev, int chan, __u8 *buffer, unsigned int count,
 *               void (*callback)(void *), void *callback_arg)
 *
 *   transmits a raw data block - specified with buffer, count - over
 *   the B channel interface specified by dev/chan.  The callback
 *   function will be called DURING THE TOP HALF OF AN INTERRUPT
 *   HANDLER and will be passed the arbitrary callback_arg
 *
 *   The callback routine should defer any time-consuming operations
 *   to a bottom-half handler; however, amd7930_bxmit may be called
 *   from within the callback to request back-to-back transmission of
 *   another data block
 *
 * void amd7930_brecv(int dev, int chan, __u8 *buffer, unsigned int size,
 *               void (*callback)(void *), void *callback_arg)
 *
 *   receive a raw data block - specified with buffer, size - over the
 *   B channel interface specified by dev/chan.  The callback function
 *   will be called DURING THE TOP HALF OF AN INTERRUPT HANDLER and
 *   will be passed the arbitrary callback_arg
 *
 *   The callback routine should defer any time-consuming operations
 *   to a bottom-half handler; however, amd7930_brecv may be called
 *   from within the callback to register another buffer and ensure
 *   continuous B channel reception without loss of data
 *
 */

#if defined (AMD79C30_ISDN)
static int amd7930_get_irqnum(int dev)
{
	struct amd7930_info *info;

	if (dev > num_drivers)
		return(0);

	info = (struct amd7930_info *) drivers[dev].private;

	return info->irq;
}

static int amd7930_get_liu_state(int dev)
{
	struct amd7930_info *info;

	if (dev > num_drivers)
		return(0);

	info = (struct amd7930_info *) drivers[dev].private;

	return info->liu_state;
}

static void amd7930_liu_init(int dev, void (*callback)(), void *callback_arg)
{
	struct amd7930_info *info;
	unsigned long flags;

	if (dev > num_drivers)
		return;

	info = (struct amd7930_info *) drivers[dev].private;

	save_and_cli(flags);

	/* Set callback for LIU state change */
        info->liu_callback = callback;
	info->liu_callback_arg = callback_arg;

	/* De-activate the ISDN Line Interface Unit (LIU) */
	sbus_writeb(AMR_LIU_LMR1, info->regs + CR);
	sbus_writeb(0, info->regs + DR);

	/* Request interrupt when LIU changes state from/to F3/F7/F8 */
	sbus_writeb(AMR_LIU_LMR2, info->regs + CR);
	sbus_writeb(AM_LIU_LMR2_EN_F3_INT |
		    AM_LIU_LMR2_EN_F7_INT |
		    AM_LIU_LMR2_EN_F8_INT,
		    info->regs + DR);

	/* amd7930_enable_ints(info); */

	/* Activate the ISDN Line Interface Unit (LIU) */
	sbus_writeb(AMR_LIU_LMR1, info->regs + CR);
	sbus_writeb(AM_LIU_LMR1_LIU_ENABL, info->regs + DR);

	restore_flags(flags);
}

static void amd7930_liu_activate(int dev, int priority)
{
	struct amd7930_info *info;
	unsigned long flags;

	if (dev > num_drivers)
		return;

	info = (struct amd7930_info *) drivers[dev].private;

	save_and_cli(flags);

        /* Set D-channel access priority
         *
         * I.430 defines a priority mechanism based on counting 1s
         * in the echo channel before transmitting
         *
         * Priority 0 is eight 1s; priority 1 is ten 1s; etc
         */
        sbus_writeb(AMR_LIU_LPR, info->regs + CR);
        sbus_writeb(priority & 0x0f, info->regs + DR);

	/* request LIU activation */
	sbus_writeb(AMR_LIU_LMR1, info->regs + CR);
	sbus_writeb(AM_LIU_LMR1_LIU_ENABL | AM_LIU_LMR1_REQ_ACTIV,
		    info->regs + DR);

	restore_flags(flags);
}

static void amd7930_liu_deactivate(int dev)
{
	struct amd7930_info *info;
	unsigned long flags;

	if (dev > num_drivers)
		return;

	info = (struct amd7930_info *) drivers[dev].private;

	save_and_cli(flags);

	/* deactivate LIU */
	sbus_writeb(AMR_LIU_LMR1, info->regs + CR);
	sbus_writeb(0, info->regs + DR);

	restore_flags(flags);
}

static void amd7930_dxmit(int dev, __u8 *buffer, unsigned int count,
			  void (*callback)(void *, int), void *callback_arg)
{
	struct amd7930_info *info;
	unsigned long flags;
	__u8 dmr1;

	if (dev > num_drivers)
		return;

	info = (struct amd7930_info *) drivers[dev].private;

	save_and_cli(flags);

	if (info->D.output_ptr) {
		restore_flags(flags);
		printk("amd7930_dxmit: transmitter in use\n");
		return;
	}

	info->D.output_ptr = buffer;
	info->D.output_count = count;
	info->D.output_callback = callback;
	info->D.output_callback_arg = callback_arg;

	/* Enable D-channel Transmit Threshold interrupt; disable addressing */
	sbus_writeb(AMR_DLC_DMR1, info->regs + CR);
	dmr1 = sbus_readb(info->regs + DR);
	dmr1 |= AMR_DLC_DMR1_DTTHRSH_INT;
	dmr1 &= ~AMR_DLC_DMR1_EN_ADDRS;
	sbus_writeb(dmr1, info->regs + DR);

	/* Begin xmit by setting D-channel Transmit Byte Count Reg (DTCR) */
	sbus_writeb(AMR_DLC_DTCR, info->regs + CR);
	sbus_writeb(count & 0xff, info->regs + DR);
	sbus_writeb((count >> 8) & 0xff, info->regs + DR);

	/* Prime xmit FIFO */
	/* fill_D_xmit_fifo(info); */
	transceive_Dchannel(info);

	restore_flags(flags);
}

static void amd7930_drecv(int dev, __u8 *buffer, unsigned int size,
			  void (*callback)(void *, int, unsigned int),
			  void *callback_arg)
{
	struct amd7930_info *info;
	unsigned long flags;
	__u8 dmr1;

	if (dev > num_drivers)
		return;

	info = (struct amd7930_info *) drivers[dev].private;

	save_and_cli(flags);

	if (info->D.input_ptr) {
		restore_flags(flags);
		printk("amd7930_drecv: receiver already has buffer!\n");
		return;
	}

	info->D.input_ptr = buffer;
	info->D.input_count = 0;
	info->D.input_limit = size;
	info->D.input_callback = callback;
	info->D.input_callback_arg = callback_arg;

	/* Enable D-channel Receive Threshold interrupt;
	 * Enable D-channel End of Receive Packet interrupt;
	 * Disable address recognition
	 */
	sbus_writeb(AMR_DLC_DMR1, info->regs + CR);
	dmr1 = sbus_readb(info->regs + DR);
	dmr1 |= AMR_DLC_DMR1_DRTHRSH_INT | AMR_DLC_DMR1_EORP_INT;
	dmr1 &= ~AMR_DLC_DMR1_EN_ADDRS;
	sbus_writeb(dmr1, info->regs + DR);

	/* Set D-channel Receive Byte Count Limit Register */
	sbus_writeb(AMR_DLC_DRCR, info->regs + CR);
	sbus_writeb(size & 0xff, info->regs + DR);
	sbus_writeb((size >> 8) & 0xff, info->regs + DR);

	restore_flags(flags);
}

static int amd7930_bopen(int dev, unsigned int chan, 
                         int mode, u_char xmit_idle_char)
{
	struct amd7930_info *info;
	unsigned long flags;
	u8 tmp;

	if (dev > num_drivers || chan<0 || chan>1)
		return -1;

	if (mode == L1_MODE_HDLC)
		return -1;
 
	info = (struct amd7930_info *) drivers[dev].private;

	save_and_cli(flags);

	if (info->Bb.channel_status == CHANNEL_AVAILABLE) {
		info->Bb.channel_status = CHANNEL_INUSE;
		info->Bb.xmit_idle_char = xmit_idle_char;
		info->Bisdn[chan] = &info->Bb;

		/* Multiplexor map - isdn (B1/2) to Bb */
		sbus_writeb(AMR_MUX_MCR2 + chan, info->regs + CR);
		sbus_writeb((AM_MUX_CHANNEL_B1 + chan) |
			    (AM_MUX_CHANNEL_Bb << 4),
			    info->regs + DR);
	} else if (info->Bc.channel_status == CHANNEL_AVAILABLE) {
		info->Bc.channel_status = CHANNEL_INUSE;
		info->Bc.xmit_idle_char = xmit_idle_char;
		info->Bisdn[chan] = &info->Bc;

		/* Multiplexor map - isdn (B1/2) to Bc */
		sbus_writeb(AMR_MUX_MCR2 + chan, info->regs + CR);
		sbus_writeb((AM_MUX_CHANNEL_B1 + chan) |
			    (AM_MUX_CHANNEL_Bc << 4),
			    info->regs + DR);
	} else {
		restore_flags(flags);
		return (-1);
	}

	/* Enable B channel transmit */
	sbus_writeb(AMR_LIU_LMR1, info->regs + CR);
	tmp = sbus_readb(info->regs + DR);
	tmp |= AM_LIU_LMR1_B1_ENABL + chan;
	sbus_writeb(tmp, info->regs + DR);

	/* Enable B channel interrupts */
	sbus_writeb(AMR_MUX_MCR4, info->regs + CR);
	sbus_writeb(AM_MUX_MCR4_ENABLE_INTS |
		    AM_MUX_MCR4_REVERSE_Bb |
		    AM_MUX_MCR4_REVERSE_Bc,
		    info->regs + DR);

	restore_flags(flags);
	return 0;
}

static void amd7930_bclose(int dev, unsigned int chan)
{
	struct amd7930_info *info;
	unsigned long flags;

	if (dev > num_drivers || chan<0 || chan>1)
		return;

	info = (struct amd7930_info *) drivers[dev].private;

	save_and_cli(flags);

	if (info->Bisdn[chan]) {
		u8 tmp;

		info->Bisdn[chan]->channel_status = CHANNEL_AVAILABLE;

		sbus_writeb(AMR_MUX_MCR2 + chan, info->regs + CR);
		sbus_writeb(0, info->regs + DR);

		info->Bisdn[chan] = NULL;

		/* Disable B channel transmit */
		sbus_writeb(AMR_LIU_LMR1, info->regs + CR);
		tmp = sbus_readb(info->regs + DR);
		tmp &= ~(AM_LIU_LMR1_B1_ENABL + chan);
		sbus_writeb(tmp, info->regs + DR);

		if (info->Bb.channel_status == CHANNEL_AVAILABLE &&
		    info->Bc.channel_status == CHANNEL_AVAILABLE) {
			/* Disable B channel interrupts */
			sbus_writeb(AMR_MUX_MCR4, info->regs + CR);
			sbus_writeb(0, info->regs + DR);
		}
	}

	restore_flags(flags);
}

static void amd7930_bxmit(int dev, unsigned int chan,
                          __u8 * buffer, unsigned long count,
			  void (*callback)(void *, int), void *callback_arg)
{
	struct amd7930_info *info;
	struct amd7930_channel *Bchan;
	unsigned long flags;

	if (dev > num_drivers)
		return;

	info = (struct amd7930_info *) drivers[dev].private;
	Bchan = info->Bisdn[chan];

	if (Bchan) {
		save_and_cli(flags);

		Bchan->output_ptr = buffer;
		Bchan->output_count = count;
		Bchan->output_format = AUDIO_ENCODING_ULAW;
	        Bchan->output_callback = (void *) callback;
        	Bchan->output_callback_arg = callback_arg;

		restore_flags(flags);
	}
}

static void amd7930_brecv(int dev, unsigned int chan, 
                          __u8 * buffer, unsigned long size,
                          void (*callback)(void *, int, unsigned int),
                          void *callback_arg)
{
	struct amd7930_info *info;
	struct amd7930_channel *Bchan;
	unsigned long flags;

	if (dev > num_drivers)
		return;

	info = (struct amd7930_info *) drivers[dev].private;
	Bchan = info->Bisdn[chan];

	if (Bchan) {
		save_and_cli(flags);

		Bchan->input_ptr = buffer;
		Bchan->input_count = size;
		Bchan->input_format = AUDIO_ENCODING_ULAW;
		Bchan->input_callback = (void *) callback;
		Bchan->input_callback_arg = callback_arg;

		restore_flags(flags);
	}
}

struct foreign_interface amd7930_foreign_interface = {
        amd7930_get_irqnum,
        amd7930_get_liu_state,
        amd7930_liu_init,
        amd7930_liu_activate,
        amd7930_liu_deactivate,
        amd7930_dxmit,
        amd7930_drecv,
        amd7930_bopen,
        amd7930_bclose,
        amd7930_bxmit,
        amd7930_brecv
};
EXPORT_SYMBOL(amd7930_foreign_interface);
#endif


/*
 *	Device detection and initialization.
 */

static struct sparcaudio_operations amd7930_ops = {
	amd7930_open,
	amd7930_release,
	amd7930_ioctl,
	amd7930_start_output,
	amd7930_stop_output,
	amd7930_start_input,
	amd7930_stop_input,
	amd7930_sunaudio_getdev,
	amd7930_set_output_volume,
	amd7930_get_output_volume,
	amd7930_set_input_volume,
	amd7930_get_input_volume,
	amd7930_set_monitor_volume,
	amd7930_get_monitor_volume,
	NULL,			/* amd7930_set_output_balance */
	amd7930_get_output_balance,
	NULL,			/* amd7930_set_input_balance */
	amd7930_get_input_balance,
	amd7930_set_output_channels,
	amd7930_get_output_channels,
	amd7930_set_input_channels,
	amd7930_get_input_channels,
	amd7930_set_output_precision,
	amd7930_get_output_precision,
	amd7930_set_input_precision,
	amd7930_get_input_precision,
	amd7930_set_output_port,
	amd7930_get_output_port,
	NULL,			/* amd7930_set_input_port */
	amd7930_get_input_port,
	amd7930_set_encoding,
	amd7930_get_encoding,
	amd7930_set_encoding,
	amd7930_get_encoding,
	amd7930_set_output_rate,
	amd7930_get_output_rate,
	amd7930_set_input_rate,
	amd7930_get_input_rate,
	amd7930_sunaudio_getdev_sunos,
	amd7930_get_output_ports,
	amd7930_get_input_ports,
	NULL,                    /* amd7930_set_output_muted */
	amd7930_get_output_muted,
        NULL,                   /* amd7930_set_output_pause */
        NULL,                   /* amd7930_get_output_pause */
        NULL,                   /* amd7930_set_input_pause */
        NULL,                   /* amd7930_get_input_pause */
        NULL,                   /* amd7930_set_output_samples */
        NULL,                   /* amd7930_get_output_samples */
        NULL,                   /* amd7930_set_input_samples */
        NULL,                   /* amd7930_get_input_samples */
        NULL,                   /* amd7930_set_output_error */
        NULL,                   /* amd7930_get_output_error */
        NULL,                   /* amd7930_set_input_error */
        NULL,                   /* amd7930_get_input_error */
        amd7930_get_formats,
};

/* Attach to an amd7930 chip given its PROM node. */
static int amd7930_attach(struct sparcaudio_driver *drv, int node,
			  struct sbus_bus *sbus, struct sbus_dev *sdev)
{
	struct linux_prom_registers regs;
	struct linux_prom_irqs irq;
	struct resource res, *resp;
	struct amd7930_info *info;
	int err;

	/* Allocate our private information structure. */
	drv->private = kmalloc(sizeof(struct amd7930_info), GFP_KERNEL);
	if (drv->private == NULL)
		return -ENOMEM;

	/* Point at the information structure and initialize it. */
	drv->ops = &amd7930_ops;
	info = (struct amd7930_info *)drv->private;
	memset(info, 0, sizeof(*info));
	info->ints_on = 1; /* force disable below */

	drv->dev = sdev;

	/* Map the registers into memory. */
	prom_getproperty(node, "reg", (char *)&regs, sizeof(regs));
	if (sbus && sdev) {
		resp = &sdev->resource[0];
	} else {
		resp = &res;
		res.start = regs.phys_addr;
		res.end = res.start + regs.reg_size - 1;
		res.flags = IORESOURCE_IO | (regs.which_io & 0xff);
	}
	info->regs_size = regs.reg_size;
	info->regs = sbus_ioremap(resp, 0, regs.reg_size, "amd7930");
	if (!info->regs) {
		printk(KERN_ERR "amd7930: could not remap registers\n");
		kfree(drv->private);
		return -EIO;
	}

	/* Put amd7930 in idle mode (interrupts disabled) */
	amd7930_idle(info);

	/* Enable extended FIFO operation on D-channel */
	sbus_writeb(AMR_DLC_EFCR, info->regs + CR);
	sbus_writeb(AMR_DLC_EFCR_EXTEND_FIFO, info->regs + DR);
	sbus_writeb(AMR_DLC_DMR4, info->regs + CR);
	sbus_writeb(/* AMR_DLC_DMR4_RCV_30 | */ AMR_DLC_DMR4_XMT_14,
		    info->regs + DR);

	/* Attach the interrupt handler to the audio interrupt. */
	prom_getproperty(node, "intr", (char *)&irq, sizeof(irq));
	info->irq = irq.pri;
	request_irq(info->irq, amd7930_interrupt,
		    SA_INTERRUPT, "amd7930", drv);
	amd7930_enable_ints(info);

	/* Initalize the local copy of the MAP registers. */
	memset(&info->map, 0, sizeof(info->map));
	info->map.mmr1 = AM_MAP_MMR1_GX | AM_MAP_MMR1_GER |
			 AM_MAP_MMR1_GR | AM_MAP_MMR1_STG;
        /* Start out with speaker, microphone */
        info->map.mmr2 |= (AM_MAP_MMR2_LS | AM_MAP_MMR2_AINB);

	/* Set the default audio parameters. */
	info->rgain = 128;
	info->pgain = 200;
	info->mgain = 0;
	info->format_type = AUDIO_ENCODING_ULAW;
	info->Bb.input_format = AUDIO_ENCODING_ULAW;
	info->Bb.output_format = AUDIO_ENCODING_ULAW;
	info->Bc.input_format = AUDIO_ENCODING_ULAW;
	info->Bc.output_format = AUDIO_ENCODING_ULAW;
	amd7930_update_map(drv);

	/* Register the amd7930 with the midlevel audio driver. */
	err = register_sparcaudio_driver(drv, 1);
	if (err < 0) {
		printk(KERN_ERR "amd7930: unable to register\n");
		free_irq(info->irq, drv);
		sbus_iounmap(info->regs, info->regs_size);
		kfree(drv->private);
		return -EIO;
	}

	/* Announce the hardware to the user. */
	printk(KERN_INFO "amd7930 at %lx irq %d\n",
	       info->regs, info->irq);

	/* Success! */
	return 0;
}

/* Detach from an amd7930 chip given the device structure. */
static void __exit amd7930_detach(struct sparcaudio_driver *drv)
{
	struct amd7930_info *info = (struct amd7930_info *)drv->private;

	unregister_sparcaudio_driver(drv, 1);
	amd7930_idle(info);
	free_irq(info->irq, drv);
	sbus_iounmap(info->regs, info->regs_size);
	kfree(drv->private);
}

/* Probe for the amd7930 chip and then attach the driver. */
static int __init amd7930_init(void)
{
	struct sbus_bus *sbus;
	struct sbus_dev *sdev;
	int node;

	/* Try to find the sun4c "audio" node first. */
	node = prom_getchild(prom_root_node);
	node = prom_searchsiblings(node, "audio");
	if (node && amd7930_attach(&drivers[0], node, NULL, NULL) == 0)
		num_drivers = 1;
	else
		num_drivers = 0;

	/* Probe each SBUS for amd7930 chips. */
	for_all_sbusdev(sdev, sbus) {
		if (!strcmp(sdev->prom_name, "audio")) {
			/* Don't go over the max number of drivers. */
			if (num_drivers >= MAX_DRIVERS)
				continue;

			if (amd7930_attach(&drivers[num_drivers],
					   sdev->prom_node, sdev->bus, sdev) == 0)
				num_drivers++;
		}
	}

	/* Only return success if we found some amd7930 chips. */
	return (num_drivers > 0) ? 0 : -EIO;
}

static void __exit amd7930_exit(void)
{
	register int i;

	for (i = 0; i < num_drivers; i++) {
		amd7930_detach(&drivers[i]);
		num_drivers--;
	}
}

module_init(amd7930_init);
module_exit(amd7930_exit);
MODULE_LICENSE("GPL");

/*************************************************************/
/*                 Audio format conversion                   */
/*************************************************************/

/* Translation tables */

static unsigned char ulaw[] = {
    3,   7,  11,  15,  19,  23,  27,  31, 
   35,  39,  43,  47,  51,  55,  59,  63, 
   66,  68,  70,  72,  74,  76,  78,  80, 
   82,  84,  86,  88,  90,  92,  94,  96, 
   98,  99, 100, 101, 102, 103, 104, 105, 
  106, 107, 108, 109, 110, 111, 112, 113, 
  113, 114, 114, 115, 115, 116, 116, 117, 
  117, 118, 118, 119, 119, 120, 120, 121, 
  121, 121, 122, 122, 122, 122, 123, 123, 
  123, 123, 124, 124, 124, 124, 125, 125, 
  125, 125, 125, 125, 126, 126, 126, 126, 
  126, 126, 126, 126, 127, 127, 127, 127, 
  127, 127, 127, 127, 127, 127, 127, 127, 
  128, 128, 128, 128, 128, 128, 128, 128, 
  128, 128, 128, 128, 128, 128, 128, 128, 
  128, 128, 128, 128, 128, 128, 128, 128, 
  253, 249, 245, 241, 237, 233, 229, 225, 
  221, 217, 213, 209, 205, 201, 197, 193, 
  190, 188, 186, 184, 182, 180, 178, 176, 
  174, 172, 170, 168, 166, 164, 162, 160, 
  158, 157, 156, 155, 154, 153, 152, 151, 
  150, 149, 148, 147, 146, 145, 144, 143, 
  143, 142, 142, 141, 141, 140, 140, 139, 
  139, 138, 138, 137, 137, 136, 136, 135, 
  135, 135, 134, 134, 134, 134, 133, 133, 
  133, 133, 132, 132, 132, 132, 131, 131, 
  131, 131, 131, 131, 130, 130, 130, 130, 
  130, 130, 130, 130, 129, 129, 129, 129, 
  129, 129, 129, 129, 129, 129, 129, 129, 
  128, 128, 128, 128, 128, 128, 128, 128, 
  128, 128, 128, 128, 128, 128, 128, 128, 
  128, 128, 128, 128, 128, 128, 128, 128
};

static __u8 mulaw2bilinear(__u8 data)
{
	return ulaw[data];
}

static unsigned char linear[] = {
     0,    0,    0,    0,    0,    0,    0,    1,
     0,    0,    0,    2,    0,    0,    0,    3,
     0,    0,    0,    4,    0,    0,    0,    5,
     0,    0,    0,    6,    0,    0,    0,    7,
     0,    0,    0,    8,    0,    0,    0,    9,
     0,    0,    0,   10,    0,    0,    0,   11,
     0,    0,    0,   12,    0,    0,    0,   13,
     0,    0,    0,   14,    0,    0,    0,   15,
     0,    0,   16,    0,   17,    0,   18,    0,
    19,    0,   20,    0,   21,    0,   22,    0,
    23,    0,   24,    0,   25,    0,   26,    0,
    27,    0,   28,    0,   29,    0,   30,    0,
    31,    0,   32,   33,   34,   35,   36,   37,
    38,   39,   40,   41,   42,   43,   44,   45,
    46,   48,   50,   52,   54,   56,   58,   60,
    62,   65,   69,   73,   77,   83,   91,  103,
   255,  231,  219,  211,  205,  201,  197,  193,
   190,  188,  186,  184,  182,  180,  178,  176,
   174,  173,  172,  171,  170,  169,  168,  167,
   166,  165,  164,  163,  162,  161,  160,    0,
   159,    0,  158,    0,  157,    0,  156,    0,
   155,    0,  154,    0,  153,    0,  152,    0,
   151,    0,  150,    0,  149,    0,  148,    0,
   147,    0,  146,    0,  145,    0,  144,    0,
     0,  143,    0,    0,    0,  142,    0,    0,
     0,  141,    0,    0,    0,  140,    0,    0,
     0,  139,    0,    0,    0,  138,    0,    0,
     0,  137,    0,    0,    0,  136,    0,    0,
     0,  135,    0,    0,    0,  134,    0,    0,
     0,  133,    0,    0,    0,  132,    0,    0,
     0,  131,    0,    0,    0,  130,    0,    0,
     0,  129,    0,    0,    0,  128,    0,    0
};

static __u8 bilinear2mulaw(__u8 data)
{
	return linear[data];
}

static int exp_lut[256] = {
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

#define BIAS 0x84
#define CLIP 32635

#define SWAP_ENDIAN(x) ((x >> 8) | ((x & 0xff) << 8))

static __u8  linear2mulaw(__u16 data)
{
	static int sign, exponent, mantissa;

	/* not really sure, if swapping is ok - comment next line to disable it */
	data = SWAP_ENDIAN(data);
	
	sign = (data >> 8) & 0x80;
	if (sign != 0) data = -data;

	if (data > CLIP) data = CLIP;
	data += BIAS;
	exponent = exp_lut[(data >> 7) & 0xFF];
	mantissa = (data >> (exponent + 3)) & 0x0F;

	return (~(sign | (exponent << 4) | mantissa));
}

static __u16 mulaw2linear(__u8 data)
{
	/* this conversion is not working */
	return data;
}

#if 0
#define INOUT(x,y) (((x) << 16) | (y))
static int convert_audio(int in_format, int out_format, __u8* buffer, int count)
{
	static int i,sign,exponent;
	static __u16 data;

	if (in_format == out_format) return count;

	switch(INOUT(in_format, out_format)) {
	case INOUT(AUDIO_ENCODING_ULAW, AUDIO_ENCODING_LINEAR8):
		for (i = 0;i < count; i++) {
			buffer[i] = ulaw[buffer[i]];
		};
		break;
	case INOUT(AUDIO_ENCODING_ULAW, AUDIO_ENCODING_LINEAR):
		break;
	case INOUT(AUDIO_ENCODING_LINEAR, AUDIO_ENCODING_ULAW):
		/* buffer is two-byte => convert to first */
		for (i = 0; i < count/2; i++) {
			data = ((__u16*)buffer)[i];
			sign = (data >> 8) & 0x80;
			if (data > CLIP) data = CLIP;
			data += BIAS;
			exponent = exp_lut[(data >> 7) & 0xFF];
			buffer[i] = ~(sign | (exponent << 4) | 
						  ((data >> (exponent + 3)) & 0x0F));
		};
		break;
	case INOUT(AUDIO_ENCODING_LINEAR8, AUDIO_ENCODING_ULAW):
		for (i = 0; i < count; i++) {
			buffer[i] = linear[buffer[i]];
		};
		break;
	default:
		return 0;
	};

	return count;
}
#undef INOUT
#endif

#undef BIAS
#undef CLIP
#undef SWAP_ENDIAN
