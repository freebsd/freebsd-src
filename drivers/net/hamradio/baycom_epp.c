/*****************************************************************************/

/*
 *	baycom_epp.c  -- baycom epp radio modem driver.
 *
 *	Copyright (C) 1998-2000
 *          Thomas Sailer (sailer@ife.ee.ethz.ch)
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
 *
 *  History:
 *   0.1  xx.xx.1998  Initial version by Matthias Welwarsky (dg2fef)
 *   0.2  21.04.1998  Massive rework by Thomas Sailer
 *                    Integrated FPGA EPP modem configuration routines
 *   0.3  11.05.1998  Took FPGA config out and moved it into a separate program
 *   0.4  26.07.1999  Adapted to new lowlevel parport driver interface
 *   0.5  03.08.1999  adapt to Linus' new __setup/__initcall
 *                    removed some pre-2.2 kernel compatibility cruft
 *   0.6  10.08.1999  Check if parport can do SPP and is safe to access during interrupt contexts
 *   0.7  12.02.2000  adapted to softnet driver interface
 *
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <linux/fs.h>
#include <linux/parport.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <linux/if_arp.h>
#include <linux/kmod.h>
#include <linux/hdlcdrv.h>
#include <linux/baycom.h>
#include <linux/soundmodem.h>
#if defined(CONFIG_AX25) || defined(CONFIG_AX25_MODULE)
/* prototypes for ax25_encapsulate and ax25_rebuild_header */
#include <net/ax25.h> 
#endif /* CONFIG_AX25 || CONFIG_AX25_MODULE */

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

/* --------------------------------------------------------------------- */

#define BAYCOM_DEBUG
#define BAYCOM_MAGIC 19730510

/* --------------------------------------------------------------------- */

static const char paranoia_str[] = KERN_ERR 
"baycom_epp: bad magic number for hdlcdrv_state struct in routine %s\n";

#define baycom_paranoia_check(dev,routine,retval)                                              \
({                                                                                             \
	if (!dev || !dev->priv || ((struct baycom_state *)dev->priv)->magic != BAYCOM_MAGIC) { \
		printk(paranoia_str, routine);                                                 \
		return retval;                                                                 \
	}                                                                                      \
})

#define baycom_paranoia_check_void(dev,routine)                                                \
({                                                                                             \
	if (!dev || !dev->priv || ((struct baycom_state *)dev->priv)->magic != BAYCOM_MAGIC) { \
		printk(paranoia_str, routine);                                                 \
		return;                                                                        \
	}                                                                                      \
})

/* --------------------------------------------------------------------- */

static const char bc_drvname[] = "baycom_epp";
static const char bc_drvinfo[] = KERN_INFO "baycom_epp: (C) 1998-2000 Thomas Sailer, HB9JNX/AE4WA\n"
KERN_INFO "baycom_epp: version 0.7 compiled " __TIME__ " " __DATE__ "\n";

/* --------------------------------------------------------------------- */

#define NR_PORTS 4

static struct net_device baycom_device[NR_PORTS];

/* --------------------------------------------------------------------- */

/* EPP status register */
#define EPP_DCDBIT      0x80
#define EPP_PTTBIT      0x08
#define EPP_NREF        0x01
#define EPP_NRAEF       0x02
#define EPP_NRHF        0x04
#define EPP_NTHF        0x20
#define EPP_NTAEF       0x10
#define EPP_NTEF        EPP_PTTBIT

/* EPP control register */
#define EPP_TX_FIFO_ENABLE 0x10
#define EPP_RX_FIFO_ENABLE 0x08
#define EPP_MODEM_ENABLE   0x20
#define EPP_LEDS           0xC0
#define EPP_IRQ_ENABLE     0x10

/* LPT registers */
#define LPTREG_ECONTROL       0x402
#define LPTREG_CONFIGB        0x401
#define LPTREG_CONFIGA        0x400
#define LPTREG_EPPDATA        0x004
#define LPTREG_EPPADDR        0x003
#define LPTREG_CONTROL        0x002
#define LPTREG_STATUS         0x001
#define LPTREG_DATA           0x000

/* LPT control register */
#define LPTCTRL_PROGRAM       0x04   /* 0 to reprogram */
#define LPTCTRL_WRITE         0x01
#define LPTCTRL_ADDRSTB       0x08
#define LPTCTRL_DATASTB       0x02
#define LPTCTRL_INTEN         0x10

/* LPT status register */
#define LPTSTAT_SHIFT_NINTR   6
#define LPTSTAT_WAIT          0x80
#define LPTSTAT_NINTR         (1<<LPTSTAT_SHIFT_NINTR)
#define LPTSTAT_PE            0x20
#define LPTSTAT_DONE          0x10
#define LPTSTAT_NERROR        0x08
#define LPTSTAT_EPPTIMEOUT    0x01

/* LPT data register */
#define LPTDATA_SHIFT_TDI     0
#define LPTDATA_SHIFT_TMS     2
#define LPTDATA_TDI           (1<<LPTDATA_SHIFT_TDI)
#define LPTDATA_TCK           0x02
#define LPTDATA_TMS           (1<<LPTDATA_SHIFT_TMS)
#define LPTDATA_INITBIAS      0x80


/* EPP modem config/status bits */
#define EPP_DCDBIT            0x80
#define EPP_PTTBIT            0x08
#define EPP_RXEBIT            0x01
#define EPP_RXAEBIT           0x02
#define EPP_RXHFULL           0x04

#define EPP_NTHF              0x20
#define EPP_NTAEF             0x10
#define EPP_NTEF              EPP_PTTBIT

#define EPP_TX_FIFO_ENABLE    0x10
#define EPP_RX_FIFO_ENABLE    0x08
#define EPP_MODEM_ENABLE      0x20
#define EPP_LEDS              0xC0
#define EPP_IRQ_ENABLE        0x10

/* Xilinx 4k JTAG instructions */
#define XC4K_IRLENGTH   3
#define XC4K_EXTEST     0
#define XC4K_PRELOAD    1
#define XC4K_CONFIGURE  5
#define XC4K_BYPASS     7

#define EPP_CONVENTIONAL  0
#define EPP_FPGA          1
#define EPP_FPGAEXTSTATUS 2

#define TXBUFFER_SIZE     ((HDLCDRV_MAXFLEN*6/5)+8)

/* ---------------------------------------------------------------------- */
/*
 * Information that need to be kept for each board.
 */

struct baycom_state {
	int magic;

        struct pardevice *pdev;
	unsigned int bh_running;
	struct tq_struct run_bh;
	unsigned int modem;
	unsigned int bitrate;
	unsigned char stat;

	struct {
		unsigned int intclk;
		unsigned int fclk;
		unsigned int bps;
		unsigned int extmodem;
		unsigned int loopback;
	} cfg;

        struct hdlcdrv_channel_params ch_params;

        struct {
		unsigned int bitbuf, bitstream, numbits, state;
		unsigned char *bufptr;
		int bufcnt;
		unsigned char buf[TXBUFFER_SIZE];
        } hdlcrx;

        struct {
		int calibrate;
                int slotcnt;
		int flags;
		enum { tx_idle = 0, tx_keyup, tx_data, tx_tail } state;
		unsigned char *bufptr;
		int bufcnt;
		unsigned char buf[TXBUFFER_SIZE];
        } hdlctx;

        struct net_device_stats stats;
	unsigned int ptt_keyed;
	struct sk_buff *skb;  /* next transmit packet  */

#ifdef BAYCOM_DEBUG
	struct debug_vals {
		unsigned long last_jiffies;
		unsigned cur_intcnt;
		unsigned last_intcnt;
		int cur_pllcorr;
		int last_pllcorr;
		unsigned int mod_cycles;
		unsigned int demod_cycles;
	} debug_vals;
#endif /* BAYCOM_DEBUG */
};

/* --------------------------------------------------------------------- */

#define KISS_VERBOSE

/* --------------------------------------------------------------------- */

#define PARAM_TXDELAY   1
#define PARAM_PERSIST   2
#define PARAM_SLOTTIME  3
#define PARAM_TXTAIL    4
#define PARAM_FULLDUP   5
#define PARAM_HARDWARE  6
#define PARAM_RETURN    255

/* --------------------------------------------------------------------- */
/*
 * the CRC routines are stolen from WAMPES
 * by Dieter Deyke
 */

static const unsigned short crc_ccitt_table[] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*---------------------------------------------------------------------------*/

#if 0
static inline void append_crc_ccitt(unsigned char *buffer, int len)
{
 	unsigned int crc = 0xffff;

	for (;len>0;len--)
		crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ *buffer++) & 0xff];
	crc ^= 0xffff;
	*buffer++ = crc;
	*buffer++ = crc >> 8;
}
#endif

/*---------------------------------------------------------------------------*/

static inline int check_crc_ccitt(const unsigned char *buf, int cnt)
{
	unsigned int crc = 0xffff;

	for (; cnt > 0; cnt--)
		crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ *buf++) & 0xff];
	return (crc & 0xffff) == 0xf0b8;
}

/*---------------------------------------------------------------------------*/

static inline int calc_crc_ccitt(const unsigned char *buf, int cnt)
{
	unsigned int crc = 0xffff;

	for (; cnt > 0; cnt--)
		crc = (crc >> 8) ^ crc_ccitt_table[(crc ^ *buf++) & 0xff];
	crc ^= 0xffff;
	return (crc & 0xffff);
}

/* ---------------------------------------------------------------------- */

#define tenms_to_flags(bc,tenms) ((tenms * bc->bitrate) / 800)

/* --------------------------------------------------------------------- */

static void inline baycom_int_freq(struct baycom_state *bc)
{
#ifdef BAYCOM_DEBUG
	unsigned long cur_jiffies = jiffies;
	/*
	 * measure the interrupt frequency
	 */
	bc->debug_vals.cur_intcnt++;
	if ((cur_jiffies - bc->debug_vals.last_jiffies) >= HZ) {
		bc->debug_vals.last_jiffies = cur_jiffies;
		bc->debug_vals.last_intcnt = bc->debug_vals.cur_intcnt;
		bc->debug_vals.cur_intcnt = 0;
		bc->debug_vals.last_pllcorr = bc->debug_vals.cur_pllcorr;
		bc->debug_vals.cur_pllcorr = 0;
	}
#endif /* BAYCOM_DEBUG */
}

/* ---------------------------------------------------------------------- */
/*
 *    eppconfig_path should be setable  via /proc/sys.
 */

static char eppconfig_path[256] = "/usr/sbin/eppfpga";

static char *envp[] = { "HOME=/", "TERM=linux", "PATH=/usr/bin:/bin", NULL };

static int errno;

static int exec_eppfpga(void *b)
{
	struct baycom_state *bc = (struct baycom_state *)b;
	char modearg[256];
	char portarg[16];
        char *argv[] = { eppconfig_path, "-s", "-p", portarg, "-m", modearg, NULL};
        int i;

	/* set up arguments */
	sprintf(modearg, "%sclk,%smodem,fclk=%d,bps=%d,divider=%d%s,extstat",
		bc->cfg.intclk ? "int" : "ext",
		bc->cfg.extmodem ? "ext" : "int", bc->cfg.fclk, bc->cfg.bps,
		(bc->cfg.fclk + 8 * bc->cfg.bps) / (16 * bc->cfg.bps),
		bc->cfg.loopback ? ",loopback" : "");
	sprintf(portarg, "%ld", bc->pdev->port->base);
	printk(KERN_DEBUG "%s: %s -s -p %s -m %s\n", bc_drvname, eppconfig_path, portarg, modearg);

	i = exec_usermodehelper(eppconfig_path, argv, envp);
	if (i < 0) {
                printk(KERN_ERR "%s: failed to exec %s -s -p %s -m %s, errno = %d\n",
                       bc_drvname, eppconfig_path, portarg, modearg, i);
                return i;
        }
        return 0;
}


/* eppconfig: called during ifconfig up to configure the modem */

static int eppconfig(struct baycom_state *bc)
{
        int i, pid, r;
	mm_segment_t fs;

        pid = kernel_thread(exec_eppfpga, bc, CLONE_FS);
        if (pid < 0) {
                printk(KERN_ERR "%s: fork failed, errno %d\n", bc_drvname, -pid);
                return pid;
        }
	fs = get_fs();
        set_fs(KERNEL_DS);      /* Allow i to be in kernel space. */
	r = waitpid(pid, &i, __WCLONE);
	set_fs(fs);
        if (r != pid) {
                printk(KERN_ERR "%s: waitpid(%d) failed, returning %d\n",
		       bc_drvname, pid, r);
		return -1;
        }
	printk(KERN_DEBUG "%s: eppfpga returned %d\n", bc_drvname, i);
	return i;
}

/* ---------------------------------------------------------------------- */

static void epp_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
}

/* ---------------------------------------------------------------------- */

static void inline do_kiss_params(struct baycom_state *bc,
				  unsigned char *data, unsigned long len)
{

#ifdef KISS_VERBOSE
#define PKP(a,b) printk(KERN_INFO "baycomm_epp: channel params: " a "\n", b)
#else /* KISS_VERBOSE */	      
#define PKP(a,b) 
#endif /* KISS_VERBOSE */	      

	if (len < 2)
		return;
	switch(data[0]) {
	case PARAM_TXDELAY:
		bc->ch_params.tx_delay = data[1];
		PKP("TX delay = %ums", 10 * bc->ch_params.tx_delay);
		break;
	case PARAM_PERSIST:   
		bc->ch_params.ppersist = data[1];
		PKP("p persistence = %u", bc->ch_params.ppersist);
		break;
	case PARAM_SLOTTIME:  
		bc->ch_params.slottime = data[1];
		PKP("slot time = %ums", bc->ch_params.slottime);
		break;
	case PARAM_TXTAIL:    
		bc->ch_params.tx_tail = data[1];
		PKP("TX tail = %ums", bc->ch_params.tx_tail);
		break;
	case PARAM_FULLDUP:   
		bc->ch_params.fulldup = !!data[1];
		PKP("%s duplex", bc->ch_params.fulldup ? "full" : "half");
		break;
	default:
		break;
	}
#undef PKP
}

/* --------------------------------------------------------------------- */
/*
 * high performance HDLC encoder
 * yes, it's ugly, but generates pretty good code
 */

#define ENCODEITERA(j)                         \
({                                             \
        if (!(notbitstream & (0x1f0 << j)))    \
                goto stuff##j;                 \
  encodeend##j: ;                              \
})

#define ENCODEITERB(j)                                          \
({                                                              \
  stuff##j:                                                     \
        bitstream &= ~(0x100 << j);                             \
        bitbuf = (bitbuf & (((2 << j) << numbit) - 1)) |        \
                ((bitbuf & ~(((2 << j) << numbit) - 1)) << 1);  \
        numbit++;                                               \
        notbitstream = ~bitstream;                              \
        goto encodeend##j;                                      \
})


static void encode_hdlc(struct baycom_state *bc)
{
	struct sk_buff *skb;
	unsigned char *wp, *bp;
	int pkt_len;
        unsigned bitstream, notbitstream, bitbuf, numbit, crc;
	unsigned char crcarr[2];
	
	if (bc->hdlctx.bufcnt > 0)
		return;
	skb = bc->skb;
	if (!skb)
		return;
	bc->skb = NULL;
	pkt_len = skb->len-1; /* strip KISS byte */
	wp = bc->hdlctx.buf;
	bp = skb->data+1;
	crc = calc_crc_ccitt(bp, pkt_len);
	crcarr[0] = crc;
	crcarr[1] = crc >> 8;
	*wp++ = 0x7e;
	bitstream = bitbuf = numbit = 0;
	while (pkt_len > -2) {
		bitstream >>= 8;
		bitstream |= ((unsigned int)*bp) << 8;
		bitbuf |= ((unsigned int)*bp) << numbit;
		notbitstream = ~bitstream;
		bp++;
		pkt_len--;
		if (!pkt_len)
			bp = crcarr;
		ENCODEITERA(0);
		ENCODEITERA(1);
		ENCODEITERA(2);
		ENCODEITERA(3);
		ENCODEITERA(4);
		ENCODEITERA(5);
		ENCODEITERA(6);
		ENCODEITERA(7);
		goto enditer;
		ENCODEITERB(0);
		ENCODEITERB(1);
		ENCODEITERB(2);
		ENCODEITERB(3);
		ENCODEITERB(4);
		ENCODEITERB(5);
		ENCODEITERB(6);
		ENCODEITERB(7);
	enditer:
		numbit += 8;
		while (numbit >= 8) {
			*wp++ = bitbuf;
			bitbuf >>= 8;
			numbit -= 8;
		}
	}
	bitbuf |= 0x7e7e << numbit;
	numbit += 16;
	while (numbit >= 8) {
		*wp++ = bitbuf;
		bitbuf >>= 8;
		numbit -= 8;
	}
	bc->hdlctx.bufptr = bc->hdlctx.buf;
	bc->hdlctx.bufcnt = wp - bc->hdlctx.buf;
	dev_kfree_skb(skb);
	bc->stats.tx_packets++;
}

/* ---------------------------------------------------------------------- */

static unsigned short random_seed;

static inline unsigned short random_num(void)
{
	random_seed = 28629 * random_seed + 157;
	return random_seed;
}

/* ---------------------------------------------------------------------- */

static int transmit(struct baycom_state *bc, int cnt, unsigned char stat)
{
	struct parport *pp = bc->pdev->port;
	unsigned char tmp[128];
	int i, j;

	if (bc->hdlctx.state == tx_tail && !(stat & EPP_PTTBIT))
		bc->hdlctx.state = tx_idle;
	if (bc->hdlctx.state == tx_idle && bc->hdlctx.calibrate <= 0) {
		if (bc->hdlctx.bufcnt <= 0)
			encode_hdlc(bc);
		if (bc->hdlctx.bufcnt <= 0)
			return 0;
		if (!bc->ch_params.fulldup) {
			if (!(stat & EPP_DCDBIT)) {
				bc->hdlctx.slotcnt = bc->ch_params.slottime;
				return 0;
			}
			if ((--bc->hdlctx.slotcnt) > 0)
				return 0;
			bc->hdlctx.slotcnt = bc->ch_params.slottime;
			if ((random_num() % 256) > bc->ch_params.ppersist)
				return 0;
		}
	}
	if (bc->hdlctx.state == tx_idle && bc->hdlctx.bufcnt > 0) {
		bc->hdlctx.state = tx_keyup;
		bc->hdlctx.flags = tenms_to_flags(bc, bc->ch_params.tx_delay);
		bc->ptt_keyed++;
	}
	while (cnt > 0) {
		switch (bc->hdlctx.state) {
		case tx_keyup:
			i = min_t(int, cnt, bc->hdlctx.flags);
			cnt -= i;
			bc->hdlctx.flags -= i;
			if (bc->hdlctx.flags <= 0)
				bc->hdlctx.state = tx_data;
			memset(tmp, 0x7e, sizeof(tmp));
			while (i > 0) {
				j = (i > sizeof(tmp)) ? sizeof(tmp) : i;
				if (j != pp->ops->epp_write_data(pp, tmp, j, 0))
					return -1;
				i -= j;
			}
			break;

		case tx_data:
			if (bc->hdlctx.bufcnt <= 0) {
				encode_hdlc(bc);
				if (bc->hdlctx.bufcnt <= 0) {
					bc->hdlctx.state = tx_tail;
					bc->hdlctx.flags = tenms_to_flags(bc, bc->ch_params.tx_tail);
					break;
				}
			}
			i = min_t(int, cnt, bc->hdlctx.bufcnt);
			bc->hdlctx.bufcnt -= i;
			cnt -= i;
			if (i != pp->ops->epp_write_data(pp, bc->hdlctx.bufptr, i, 0))
					return -1;
			bc->hdlctx.bufptr += i;
			break;
			
		case tx_tail:
			encode_hdlc(bc);
			if (bc->hdlctx.bufcnt > 0) {
				bc->hdlctx.state = tx_data;
				break;
			}
			i = min_t(int, cnt, bc->hdlctx.flags);
			if (i) {
				cnt -= i;
				bc->hdlctx.flags -= i;
				memset(tmp, 0x7e, sizeof(tmp));
				while (i > 0) {
					j = (i > sizeof(tmp)) ? sizeof(tmp) : i;
					if (j != pp->ops->epp_write_data(pp, tmp, j, 0))
						return -1;
					i -= j;
				}
				break;
			}

		default:  /* fall through */
			if (bc->hdlctx.calibrate <= 0)
				return 0;
			i = min_t(int, cnt, bc->hdlctx.calibrate);
			cnt -= i;
			bc->hdlctx.calibrate -= i;
			memset(tmp, 0, sizeof(tmp));
			while (i > 0) {
				j = (i > sizeof(tmp)) ? sizeof(tmp) : i;
				if (j != pp->ops->epp_write_data(pp, tmp, j, 0))
					return -1;
				i -= j;
			}
			break;
		}
	}
	return 0;
}

/* ---------------------------------------------------------------------- */

static void do_rxpacket(struct net_device *dev)
{
	struct baycom_state *bc = (struct baycom_state *)dev->priv;
	struct sk_buff *skb;
	unsigned char *cp;
	unsigned pktlen;

	if (bc->hdlcrx.bufcnt < 4) 
		return;
	if (!check_crc_ccitt(bc->hdlcrx.buf, bc->hdlcrx.bufcnt)) 
		return;
	pktlen = bc->hdlcrx.bufcnt-2+1; /* KISS kludge */
	if (!(skb = dev_alloc_skb(pktlen))) {
		printk("%s: memory squeeze, dropping packet\n", dev->name);
		bc->stats.rx_dropped++;
		return;
	}
	skb->dev = dev;
	cp = skb_put(skb, pktlen);
	*cp++ = 0; /* KISS kludge */
	memcpy(cp, bc->hdlcrx.buf, pktlen - 1);
	skb->protocol = htons(ETH_P_AX25);
	skb->mac.raw = skb->data;
	netif_rx(skb);
	bc->stats.rx_packets++;
}

#define DECODEITERA(j)                                                        \
({                                                                            \
        if (!(notbitstream & (0x0fc << j)))              /* flag or abort */  \
                goto flgabrt##j;                                              \
        if ((bitstream & (0x1f8 << j)) == (0xf8 << j))   /* stuffed bit */    \
                goto stuff##j;                                                \
  enditer##j: ;                                                               \
})

#define DECODEITERB(j)                                                                 \
({                                                                                     \
  flgabrt##j:                                                                          \
        if (!(notbitstream & (0x1fc << j))) {              /* abort received */        \
                state = 0;                                                             \
                goto enditer##j;                                                       \
        }                                                                              \
        if ((bitstream & (0x1fe << j)) != (0x0fc << j))   /* flag received */          \
                goto enditer##j;                                                       \
        if (state)                                                                     \
                do_rxpacket(dev);                                                      \
        bc->hdlcrx.bufcnt = 0;                                                         \
        bc->hdlcrx.bufptr = bc->hdlcrx.buf;                                            \
        state = 1;                                                                     \
        numbits = 7-j;                                                                 \
        goto enditer##j;                                                               \
  stuff##j:                                                                            \
        numbits--;                                                                     \
        bitbuf = (bitbuf & ((~0xff) << j)) | ((bitbuf & ~((~0xff) << j)) << 1);        \
        goto enditer##j;                                                               \
})
        
static int receive(struct net_device *dev, int cnt)
{
	struct baycom_state *bc = (struct baycom_state *)dev->priv;
	struct parport *pp = bc->pdev->port;
        unsigned int bitbuf, notbitstream, bitstream, numbits, state;
	unsigned char tmp[128];
        unsigned char *cp;
	int cnt2, ret = 0;
        
        numbits = bc->hdlcrx.numbits;
	state = bc->hdlcrx.state;
	bitstream = bc->hdlcrx.bitstream;
	bitbuf = bc->hdlcrx.bitbuf;
	while (cnt > 0) {
		cnt2 = (cnt > sizeof(tmp)) ? sizeof(tmp) : cnt;
		cnt -= cnt2;
		if (cnt2 != pp->ops->epp_read_data(pp, tmp, cnt2, 0)) {
			ret = -1;
			break;
		}
		cp = tmp;
		for (; cnt2 > 0; cnt2--, cp++) {
			bitstream >>= 8;
			bitstream |= (*cp) << 8;
			bitbuf >>= 8;
			bitbuf |= (*cp) << 8;
			numbits += 8;
			notbitstream = ~bitstream;
			DECODEITERA(0);
			DECODEITERA(1);
			DECODEITERA(2);
			DECODEITERA(3);
			DECODEITERA(4);
			DECODEITERA(5);
			DECODEITERA(6);
			DECODEITERA(7);
			goto enddec;
			DECODEITERB(0);
			DECODEITERB(1);
			DECODEITERB(2);
			DECODEITERB(3);
			DECODEITERB(4);
			DECODEITERB(5);
			DECODEITERB(6);
			DECODEITERB(7);
		enddec:
			while (state && numbits >= 8) {
				if (bc->hdlcrx.bufcnt >= TXBUFFER_SIZE) {
					state = 0;
				} else {
					*(bc->hdlcrx.bufptr)++ = bitbuf >> (16-numbits);
					bc->hdlcrx.bufcnt++;
					numbits -= 8;
				}
			}
		}
	}
        bc->hdlcrx.numbits = numbits;
	bc->hdlcrx.state = state;
	bc->hdlcrx.bitstream = bitstream;
	bc->hdlcrx.bitbuf = bitbuf;
	return ret;
}

/* --------------------------------------------------------------------- */

#ifdef __i386__
#define GETTICK(x)                                                \
({                                                                \
	if (cpu_has_tsc)                                          \
		__asm__ __volatile__("rdtsc" : "=a" (x) : : "dx");\
})
#else /* __i386__ */
#define GETTICK(x)
#endif /* __i386__ */

static void epp_bh(struct net_device *dev)
{
	struct baycom_state *bc;
	struct parport *pp;
	unsigned char stat;
	unsigned char tmp[2];
	unsigned int time1 = 0, time2 = 0, time3 = 0;
	int cnt, cnt2;
	
	baycom_paranoia_check_void(dev, "epp_bh");
	bc = (struct baycom_state *)dev->priv;
	if (!bc->bh_running)
		return;
	baycom_int_freq(bc);
	pp = bc->pdev->port;
	/* update status */
	if (pp->ops->epp_read_addr(pp, &stat, 1, 0) != 1)
		goto epptimeout;
	bc->stat = stat;
	bc->debug_vals.last_pllcorr = stat;
	GETTICK(time1);
	if (bc->modem == EPP_FPGAEXTSTATUS) {
		/* get input count */
		tmp[0] = EPP_TX_FIFO_ENABLE|EPP_RX_FIFO_ENABLE|EPP_MODEM_ENABLE|1;
		if (pp->ops->epp_write_addr(pp, tmp, 1, 0) != 1)
			goto epptimeout;
		if (pp->ops->epp_read_addr(pp, tmp, 2, 0) != 2)
			goto epptimeout;
		cnt = tmp[0] | (tmp[1] << 8);
		cnt &= 0x7fff;
		/* get output count */
		tmp[0] = EPP_TX_FIFO_ENABLE|EPP_RX_FIFO_ENABLE|EPP_MODEM_ENABLE|2;
		if (pp->ops->epp_write_addr(pp, tmp, 1, 0) != 1)
			goto epptimeout;
		if (pp->ops->epp_read_addr(pp, tmp, 2, 0) != 2)
			goto epptimeout;
		cnt2 = tmp[0] | (tmp[1] << 8);
		cnt2 = 16384 - (cnt2 & 0x7fff);
		/* return to normal */
		tmp[0] = EPP_TX_FIFO_ENABLE|EPP_RX_FIFO_ENABLE|EPP_MODEM_ENABLE;
		if (pp->ops->epp_write_addr(pp, tmp, 1, 0) != 1)
			goto epptimeout;
		if (transmit(bc, cnt2, stat))
			goto epptimeout;
		GETTICK(time2);
		if (receive(dev, cnt))
			goto epptimeout;
		if (pp->ops->epp_read_addr(pp, &stat, 1, 0) != 1)
			goto epptimeout;
		bc->stat = stat;
	} else {
		/* try to tx */
		switch (stat & (EPP_NTAEF|EPP_NTHF)) {
		case EPP_NTHF:
			cnt = 2048 - 256;
			break;
		
		case EPP_NTAEF:
			cnt = 2048 - 1793;
			break;
		
		case 0:
			cnt = 0;
			break;
		
		default:
			cnt = 2048 - 1025;
			break;
		}
		if (transmit(bc, cnt, stat))
			goto epptimeout;
		GETTICK(time2);
		/* do receiver */
		while ((stat & (EPP_NRAEF|EPP_NRHF)) != EPP_NRHF) {
			switch (stat & (EPP_NRAEF|EPP_NRHF)) {
			case EPP_NRAEF:
				cnt = 1025;
				break;

			case 0:
				cnt = 1793;
				break;

			default:
				cnt = 256;
				break;
			}
			if (receive(dev, cnt))
				goto epptimeout;
			if (pp->ops->epp_read_addr(pp, &stat, 1, 0) != 1)
				goto epptimeout;
		}
		cnt = 0;
		if (bc->bitrate < 50000)
			cnt = 256;
		else if (bc->bitrate < 100000)
			cnt = 128;
		while (cnt > 0 && stat & EPP_NREF) {
			if (receive(dev, 1))
				goto epptimeout;
			cnt--;
			if (pp->ops->epp_read_addr(pp, &stat, 1, 0) != 1)
				goto epptimeout;
		}
	}
	GETTICK(time3);
#ifdef BAYCOM_DEBUG
	bc->debug_vals.mod_cycles = time2 - time1;
	bc->debug_vals.demod_cycles = time3 - time2;
#endif /* BAYCOM_DEBUG */
	queue_task(&bc->run_bh, &tq_timer);
	if (!bc->skb)
		netif_wake_queue(dev);
	return;
 epptimeout:
	printk(KERN_ERR "%s: EPP timeout!\n", bc_drvname);
}

/* ---------------------------------------------------------------------- */
/*
 * ===================== network driver interface =========================
 */

static int baycom_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct baycom_state *bc;

	baycom_paranoia_check(dev, "baycom_send_packet", 0);
	bc = (struct baycom_state *)dev->priv;
	if (skb->data[0] != 0) {
		do_kiss_params(bc, skb->data, skb->len);
		dev_kfree_skb(skb);
		return 0;
	}
	if (bc->skb)
		return -1;
	/* strip KISS byte */
	if (skb->len >= HDLCDRV_MAXFLEN+1 || skb->len < 3) {
		dev_kfree_skb(skb);
		return 0;
	}
	netif_stop_queue(dev);
	bc->skb = skb;
	return 0;
}

/* --------------------------------------------------------------------- */

static int baycom_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *sa = (struct sockaddr *)addr;

	/* addr is an AX.25 shifted ASCII mac address */
	memcpy(dev->dev_addr, sa->sa_data, dev->addr_len); 
	return 0;                                         
}

/* --------------------------------------------------------------------- */

static struct net_device_stats *baycom_get_stats(struct net_device *dev)
{
	struct baycom_state *bc;

	baycom_paranoia_check(dev, "baycom_get_stats", NULL);
	bc = (struct baycom_state *)dev->priv;
	/* 
	 * Get the current statistics.  This may be called with the
	 * card open or closed. 
	 */
	return &bc->stats;
}

/* --------------------------------------------------------------------- */

static void epp_wakeup(void *handle)
{
        struct net_device *dev = (struct net_device *)handle;
        struct baycom_state *bc;

	baycom_paranoia_check_void(dev, "epp_wakeup");
        bc = (struct baycom_state *)dev->priv;
        printk(KERN_DEBUG "baycom_epp: %s: why am I being woken up?\n", dev->name);
        if (!parport_claim(bc->pdev))
                printk(KERN_DEBUG "baycom_epp: %s: I'm broken.\n", dev->name);
}

/* --------------------------------------------------------------------- */

/*
 * Open/initialize the board. This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 */

static int epp_open(struct net_device *dev)
{
	struct baycom_state *bc;
        struct parport *pp;
	const struct tq_struct run_bh = {
		routine: (void *)(void *)epp_bh,
		data: dev
	};
	unsigned int i, j;
	unsigned char tmp[128];
	unsigned char stat;
	unsigned long tstart;
	
	baycom_paranoia_check(dev, "epp_open", -ENXIO);
	bc = (struct baycom_state *)dev->priv;
        pp = parport_enumerate();
        while (pp && pp->base != dev->base_addr) 
                pp = pp->next;
        if (!pp) {
                printk(KERN_ERR "%s: parport at 0x%lx unknown\n", bc_drvname, dev->base_addr);
                return -ENXIO;
        }
#if 0
        if (pp->irq < 0) {
                printk(KERN_ERR "%s: parport at 0x%lx has no irq\n", bc_drvname, pp->base);
                return -ENXIO;
        }
#endif
	if ((~pp->modes) & (PARPORT_MODE_TRISTATE | PARPORT_MODE_PCSPP | PARPORT_MODE_SAFEININT)) {
                printk(KERN_ERR "%s: parport at 0x%lx cannot be used\n",
		       bc_drvname, pp->base);
                return -EIO;
	}
	memset(&bc->modem, 0, sizeof(bc->modem));
        if (!(bc->pdev = parport_register_device(pp, dev->name, NULL, epp_wakeup, 
                                                 epp_interrupt, PARPORT_DEV_EXCL, dev))) {
                printk(KERN_ERR "%s: cannot register parport at 0x%lx\n", bc_drvname, pp->base);
                return -ENXIO;
        }
        if (parport_claim(bc->pdev)) {
                printk(KERN_ERR "%s: parport at 0x%lx busy\n", bc_drvname, pp->base);
                parport_unregister_device(bc->pdev);
                return -EBUSY;
        }
        dev->irq = /*pp->irq*/ 0;
	bc->run_bh = run_bh;
	bc->bh_running = 1;
	bc->modem = EPP_CONVENTIONAL;
	if (eppconfig(bc))
		printk(KERN_INFO "%s: no FPGA detected, assuming conventional EPP modem\n", bc_drvname);
	else
		bc->modem = /*EPP_FPGA*/ EPP_FPGAEXTSTATUS;
	parport_write_control(pp, LPTCTRL_PROGRAM); /* prepare EPP mode; we aren't using interrupts */
	/* reset the modem */
	tmp[0] = 0;
	tmp[1] = EPP_TX_FIFO_ENABLE|EPP_RX_FIFO_ENABLE|EPP_MODEM_ENABLE;
	if (pp->ops->epp_write_addr(pp, tmp, 2, 0) != 2)
		goto epptimeout;
	/* autoprobe baud rate */
	tstart = jiffies;
	i = 0;
	while ((signed)(jiffies-tstart-HZ/3) < 0) {
		if (pp->ops->epp_read_addr(pp, &stat, 1, 0) != 1)
			goto epptimeout;
		if ((stat & (EPP_NRAEF|EPP_NRHF)) == EPP_NRHF) {
			schedule();
			continue;
		}
		if (pp->ops->epp_read_data(pp, tmp, 128, 0) != 128)
			goto epptimeout;
		if (pp->ops->epp_read_data(pp, tmp, 128, 0) != 128)
			goto epptimeout;
		i += 256;
	}
	for (j = 0; j < 256; j++) {
		if (pp->ops->epp_read_addr(pp, &stat, 1, 0) != 1)
			goto epptimeout;
		if (!(stat & EPP_NREF))
			break;
		if (pp->ops->epp_read_data(pp, tmp, 1, 0) != 1)
			goto epptimeout;
		i++;
	}
	tstart = jiffies - tstart;
	bc->bitrate = i * (8 * HZ) / tstart;
	j = 1;
	i = bc->bitrate >> 3;
	while (j < 7 && i > 150) {
		j++;
		i >>= 1;
	}
	printk(KERN_INFO "%s: autoprobed bitrate: %d  int divider: %d  int rate: %d\n", 
	       bc_drvname, bc->bitrate, j, bc->bitrate >> (j+2));
	tmp[0] = EPP_TX_FIFO_ENABLE|EPP_RX_FIFO_ENABLE|EPP_MODEM_ENABLE/*|j*/;
	if (pp->ops->epp_write_addr(pp, tmp, 1, 0) != 1)
		goto epptimeout;
	/*
	 * initialise hdlc variables
	 */
	bc->hdlcrx.state = 0;
	bc->hdlcrx.numbits = 0;
	bc->hdlctx.state = tx_idle;
	bc->hdlctx.bufcnt = 0;
	bc->hdlctx.slotcnt = bc->ch_params.slottime;
	bc->hdlctx.calibrate = 0;
	/* start the bottom half stuff */
	queue_task(&bc->run_bh, &tq_timer);
	netif_start_queue(dev);
	MOD_INC_USE_COUNT;
	return 0;

 epptimeout:
	printk(KERN_ERR "%s: epp timeout during bitrate probe\n", bc_drvname);
	parport_write_control(pp, 0); /* reset the adapter */
        parport_release(bc->pdev);
        parport_unregister_device(bc->pdev);
	return -EIO;
}

/* --------------------------------------------------------------------- */

static int epp_close(struct net_device *dev)
{
	struct baycom_state *bc;
	struct parport *pp;
	unsigned char tmp[1];

	baycom_paranoia_check(dev, "epp_close", -EINVAL);
	bc = (struct baycom_state *)dev->priv;
	pp = bc->pdev->port;
	bc->bh_running = 0;
	run_task_queue(&tq_timer);  /* dequeue bottom half */
	bc->stat = EPP_DCDBIT;
	tmp[0] = 0;
	pp->ops->epp_write_addr(pp, tmp, 1, 0);
	parport_write_control(pp, 0); /* reset the adapter */
        parport_release(bc->pdev);
        parport_unregister_device(bc->pdev);
	if (bc->skb)
		dev_kfree_skb(bc->skb);
	bc->skb = NULL;
	printk(KERN_INFO "%s: close epp at iobase 0x%lx irq %u\n",
	       bc_drvname, dev->base_addr, dev->irq);
	MOD_DEC_USE_COUNT;
	return 0;
}

/* --------------------------------------------------------------------- */

static int baycom_setmode(struct baycom_state *bc, const char *modestr)
{
	const char *cp;

	if (strstr(modestr,"intclk"))
		bc->cfg.intclk = 1;
	if (strstr(modestr,"extclk"))
		bc->cfg.intclk = 0;
	if (strstr(modestr,"intmodem"))
		bc->cfg.extmodem = 0;
	if (strstr(modestr,"extmodem"))
		bc->cfg.extmodem = 1;
	if (strstr(modestr,"noloopback"))
		bc->cfg.loopback = 0;
	if (strstr(modestr,"loopback"))
		bc->cfg.loopback = 1;
	if ((cp = strstr(modestr,"fclk="))) {
		bc->cfg.fclk = simple_strtoul(cp+5, NULL, 0);
		if (bc->cfg.fclk < 1000000)
			bc->cfg.fclk = 1000000;
		if (bc->cfg.fclk > 25000000)
			bc->cfg.fclk = 25000000;
	}
	if ((cp = strstr(modestr,"bps="))) {
		bc->cfg.bps = simple_strtoul(cp+4, NULL, 0);
		if (bc->cfg.bps < 1000)
			bc->cfg.bps = 1000;
		if (bc->cfg.bps > 1500000)
			bc->cfg.bps = 1500000;
	}
	return 0;
}

/* --------------------------------------------------------------------- */

static int baycom_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct baycom_state *bc;
	struct baycom_ioctl bi;
	struct hdlcdrv_ioctl hi;
	struct sm_ioctl si;

	baycom_paranoia_check(dev, "baycom_ioctl", -EINVAL);
	bc = (struct baycom_state *)dev->priv;
	if (cmd != SIOCDEVPRIVATE)
		return -ENOIOCTLCMD;
	if (get_user(cmd, (int *)ifr->ifr_data))
		return -EFAULT;
#ifdef BAYCOM_DEBUG
	if (cmd == BAYCOMCTL_GETDEBUG) {
		bi.data.dbg.debug1 = bc->ptt_keyed;
		bi.data.dbg.debug2 = bc->debug_vals.last_intcnt;
		bi.data.dbg.debug3 = bc->debug_vals.last_pllcorr;
		bc->debug_vals.last_intcnt = 0;
		if (copy_to_user(ifr->ifr_data, &bi, sizeof(bi)))
			return -EFAULT;
		return 0;
	}
	if (cmd == SMCTL_GETDEBUG) {
                si.data.dbg.int_rate = bc->debug_vals.last_intcnt;
                si.data.dbg.mod_cycles = bc->debug_vals.mod_cycles;
                si.data.dbg.demod_cycles = bc->debug_vals.demod_cycles;
                si.data.dbg.dma_residue = 0;
                bc->debug_vals.mod_cycles = bc->debug_vals.demod_cycles = 0;
		bc->debug_vals.last_intcnt = 0;
                if (copy_to_user(ifr->ifr_data, &si, sizeof(si)))
                        return -EFAULT;
                return 0;
	}
#endif /* BAYCOM_DEBUG */

	if (copy_from_user(&hi, ifr->ifr_data, sizeof(hi)))
		return -EFAULT;
	switch (hi.cmd) {
	default:
		return -ENOIOCTLCMD;

	case HDLCDRVCTL_GETCHANNELPAR:
		hi.data.cp.tx_delay = bc->ch_params.tx_delay;
		hi.data.cp.tx_tail = bc->ch_params.tx_tail;
		hi.data.cp.slottime = bc->ch_params.slottime;
		hi.data.cp.ppersist = bc->ch_params.ppersist;
		hi.data.cp.fulldup = bc->ch_params.fulldup;
		break;

	case HDLCDRVCTL_SETCHANNELPAR:
		if (!capable(CAP_NET_ADMIN))
			return -EACCES;
		bc->ch_params.tx_delay = hi.data.cp.tx_delay;
		bc->ch_params.tx_tail = hi.data.cp.tx_tail;
		bc->ch_params.slottime = hi.data.cp.slottime;
		bc->ch_params.ppersist = hi.data.cp.ppersist;
		bc->ch_params.fulldup = hi.data.cp.fulldup;
		bc->hdlctx.slotcnt = 1;
		return 0;
		
	case HDLCDRVCTL_GETMODEMPAR:
		hi.data.mp.iobase = dev->base_addr;
		hi.data.mp.irq = dev->irq;
		hi.data.mp.dma = dev->dma;
		hi.data.mp.dma2 = 0;
		hi.data.mp.seriobase = 0;
		hi.data.mp.pariobase = 0;
		hi.data.mp.midiiobase = 0;
		break;

	case HDLCDRVCTL_SETMODEMPAR:
		if ((!capable(CAP_SYS_RAWIO)) || netif_running(dev))
			return -EACCES;
		dev->base_addr = hi.data.mp.iobase;
		dev->irq = /*hi.data.mp.irq*/0;
		dev->dma = /*hi.data.mp.dma*/0;
		return 0;	
		
	case HDLCDRVCTL_GETSTAT:
		hi.data.cs.ptt = !!(bc->stat & EPP_PTTBIT);
		hi.data.cs.dcd = !(bc->stat & EPP_DCDBIT);
		hi.data.cs.ptt_keyed = bc->ptt_keyed;
		hi.data.cs.tx_packets = bc->stats.tx_packets;
		hi.data.cs.tx_errors = bc->stats.tx_errors;
		hi.data.cs.rx_packets = bc->stats.rx_packets;
		hi.data.cs.rx_errors = bc->stats.rx_errors;
		break;		

	case HDLCDRVCTL_OLDGETSTAT:
		hi.data.ocs.ptt = !!(bc->stat & EPP_PTTBIT);
		hi.data.ocs.dcd = !(bc->stat & EPP_DCDBIT);
		hi.data.ocs.ptt_keyed = bc->ptt_keyed;
		break;		

	case HDLCDRVCTL_CALIBRATE:
		if (!capable(CAP_SYS_RAWIO))
			return -EACCES;
		bc->hdlctx.calibrate = hi.data.calibrate * bc->bitrate / 8;
		return 0;

	case HDLCDRVCTL_DRIVERNAME:
		strncpy(hi.data.drivername, "baycom_epp", sizeof(hi.data.drivername));
		break;
		
	case HDLCDRVCTL_GETMODE:
		sprintf(hi.data.modename, "%sclk,%smodem,fclk=%d,bps=%d%s", 
			bc->cfg.intclk ? "int" : "ext",
			bc->cfg.extmodem ? "ext" : "int", bc->cfg.fclk, bc->cfg.bps,
			bc->cfg.loopback ? ",loopback" : "");
		break;

	case HDLCDRVCTL_SETMODE:
		if (!capable(CAP_NET_ADMIN) || netif_running(dev))
			return -EACCES;
		hi.data.modename[sizeof(hi.data.modename)-1] = '\0';
		return baycom_setmode(bc, hi.data.modename);

	case HDLCDRVCTL_MODELIST:
		strncpy(hi.data.modename, "intclk,extclk,intmodem,extmodem,divider=x",
			sizeof(hi.data.modename));
		break;

	case HDLCDRVCTL_MODEMPARMASK:
		return HDLCDRV_PARMASK_IOBASE;

	}
	if (copy_to_user(ifr->ifr_data, &hi, sizeof(hi)))
		return -EFAULT;
	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Check for a network adaptor of this type, and return '0' if one exists.
 * If dev->base_addr == 0, probe all likely locations.
 * If dev->base_addr == 1, always return failure.
 * If dev->base_addr == 2, allocate space for the device and return success
 * (detachable devices only).
 */
static int baycom_probe(struct net_device *dev)
{
	static char ax25_bcast[AX25_ADDR_LEN] = {
		'Q' << 1, 'S' << 1, 'T' << 1, ' ' << 1, ' ' << 1, ' ' << 1, '0' << 1
	};
	static char ax25_nocall[AX25_ADDR_LEN] = {
		'L' << 1, 'I' << 1, 'N' << 1, 'U' << 1, 'X' << 1, ' ' << 1, '1' << 1
	};
	const struct hdlcdrv_channel_params dflt_ch_params = { 
		20, 2, 10, 40, 0 
	};
	struct baycom_state *bc;

	if (!dev)
		return -ENXIO;
	baycom_paranoia_check(dev, "baycom_probe", -ENXIO);
	/*
	 * not a real probe! only initialize data structures
	 */
	bc = (struct baycom_state *)dev->priv;
	/*
	 * initialize the baycom_state struct
	 */
	bc->ch_params = dflt_ch_params;
	bc->ptt_keyed = 0;

	/*
	 * initialize the device struct
	 */
	dev->open = epp_open;
	dev->stop = epp_close;
	dev->do_ioctl = baycom_ioctl;
	dev->hard_start_xmit = baycom_send_packet;
	dev->get_stats = baycom_get_stats;

	/* Fill in the fields of the device structure */
	bc->skb = NULL;
	
#if defined(CONFIG_AX25) || defined(CONFIG_AX25_MODULE)
	dev->hard_header = ax25_encapsulate;
	dev->rebuild_header = ax25_rebuild_header;
#else /* CONFIG_AX25 || CONFIG_AX25_MODULE */
	dev->hard_header = NULL;
	dev->rebuild_header = NULL;
#endif /* CONFIG_AX25 || CONFIG_AX25_MODULE */
	dev->set_mac_address = baycom_set_mac_address;
	
	dev->type = ARPHRD_AX25;           /* AF_AX25 device */
	dev->hard_header_len = AX25_MAX_HEADER_LEN + AX25_BPQ_HEADER_LEN;
	dev->mtu = AX25_DEF_PACLEN;        /* eth_mtu is the default */
	dev->addr_len = AX25_ADDR_LEN;     /* sizeof an ax.25 address */
	memcpy(dev->broadcast, ax25_bcast, AX25_ADDR_LEN);
	memcpy(dev->dev_addr, ax25_nocall, AX25_ADDR_LEN);
	dev->tx_queue_len = 16;

	/* New style flags */
	dev->flags = 0;

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * command line settable parameters
 */
static const char *mode[NR_PORTS] = { "", };
static int iobase[NR_PORTS] = { 0x378, };

MODULE_PARM(mode, "1-" __MODULE_STRING(NR_PORTS) "s");
MODULE_PARM_DESC(mode, "baycom operating mode");
MODULE_PARM(iobase, "1-" __MODULE_STRING(NR_PORTS) "i");
MODULE_PARM_DESC(iobase, "baycom io base address");

MODULE_AUTHOR("Thomas M. Sailer, sailer@ife.ee.ethz.ch, hb9jnx@hb9w.che.eu");
MODULE_DESCRIPTION("Baycom epp amateur radio modem driver");
MODULE_LICENSE("GPL");

/* --------------------------------------------------------------------- */

static int __init init_baycomepp(void)
{
	struct net_device *dev;
	int i, found = 0;
	char set_hw = 1;
	struct baycom_state *bc;

	printk(bc_drvinfo);
	/*
	 * register net devices
	 */
	for (i = 0; i < NR_PORTS; i++) {
		dev = baycom_device+i;
		if (!mode[i])
			set_hw = 0;
		if (!set_hw)
			iobase[i] = 0;
		memset(dev, 0, sizeof(struct net_device));
		if (!(bc = dev->priv = kmalloc(sizeof(struct baycom_state), GFP_KERNEL)))
			return -ENOMEM;
		/*
		 * initialize part of the baycom_state struct
		 */
		memset(bc, 0, sizeof(struct baycom_state));
		bc->magic = BAYCOM_MAGIC;
		sprintf(dev->name, "bce%d", i);
		bc->cfg.fclk = 19666600;
		bc->cfg.bps = 9600;
		/*
		 * initialize part of the device struct
		 */
		dev->if_port = 0;
		dev->init = baycom_probe;
		dev->base_addr = iobase[i];
		dev->irq = 0;
		dev->dma = 0;
		if (register_netdev(dev)) {
			printk(KERN_WARNING "%s: cannot register net device %s\n", bc_drvname, dev->name);
			kfree(dev->priv);
			return -ENXIO;
		}
		if (set_hw && baycom_setmode(bc, mode[i]))
			set_hw = 0;
		found++;
	}
	if (!found)
		return -ENXIO;
	return 0;
}

static void __exit cleanup_baycomepp(void)
{
	struct net_device *dev;
	struct baycom_state *bc;
	int i;

	for(i = 0; i < NR_PORTS; i++) {
		dev = baycom_device+i;
		bc = (struct baycom_state *)dev->priv;
		if (bc) {
			if (bc->magic == BAYCOM_MAGIC) {
				unregister_netdev(dev);
				kfree(dev->priv);
			} else
				printk(paranoia_str, "cleanup_module");
		}
	}
}

module_init(init_baycomepp);
module_exit(cleanup_baycomepp);

/* --------------------------------------------------------------------- */

#ifndef MODULE

/*
 * format: baycom_epp=io,mode
 * mode: fpga config options
 */

static int __init baycom_epp_setup(char *str)
{
        static unsigned __initdata nr_dev = 0;
	int ints[2];

        if (nr_dev >= NR_PORTS)
                return 0;
	str = get_options(str, 2, ints);
	if (ints[0] < 1)
		return 0;
	mode[nr_dev] = str;
	iobase[nr_dev] = ints[1];
	nr_dev++;
	return 1;
}

__setup("baycom_epp=", baycom_epp_setup);

#endif /* MODULE */
/* --------------------------------------------------------------------- */
