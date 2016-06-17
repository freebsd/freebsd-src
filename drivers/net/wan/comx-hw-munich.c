/*
 * Hardware-level driver for the SliceCOM board for Linux kernels 2.4.X
 *
 * Current maintainer / latest changes: Pasztor Szilard <don@itc.hu>
 *
 * Original author: Bartok Istvan <bartoki@itc.hu>
 * Based on skeleton by Tivadar Szemethy <tiv@itc.hu>
 *
 * 0.51:
 *      - port for 2.4.x
 *	- clean up some code, make it more portable
 *	- busted direct hardware access through mapped memory
 *	- fix a possible race
 *	- prevent procfs buffer overflow
 *
 * 0.50:
 *	- support for the pcicom board, lots of rearrangements
 *	- handle modem status lines
 *
 * 0.50a:
 *	- fix for falc version 1.0
 *
 * 0.50b: T&t
 *	- fix for bad localbus
 */

#define VERSION		"0.51"
#define VERSIONSTR	"SliceCOM v" VERSION ", 2002/01/07\n"

#include <linux/config.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <asm/delay.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/init.h>

#define COMX_NEW

#ifndef COMX_NEW
#include "../include/comx.h"
#include "../include/munich32x.h"
#include "../include/falc-lh.h"
#else
#include "comx.h"
#include "munich32x.h"
#include "falc-lh.h"
#endif

MODULE_AUTHOR("Bartok Istvan <bartoki@itc.hu>, Gergely Madarasz <gorgo@itc.hu>, Szilard Pasztor <don@itc.hu>");
MODULE_DESCRIPTION("Hardware-level driver for the SliceCOM and PciCOM (WelCOM) adapters");
MODULE_LICENSE("GPL");
/*
 *	TODO: az ilyenek a comxhw.h -ban szoktak lenni, idovel menjenek majd oda:
 */

#define FILENAME_BOARDNUM	"boardnum"	/* /proc/comx/comx0.1/boardnum          */
#define FILENAME_TIMESLOTS	"timeslots"	/* /proc/comx/comx0.1/timeslots         */
#define FILENAME_FRAMING	"framing"	/* /proc/comx/comx0.1/framing           */
#define FILENAME_LINECODE	"linecode"	/* /proc/comx/comx0.1/linecode          */
#define FILENAME_CLOCK_SOURCE	"clock_source"	/* /proc/comx/comx0.1/clock_source      */
#define FILENAME_LOOPBACK	"loopback"	/* /proc/comx/comx0.1/loopback          */
#define FILENAME_REG		"reg"		/* /proc/comx/comx0.1/reg               */
#define FILENAME_LBIREG		"lbireg"	/* /proc/comx/comx0.1/lbireg            */

#define SLICECOM_BOARDNUM_DEFAULT	0

#define SLICECOM_FRAMING_CRC4		1
#define SLICECOM_FRAMING_NO_CRC4	2
#define SLICECOM_FRAMING_DEFAULT	SLICECOM_FRAMING_CRC4

#define SLICECOM_LINECODE_HDB3		1
#define SLICECOM_LINECODE_AMI		2
#define SLICECOM_LINECODE_DEFAULT	SLICECOM_LINECODE_HDB3

#define SLICECOM_CLOCK_SOURCE_LINE	1
#define SLICECOM_CLOCK_SOURCE_INTERNAL	2
#define SLICECOM_CLOCK_SOURCE_DEFAULT	SLICECOM_CLOCK_SOURCE_LINE

#define SLICECOM_LOOPBACK_NONE		1
#define SLICECOM_LOOPBACK_LOCAL		2
#define SLICECOM_LOOPBACK_REMOTE	3
#define SLICECOM_LOOPBACK_DEFAULT	SLICECOM_LOOPBACK_NONE

#define MUNICH_VIRT(addr) (void *)(&bar1[addr])

struct slicecom_stringtable
{
    char *name;
    int value;
};

/* A convention: keep "default" the last not NULL when reading from /proc,
   "error" is an indication that something went wrong, we have an undefined value */

struct slicecom_stringtable slicecom_framings[] =
{
    {"crc4", SLICECOM_FRAMING_CRC4},
    {"no-crc4", SLICECOM_FRAMING_NO_CRC4},
    {"default", SLICECOM_FRAMING_DEFAULT},
    {"error", 0}
};

struct slicecom_stringtable slicecom_linecodes[] =
{
    {"hdb3", SLICECOM_LINECODE_HDB3},
    {"ami", SLICECOM_LINECODE_AMI},
    {"default", SLICECOM_LINECODE_DEFAULT},
    {"error", 0}
};

struct slicecom_stringtable slicecom_clock_sources[] =
{
    {"line", SLICECOM_CLOCK_SOURCE_LINE},
    {"internal", SLICECOM_CLOCK_SOURCE_INTERNAL},
    {"default", SLICECOM_CLOCK_SOURCE_DEFAULT},
    {"error", 0}
};

struct slicecom_stringtable slicecom_loopbacks[] =
{
    {"none", SLICECOM_LOOPBACK_NONE},
    {"local", SLICECOM_LOOPBACK_LOCAL},
    {"remote", SLICECOM_LOOPBACK_REMOTE},
    {"default", SLICECOM_LOOPBACK_DEFAULT},
    {"error", 0}
};

/*
 *	Some tunable values...
 *
 *	Note: when tuning values which change the length of text in
 *	/proc/comx/comx[n]/status, keep in mind that it must be shorter then
 *	PAGESIZE !
 */

#define MAX_BOARDS	4	/* ezzel 4 kartya lehet a gepben: 0..3          */
#define RX_DESC_MAX	8	/* Rx ring size, must be >= 4                   */
#define TX_DESC_MAX	4	/* Tx ring size, must be >= 2                   */
				/* a sokkal hosszabb Tx ring mar ronthatja a nem-FIFO packet    */
				/* schedulerek (fair queueing, stb.) hatekonysagat.             */
#define MAX_WORK	10	/* TOD: update the info max. ennyi-1 esemenyt dolgoz fel egy interrupt hivasnal */

/*
 *	These are tunable too, but don't touch them without fully understanding what is happening
 */

#define UDELAY		20	/* We wait UDELAY usecs with disabled interrupts before and     */
				/* after each command to avoid writing into each other's        */
				/* ccb->action_spec. A _send_packet nem var, mert azt az        */
				/* _interrupt()-bol is meghivhatja a LINE_tx()                  */

/*
 *	Just to avoid warnings about implicit declarations:
 */

static int MUNICH_close(struct net_device *dev);
static struct comx_hardware slicecomhw;
static struct comx_hardware pcicomhw;

static unsigned long flags;
static spinlock_t mister_lock = SPIN_LOCK_UNLOCKED;

typedef volatile struct		/* Time Slot Assignment */
{
    u32 rxfillmask:8,		// ----------------------------+------+
				//                             |      |
      rxchannel:5,		// ----------------------+---+ |      |
      rti:1,			// ---------------------+|   | |      |
      res2:2,			// -------------------++||   | |      |
				//                    ||||   | |      |
      txfillmask:8,		// ----------+------+ ||||   | |      |
				//           |      | ||||   | |      |
      txchannel:5,		// ----+---+ |      | ||||   | |      |
      tti:1,			// ---+|   | |      | ||||   | |      |
      res1:2;			// -++||   | |      | ||||   | |      |
				//   3          2          1
    				//  10987654 32109876 54321098 76543210
} timeslot_spec_t;

typedef volatile struct		/* Receive Descriptor */
{
    u32 zero1:16, no:13, hi:1, hold:1, zero2:1;

    u32 next;
    u32 data;

    u32 zero3:8, status:8, bno:13, zero4:1, c:1, fe:1;
} rx_desc_t;

typedef volatile struct		/* Transmit Descriptor */
{
    u32 fnum:11, csm:1, no13:1, zero1:2, v110:1, no:13, hi:1, hold:1, fe:1;

    u32 next;
    u32 data;

} tx_desc_t;

typedef volatile struct		/* Channel Specification */
{
    u32 iftf:1, mode:2, fa:1, trv:2, crc:1, inv:1, cs:1, tflag:7, ra:1, ro:1,
	th:1, ta:1, to:1, ti:1, ri:1, nitbs:1, fit:1, fir:1, re:1, te:1, ch:1,
	ifc:1, sfe:1, fe2:1;

    u32 frda;
    u32 ftda;

    u32 itbs:6, zero1:26;

} channel_spec_t;

typedef volatile struct		/* Configuration Control Block */
{
    u32 action_spec;
    u32 reserved1;
    u32 reserved2;
    timeslot_spec_t timeslot_spec[32];
    channel_spec_t channel_spec[32];
    u32 current_rx_desc[32];
    u32 current_tx_desc[32];
    u32 csa;			/* Control Start Address. CSA = *CCBA; CCB = *CSA */
				/* MUNICH does it like: CCB = *( *CCBA )          */
} munich_ccb_t;

typedef volatile struct		/* Entry in the interrupt queue */
{
    u32 all;
} munich_intq_t;

#define MUNICH_INTQLEN	63	/* Rx/Tx Interrupt Queue Length
				   (not the real len, but the TIQL/RIQL value)  */
#define MUNICH_INTQMAX	( 16*(MUNICH_INTQLEN+1) )	/* Rx/Tx/Periph Interrupt Queue size in munich_intq_t's */
#define MUNICH_INTQSIZE	( 4*MUNICH_INTQMAX )	/* Rx/Tx/Periph Interrupt Queue size in bytes           */

#define MUNICH_PIQLEN	4	/* Peripheral Interrupt Queue Length. Unlike the RIQL/TIQL, */
#define MUNICH_PIQMAX	( 4*MUNICH_PIQLEN )	/* PIQL register needs it like this                     */
#define MUNICH_PIQSIZE	( 4*MUNICH_PIQMAX )

typedef volatile u32 vol_u32;	/* TOD: ezek megszunnek ha atirom readw()/writew()-re - kész */
typedef volatile u8 vol_u8;

typedef volatile struct		/* counters of E1-errors and errored seconds, see rfc2495 */
{
    /* use here only unsigned ints, we depend on it when calculating the sum for the last N intervals */

    unsigned line_code_violations,	/* AMI: bipolar violations, HDB3: hdb3 violations                       */
      path_code_violations,	/* FAS errors and CRC4 errors                                                   */
      e_bit_errors,		/* E-Bit Errors (the remote side received from us with CRC4-error) */
      slip_secs,		/* number of seconds with (receive) Controlled Slip(s)          */
      fr_loss_secs,		/* number of seconds an Out Of Frame defect was detected                */
      line_err_secs,		/* number of seconds with one or more Line Code Violations              */
      degraded_mins,		/* Degraded Minute - the estimated error rate is >1E-6, but <1E-3       */
      errored_secs,		/* Errored Second - at least one of these happened:
				   - Path Code Violation
				   - Out Of Frame defect
				   - Slip
				   - receiving AIS
				   - not incremented during an Unavailable Second                       */
      bursty_err_secs,		/* Bursty Errored Second: (rfc2495 says it does not apply to E1)
				   - Path Code Violations >1, but <320
				   - not a Severely Errored Second
				   - no AIS
				   - not incremented during an Unavailable Second                       */
      severely_err_secs,	/* Severely Errored Second:
				   - CRC4: >=832 Path COde Violations || >0 Out Of Frame defects
				   - noCRC4: >=2048 Line Code Violations
				   - not incremented during an Unavailable Second                       */
      unavail_secs;		/* number of Unavailable Seconds. Unavailable state is said after:
				   - 10 contiguous Severely Errored Seconds
				   - or RAI || AIS || LOF || LOS 
				   - (any) loopback has been set                                                */

    /*
     * we do not strictly comply to the rfc: we do not retroactively reduce errored_secs,
     * bursty_err_secs, severely_err_secs when 'unavailable state' is reached
     */

} e1_stats_t;

typedef volatile struct		/* ezek board-adatok, nem lehetnek a slicecom_privdata -ban     */
{
    int use_count;		/* num. of interfaces using the board                           */
    int irq;			/* a kartya irq-ja. belemasoljuk a dev->irq -kba is, de csak hogy       */
    /* szebb legyen az ifconfig outputja                            */
    /* ha != 0, az azt jelenti hogy az az irq most nekunk sikeresen */
    /* le van foglalva                                              */
    struct pci_dev *pci;	/* a kartya PCI strukturaja. NULL, ha nincs kartya              */
    u32 *bar1;			/* pci->base_address[0] ioremap()-ed by munich_probe(),         */
    /* on x86 can be used both as a bus or virtual address.         */
    /* These are the Munich's registers                             */
    u8 *lbi;			/* pci->base_address[1] ioremap()-ed by munich_probe(),         */
    /* this is a 256-byte range, the start of the LBI on the board  */
    munich_ccb_t *ccb;		/* virtual address of CCB                                       */
    munich_intq_t *tiq;		/* Tx Interrupt Queue                                           */
    munich_intq_t *riq;		/* Rx Interrupt Queue                                           */
    munich_intq_t *piq;		/* Peripheral Interrupt Queue (FALC interrupts arrive here)     */
    int tiq_ptr,		/* A 'current' helyek a tiq/riq/piq -ban.                       */
      riq_ptr,			/* amikor feldolgoztam az interruptokat, a legelso ures         */
      piq_ptr;			/* interrupt_information szora mutatnak.                        */
    struct net_device *twins[32];	/* MUNICH channel -> network interface assignment       */

    unsigned long lastcheck;	/* When were the Rx rings last checked. Time in jiffies         */

    struct timer_list modemline_timer;
    char isx21;
    char lineup;
    char framing;		/* a beallitasok tarolasa                               */
    char linecode;
    char clock_source;
    char loopback;

    char devname[30];		/* what to show in /proc/interrupts                     */
    unsigned histogram[MAX_WORK];	/* number of processed events in the interrupt loop     */
    unsigned stat_pri_races;	/* number of special events, we try to handle them      */
    unsigned stat_pti_races;
    unsigned stat_pri_races_missed;	/* when it can not be handled, because of MAX_WORK      */
    unsigned stat_pti_races_missed;

#define SLICECOM_BOARD_INTERVALS_SIZE	97
    e1_stats_t intervals[SLICECOM_BOARD_INTERVALS_SIZE];	/* E1 line statistics           */
    unsigned current_interval;	/* pointer to the current interval                      */
    unsigned elapsed_seconds;	/* elapsed seconds from the start of the current interval */
    unsigned ses_seconds;	/* counter of contiguous Severely Errored Seconds       */
    unsigned is_unavailable;	/* set to 1 after 10 contiguous Severely Errored Seconds */
    unsigned no_ses_seconds;	/* contiguous Severely Error -free seconds in unavail state */

    unsigned deg_elapsed_seconds;	/* for counting the 'Degraded Mins'                     */
    unsigned deg_cumulated_errors;

    struct module *owner;	/* pointer to our module to avoid module load races */
} munich_board_t;

struct slicecom_privdata
{
    int busy;			/* transmitter busy - number of packets in the Tx ring  */
    int channel;		/* Munich logical channel ('channel-group' in Cisco)    */
    unsigned boardnum;
    u32 timeslots;		/* i-th bit means i-th timeslot is our                  */

    int tx_ring_hist[TX_DESC_MAX];	/* histogram: number of packets in Tx ring when _send_packet is called  */

    tx_desc_t tx_desc[TX_DESC_MAX];	/* the ring of Tx descriptors                           */
    u8 tx_data[TX_DESC_MAX][TXBUFFER_SIZE];	/* buffers for data to transmit                 */
    int tx_desc_ptr;		/* hanyadik descriptornal tartunk a beirassal   */
    /* ahol ez all, oda irtunk utoljara                     */

    rx_desc_t rx_desc[RX_DESC_MAX];	/* the ring of Rx descriptors                           */
    u8 rx_data[RX_DESC_MAX][RXBUFFER_SIZE];	/* buffers for received data                            */
    int rx_desc_ptr;		/* hanyadik descriptornal tartunk az olvasassal */

    int rafutott;
};

static u32 reg, reg_ertek;	/* why static: don't write stack trash into regs if strtoul() fails */
static u32 lbireg;
static u8 lbireg_ertek;		/* why static: don't write stack trash into regs if strtoul() fails */

static munich_board_t slicecom_boards[MAX_BOARDS];
static munich_board_t pcicom_boards[MAX_BOARDS];

/*
 * Reprogram Idle Channel Registers in the FALC - send special code in not used channels
 * Should be called from the open and close, when the timeslot assignment changes
 */

void rework_idle_channels(struct net_device *dev)
{
    struct comx_channel *ch = dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;
    munich_board_t *board = slicecom_boards + hw->boardnum;
    munich_ccb_t *ccb = board->ccb;

    u8 *lbi = board->lbi;
    int i, j, tmp;


    spin_lock_irqsave(&mister_lock, flags);

    for (i = 0; i < 4; i++)
    {
	tmp = 0xFF;
	for (j = 0; j < 8; j++)
	    if (ccb->timeslot_spec[8 * i + j].tti == 0) tmp ^= (0x80 >> j);
	writeb(tmp, lbi + 0x30 + i);
    }

    spin_unlock_irqrestore(&mister_lock, flags);
}

/*
 * Set PCM framing - /proc/comx/comx0/framing
 */

void slicecom_set_framing(int boardnum, int value)
{
    u8 *lbi = slicecom_boards[boardnum].lbi;

    spin_lock_irqsave(&mister_lock, flags);

    slicecom_boards[boardnum].framing = value;
    switch (value)
    {
	case SLICECOM_FRAMING_CRC4:
	    writeb(readb(lbi + FMR1) | 8, lbi + FMR1);
	    writeb((readb(lbi + FMR2) & 0x3f) | 0x80, lbi + FMR2);
	    break;
	case SLICECOM_FRAMING_NO_CRC4:
	    writeb(readb(lbi + FMR1) & 0xf7, lbi + FMR1);
	    writeb(readb(lbi + FMR2) & 0x3f, lbi + FMR2);
	    break;
	default:
	    printk("slicecom: board %d: unhandled " FILENAME_FRAMING
		   " value %d\n", boardnum, value);
    }

    spin_unlock_irqrestore(&mister_lock, flags);
}

/*
 * Set PCM linecode - /proc/comx/comx0/linecode
 */

void slicecom_set_linecode(int boardnum, int value)
{
    u8 *lbi = slicecom_boards[boardnum].lbi;

    spin_lock_irqsave(&mister_lock, flags);

    slicecom_boards[boardnum].linecode = value;
    switch (value)
    {
	case SLICECOM_LINECODE_HDB3:
	    writeb(readb(lbi + FMR0) | 0xf0, lbi + FMR0);
	    break;
	case SLICECOM_LINECODE_AMI:
	    writeb((readb(lbi + FMR0) & 0x0f) | 0xa0, lbi + FMR0);
	    break;
	default:
	    printk("slicecom: board %d: unhandled " FILENAME_LINECODE
		   " value %d\n", boardnum, value);
    }
    spin_unlock_irqrestore(&mister_lock, flags);
}

/*
 * Set PCM clock source - /proc/comx/comx0/clock_source
 */

void slicecom_set_clock_source(int boardnum, int value)
{
    u8 *lbi = slicecom_boards[boardnum].lbi;

    spin_lock_irqsave(&mister_lock, flags);

    slicecom_boards[boardnum].clock_source = value;
    switch (value)
    {
	case SLICECOM_CLOCK_SOURCE_LINE:
	    writeb(readb(lbi + LIM0) & ~1, lbi + LIM0);
	    break;
	case SLICECOM_CLOCK_SOURCE_INTERNAL:
	    writeb(readb(lbi + LIM0) | 1, lbi + LIM0);
	    break;
	default:
	    printk("slicecom: board %d: unhandled " FILENAME_CLOCK_SOURCE
		   " value %d\n", boardnum, value);
    }
    spin_unlock_irqrestore(&mister_lock, flags);
}

/*
 * Set loopbacks - /proc/comx/comx0/loopback
 */

void slicecom_set_loopback(int boardnum, int value)
{
    u8 *lbi = slicecom_boards[boardnum].lbi;

    spin_lock_irqsave(&mister_lock, flags);

    slicecom_boards[boardnum].loopback = value;
    switch (value)
    {
	case SLICECOM_LOOPBACK_NONE:
	    writeb(readb(lbi + LIM0) & ~2, lbi + LIM0);	/* Local Loop OFF  */
	    writeb(readb(lbi + LIM1) & ~2, lbi + LIM1);	/* Remote Loop OFF */
	    break;
	case SLICECOM_LOOPBACK_LOCAL:
	    writeb(readb(lbi + LIM1) & ~2, lbi + LIM1);	/* Remote Loop OFF */
	    writeb(readb(lbi + LIM0) | 2, lbi + LIM0);	/* Local Loop ON   */
	    break;
	case SLICECOM_LOOPBACK_REMOTE:
	    writeb(readb(lbi + LIM0) & ~2, lbi + LIM0);	/* Local Loop OFF  */
	    writeb(readb(lbi + LIM1) | 2, lbi + LIM1);	/* Remote Loop ON  */
	    break;
	default:
	    printk("slicecom: board %d: unhandled " FILENAME_LOOPBACK
		   " value %d\n", boardnum, value);
    }
    spin_unlock_irqrestore(&mister_lock, flags);
}

/*
 * Update E1 line status LEDs on the adapter
 */

void slicecom_update_leds(munich_board_t * board)
{
    u32 *bar1 = board->bar1;
    u8 *lbi = board->lbi;
    u8 frs0;
    u32 leds;
    int i;

    spin_lock_irqsave(&mister_lock, flags);

    leds = 0;
    frs0 = readb(lbi + FRS0);	/* FRS0 bits described on page 137 */

    if (!(frs0 & 0xa0))
    {
	leds |= 0x2000;		/* Green LED: Input signal seems to be OK, no LOS, no LFA       */
	if (frs0 & 0x10)
	    leds |= 0x8000;	/* Red LED: Receiving Remote Alarm                                      */
    }
    writel(leds, MUNICH_VIRT(GPDATA));

    if (leds == 0x2000 && !board->lineup)
    {				/* line up */
	board->lineup = 1;
	for (i = 0; i < 32; i++)
	{
	    if (board->twins[i] && (board->twins[i]->flags & IFF_RUNNING))
	    {
		struct comx_channel *ch = board->twins[i]->priv;

		if (!test_and_set_bit(0, &ch->lineup_pending))
		{
		    ch->lineup_timer.function = comx_lineup_func;
		    ch->lineup_timer.data = (unsigned long)board->twins[i];
		    ch->lineup_timer.expires = jiffies + HZ * ch->lineup_delay;
		    add_timer(&ch->lineup_timer);
		}
	    }
	}
    }
    else if (leds != 0x2000 && board->lineup)
    {				/* line down */
	board->lineup = 0;
	for (i = 0; i < 32; i++)
	    if (board->twins[i] && (board->twins[i]->flags & IFF_RUNNING))
	    {
		struct comx_channel *ch = board->twins[i]->priv;

		if (test_and_clear_bit(0, &ch->lineup_pending))
		    del_timer(&ch->lineup_timer);
		else if (ch->line_status & LINE_UP)
		{
		    ch->line_status &= ~LINE_UP;
		    if (ch->LINE_status)
			ch->LINE_status(board->twins[i], ch->line_status);
		}
	    }
    }
    spin_unlock_irqrestore(&mister_lock, flags);
}

/*
 * This function gets called every second when the FALC issues the interrupt.
 * Hardware counters contain error counts for last 1-second time interval.
 * We add them to the global counters here.
 * Read rfc2495 to understand this.
 */

void slicecom_update_line_counters(munich_board_t * board)
{
    e1_stats_t *curr_int = &board->intervals[board->current_interval];

    u8 *lbi = board->lbi;

    unsigned framing_errors, code_violations, path_code_violations, crc4_errors,
	e_bit_errors;
    unsigned slip_detected,	/* this one has logical value, not the number of slips! */
      out_of_frame_defect,	/* logical value        */
      ais_defect,		/* logical value        */
      errored_sec, bursty_err_sec, severely_err_sec = 0, failure_sec;
    u8 isr2, isr3, isr5, frs0;

    spin_lock_irqsave(&mister_lock, flags);

    isr2 = readb(lbi + ISR2);	/* ISR0-5 described on page 156     */
    isr3 = readb(lbi + ISR3);
    isr5 = readb(lbi + ISR5);
    frs0 = readb(lbi + FRS0);	/* FRS0 described on page 137       */

    /* Error Events: */

    code_violations = readb(lbi + CVCL) + (readb(lbi + CVCH) << 8);
    framing_errors = readb(lbi + FECL) + (readb(lbi + FECH) << 8);
    crc4_errors = readb(lbi + CEC1L) + (readb(lbi + CEC1H) << 8);
    e_bit_errors = readb(lbi + EBCL) + (readb(lbi + EBCH) << 8);
    slip_detected = isr3 & (ISR3_RSN | ISR3_RSP);

    path_code_violations = framing_errors + crc4_errors;

    curr_int->line_code_violations += code_violations;
    curr_int->path_code_violations += path_code_violations;
    curr_int->e_bit_errors += e_bit_errors;

    /* Performance Defects: */

    /* there was an LFA in the last second, but maybe disappeared: */
    out_of_frame_defect = (isr2 & ISR2_LFA) || (frs0 & FRS0_LFA);

    /* there was an AIS in the last second, but maybe disappeared: */
    ais_defect = (isr2 & ISR2_AIS) || (frs0 & FRS0_AIS);

    /* Performance Parameters: */

    if (out_of_frame_defect)
	curr_int->fr_loss_secs++;
    if (code_violations)
	curr_int->line_err_secs++;

    errored_sec = ((board->framing == SLICECOM_FRAMING_NO_CRC4) &&
		   (code_violations)) || path_code_violations ||
	out_of_frame_defect || slip_detected || ais_defect;

    bursty_err_sec = !out_of_frame_defect && !ais_defect &&
	(path_code_violations > 1) && (path_code_violations < 320);

    switch (board->framing)
    {
	case SLICECOM_FRAMING_CRC4:
	    severely_err_sec = out_of_frame_defect ||
		(path_code_violations >= 832);
	    break;
	case SLICECOM_FRAMING_NO_CRC4:
	    severely_err_sec = (code_violations >= 2048);
	    break;
    }

    /*
     * failure_sec: true if there was a condition leading to a failure
     * (and leading to unavailable state) in this second:
     */

    failure_sec = (isr2 & ISR2_RA) || (frs0 & FRS0_RRA)	/* Remote/Far End/Distant Alarm Failure */
	|| ais_defect || out_of_frame_defect	/* AIS or LOF Failure                           */
	|| (isr2 & ISR2_LOS) || (frs0 & FRS0_LOS)	/* Loss Of Signal Failure                       */
	|| (board->loopback != SLICECOM_LOOPBACK_NONE);	/* Loopback has been set                        */

    if (board->is_unavailable)
    {
	if (severely_err_sec)
	    board->no_ses_seconds = 0;
	else
	    board->no_ses_seconds++;

	if ((board->no_ses_seconds >= 10) && !failure_sec)
	{
	    board->is_unavailable = 0;
	    board->ses_seconds = 0;
	    board->no_ses_seconds = 0;
	}
    }
    else
    {
	if (severely_err_sec)
	    board->ses_seconds++;
	else
	    board->ses_seconds = 0;

	if ((board->ses_seconds >= 10) || failure_sec)
	{
	    board->is_unavailable = 1;
	    board->ses_seconds = 0;
	    board->no_ses_seconds = 0;
	}
    }

    if (board->is_unavailable)
	curr_int->unavail_secs++;
    else
    {
	if (slip_detected)
	    curr_int->slip_secs++;
	curr_int->errored_secs += errored_sec;
	curr_int->bursty_err_secs += bursty_err_sec;
	curr_int->severely_err_secs += severely_err_sec;
    }

    /* the RFC does not say clearly which errors to count here, we try to count bit errors */

    if (!board->is_unavailable && !severely_err_sec)
    {
	board->deg_cumulated_errors += code_violations;
	board->deg_elapsed_seconds++;
	if (board->deg_elapsed_seconds >= 60)
	{
	    if (board->deg_cumulated_errors >= 123)
		curr_int->degraded_mins++;
	    board->deg_cumulated_errors = 0;
	    board->deg_elapsed_seconds = 0;
	}

    }

    board->elapsed_seconds++;
    if (board->elapsed_seconds >= 900)
    {
	board->current_interval =
	    (board->current_interval + 1) % SLICECOM_BOARD_INTERVALS_SIZE;
	memset((void *)&board->intervals[board->current_interval], 0,
	       sizeof(e1_stats_t));
	board->elapsed_seconds = 0;
    }

    spin_unlock_irqrestore(&mister_lock, flags);
}

static void pcicom_modemline(unsigned long b)
{
    munich_board_t *board = (munich_board_t *) b;
    struct net_device *dev = board->twins[0];
    struct comx_channel *ch = dev->priv;
    unsigned long regs;

    regs = readl((void *)(&board->bar1[GPDATA]));
    if ((ch->line_status & LINE_UP) && (regs & 0x0800))
    {
	ch->line_status &= ~LINE_UP;
	board->lineup = 0;
	if (ch->LINE_status)
	{
	    ch->LINE_status(dev, ch->line_status);
	}
    }

    if (!(ch->line_status & LINE_UP) && !(regs & 0x0800))
    {
	ch->line_status |= LINE_UP;
	board->lineup = 1;
	if (ch->LINE_status)
	{
	    ch->LINE_status(dev, ch->line_status);
	}
    }

    mod_timer((struct timer_list *)&board->modemline_timer, jiffies + HZ);
}

/* 
 * Is it possible to transmit ?
 * Called (may be called) by the protocol layer 
 */

static int MUNICH_txe(struct net_device *dev)
{
    struct comx_channel *ch = dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;

    return (hw->busy < TX_DESC_MAX - 1);
}

/* 
 * Hw probe function. Detects all the boards in the system,
 * and fills up slicecom_boards[] and pcicom_boards[]
 * Returns 0 on success.
 * We do not disable interrupts!
 */
static int munich_probe(void)
{
    struct pci_dev *pci;
    int boardnum;
    int slicecom_boardnum;
    int pcicom_boardnum;
    u32 *bar1;
    u8 *lbi;
    munich_board_t *board;

    for (boardnum = 0; boardnum < MAX_BOARDS; boardnum++)
    {
	pcicom_boards[boardnum].pci = 0;
	pcicom_boards[boardnum].bar1 = 0;
	pcicom_boards[boardnum].lbi = 0;
	slicecom_boards[boardnum].pci = 0;
	slicecom_boards[boardnum].bar1 = 0;
	slicecom_boards[boardnum].lbi = 0;
    }

    pci = NULL;
    board = NULL;
    slicecom_boardnum = 0;
    pcicom_boardnum = 0;

    for (boardnum = 0;
	boardnum < MAX_BOARDS && (pci = pci_find_device(PCI_VENDOR_ID_SIEMENS,
	PCI_DEVICE_ID_SIEMENS_MUNICH32X, pci)); boardnum++)
    {
	if (pci_enable_device(pci))
	    continue;

	printk("munich_probe: munich chip found, IRQ %d\n", pci->irq);

#if (LINUX_VERSION_CODE < 0x02030d)
	bar1 = ioremap_nocache(pci->base_address[0], 0x100);
	lbi = ioremap_nocache(pci->base_address[1], 0x100);
#else
	bar1 = ioremap_nocache(pci->resource[0].start, 0x100);
	lbi = ioremap_nocache(pci->resource[1].start, 0x100);
#endif

	if (bar1 && lbi)
	{
	    pci_write_config_dword(pci, MUNICH_PCI_PCIRES, 0xe0000);
	    set_current_state(TASK_UNINTERRUPTIBLE);
	    schedule_timeout(1);
	    pci_write_config_dword(pci, MUNICH_PCI_PCIRES, 0);
	    set_current_state(TASK_UNINTERRUPTIBLE);
	    schedule_timeout(1);
	    /* check the type of the card */
	    writel(LREG0_MAGIC, MUNICH_VIRT(LREG0));
	    writel(LREG1_MAGIC, MUNICH_VIRT(LREG1));
	    writel(LREG2_MAGIC, MUNICH_VIRT(LREG2));
	    writel(LREG3_MAGIC, MUNICH_VIRT(LREG3));
	    writel(LREG4_MAGIC, MUNICH_VIRT(LREG4));
	    writel(LREG5_MAGIC, MUNICH_VIRT(LREG5));
	    writel(LCONF_MAGIC2,MUNICH_VIRT(LCONF));	/* enable the DMSM */

	    if ((readb(lbi + VSTR) == 0x13) || (readb(lbi + VSTR) == 0x10))
	    {
		board = slicecom_boards + slicecom_boardnum;
		sprintf((char *)board->devname, "slicecom%d",
			slicecom_boardnum);
		board->isx21 = 0;
		slicecom_boardnum++;
	    }
	    else if ((readb(lbi + VSTR) == 0x6) || (readb(lbi + GIS) == 0x6))
	    {
		board = pcicom_boards + pcicom_boardnum;
		sprintf((char *)board->devname, "pcicom%d", pcicom_boardnum);
		board->isx21 = 1;
		pcicom_boardnum++;
	    }
	    if (board)
	    {
		printk("munich_probe: %s board found\n", board->devname);
		writel(LCONF_MAGIC1, MUNICH_VIRT(LCONF));	/* reset the DMSM */
		board->pci = pci;
		board->bar1 = bar1;
		board->lbi = lbi;
		board->framing = SLICECOM_FRAMING_DEFAULT;
		board->linecode = SLICECOM_LINECODE_DEFAULT;
		board->clock_source = SLICECOM_CLOCK_SOURCE_DEFAULT;
		board->loopback = SLICECOM_LOOPBACK_DEFAULT;
		SET_MODULE_OWNER(board);
	    }
	    else
	    {
		printk("munich_probe: Board error, VSTR: %02X\n",
		       readb(lbi + VSTR));
		iounmap((void *)bar1);
		iounmap((void *)lbi);
	    }
	}
	else
	{
	    printk("munich_probe: ioremap() failed, not enabling this board!\n");
	    /* .pci = NULL, so the MUNICH_open will not try to open it            */
	    if (bar1) iounmap((void *)bar1);
	    if (lbi) iounmap((void *)lbi);
	}
    }

    if (!pci && !boardnum)
    {
	printk("munich_probe: no PCI present!\n");
	return -ENODEV;
    }

    if (pcicom_boardnum + slicecom_boardnum == 0)
    {
	printk
	    ("munich_probe: Couldn't find any munich board: vendor:device %x:%x not found\n",
	     PCI_VENDOR_ID_SIEMENS, PCI_DEVICE_ID_SIEMENS_MUNICH32X);
	return -ENODEV;
    }

    /* Found some */
    if (pcicom_boardnum)
	printk("%d pcicom board(s) found.\n", pcicom_boardnum);
    if (slicecom_boardnum)
	printk("%d slicecom board(s) found.\n", slicecom_boardnum);

    return 0;
}

/* 
 * Reset the hardware. Get called only from within this module if needed.
 */
#if 0
static int slicecom_reset(struct net_device *dev)
{
    struct comx_channel *ch = dev->priv;

    printk("slicecom_reset: resetting the hardware\n");

    /* Begin to reset the hardware */

    if (ch->HW_set_clock)
	ch->HW_set_clock(dev);

    /* And finish it */

    return 0;
}
#endif

/* 
 * Transmit a packet. 
 * Called by the protocol layer
 * Return values:	
 *	FRAME_ACCEPTED:	frame is being transmited, transmitter is busy
 *	FRAME_QUEUED:	frame is being transmitted, there's more room in
 *				the transmitter for additional packet(s)
 *	FRAME_ERROR:
 *	FRAME_DROPPED:	there was some error
 */

static int MUNICH_send_packet(struct net_device *dev, struct sk_buff *skb)
{
    struct comx_channel *ch = (struct comx_channel *)dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;

    /* Send it to the debug facility too if needed: */

    if (ch->debug_flags & DEBUG_HW_TX)
	comx_debug_bytes(dev, skb->data, skb->len, "MUNICH_send_packet");

    /* If the line is inactive, don't accept: */

    /* TODO: atgondolni hogy mi is legyen itt */
    /* if (!(ch->line_status & LINE_UP)) return FRAME_DROPPED; */

    /* More check, to be sure: */

    if (skb->len > TXBUFFER_SIZE)
    {
	ch->stats.tx_errors++;
	kfree_skb(skb);
	return FRAME_ERROR;
    }

    /* Maybe you have to disable irq's while programming the hw: */

    spin_lock_irqsave(&mister_lock, flags);

    /* And more check: */

    if (hw->busy >= TX_DESC_MAX - 1)
    {
	printk(KERN_ERR
	       "%s: Transmitter called while busy... dropping frame, busy = %d\n",
	       dev->name, hw->busy);
	spin_unlock_irqrestore(&mister_lock, flags);
	kfree_skb(skb);
	return FRAME_DROPPED;
    }

    if (hw->busy >= 0)
	hw->tx_ring_hist[hw->busy]++;
    /* DELL: */
    else
	printk("slicecom: %s: FATAL: busy = %d\n", dev->name, hw->busy);

//              /* DEL: */
//      printk("slicecom: %s: _send_packet called, busy = %d\n", dev->name, hw->busy );

    /* Packet can go, update stats: */

    ch->stats.tx_packets++;
    ch->stats.tx_bytes += skb->len;

    /* Pass the packet to the HW:                   */
    /* Step forward with the transmit descriptors:  */

    hw->tx_desc_ptr = (hw->tx_desc_ptr + 1) % TX_DESC_MAX;

    memcpy(&(hw->tx_data[hw->tx_desc_ptr][0]), skb->data, skb->len);
    hw->tx_desc[hw->tx_desc_ptr].no = skb->len;

    /* We don't issue any command, just step with the HOLD bit      */

    hw->tx_desc[hw->tx_desc_ptr].hold = 1;
    hw->tx_desc[(hw->tx_desc_ptr + TX_DESC_MAX - 1) % TX_DESC_MAX].hold = 0;

#ifdef COMX_NEW
    dev_kfree_skb(skb);
#endif
    /* csomag kerult a Tx ringbe: */

    hw->busy++;

    /* Report it: */

    if (ch->debug_flags & DEBUG_HW_TX)
	comx_debug(dev, "%s: MUNICH_send_packet was successful\n\n", dev->name);

    if (hw->busy >= TX_DESC_MAX - 1)
    {
	spin_unlock_irqrestore(&mister_lock, flags);
	return FRAME_ACCEPTED;
    }

    spin_unlock_irqrestore(&mister_lock, flags);

    /* All done */

    return FRAME_QUEUED;
}

/*
 * Interrupt handler routine.
 * Called by the Linux kernel.
 * BEWARE! The interrupts are enabled on the call!
 */
static void MUNICH_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    struct sk_buff *skb;
    int length;
    int rx_status;
    int work;			/* hany esemenyt kezeltem mar le                                */
    u32 *bar1;
    u8 *lbi;
    u32 stat,			/* az esemenyek, amiket a ebben a loop korben le kell meg kezelni       */
      race_stat = 0,		/* race eseten ebben uzenek magamnak hogy mit kell meg lekezelni        */
      ack;			/* ezt fogom a vegen a STAT-ba irni, kiveszek belole 1-1 bitet ha       */

    /* az adott dolgot nem kell ack-olni mert volt vele munkam, es  */
    /* legjobb ha visszaterek ide megegyszer                        */
    munich_intq_t int_info;

    struct net_device *dev;
    struct comx_channel *ch;
    struct slicecom_privdata *hw;
    munich_board_t *board = (munich_board_t *) dev_id;
    int channel;

    //      , boardnum = (int)dev_id;

    // board = munich_boards + boardnum;
    bar1 = board->bar1;
    lbi = board->lbi;

    //      Do not uncomment this under heavy load! :->
    //      printk("MUNICH_interrupt: masked STAT=0x%08x, tiq=0x%08x, riq=0x%08x, piq=0x%08x\n", stat, board->tiq[0].all, board->riq[0].all, board->piq[0].all );

    for (work = 0; (stat = (race_stat | (readl(MUNICH_VIRT(STAT)) & ~STAT_NOT_HANDLED_BY_INTERRUPT))) && (work < MAX_WORK - 1); work++)
    {
	ack = stat & (STAT_PRI | STAT_PTI | STAT_LBII);

	/* Handle the interrupt information in the Rx queue. We don't really trust      */
	/* info from this queue, because it can be overflowed, so later check           */
	/* every Rx ring for received packets. But there are some errors which can't    */
	/* be counted from the Rx rings, so we parse it.                                        */

	int_info = board->riq[board->riq_ptr];
	if (int_info.all & 0xF0000000)	/* ha ez nem 0, akkor itt interrupt_info van                    */
	{
	    ack &= ~STAT_PRI;	/* don't ack the interrupt, we had some work to do              */

	    channel = PCM_INT_CHANNEL(int_info.all);
	    dev = board->twins[channel];

	    if (dev == NULL)
	    {
		printk
		    ("MUNICH_interrupt: got an Rx interrupt info for NULL device "
		     "%s.twins[%d], int_info = 0x%08x\n", board->devname,
		     channel, int_info.all);
		goto go_for_next_interrupt;
	    }

	    ch = (struct comx_channel *)dev->priv;
	    hw = (struct slicecom_privdata *)ch->HW_privdata;

	    //      printk("Rx STAT=0x%08x int_info=0x%08x rx_desc_ptr=%d rx_desc.status=0x%01x\n",
	    //              stat, int_info.all, hw->rx_desc_ptr, hw->rx_desc[ hw->rx_desc_ptr ].status );

	    if (int_info.all & PCM_INT_HI)
		printk("SliceCOM: %s: Host Initiated interrupt\n", dev->name);
	    if (int_info.all & PCM_INT_IFC)
		printk("SliceCOM: %s: Idle/Flag Change\n", dev->name);
	    /* TOD: jo ez az Idle/Flag Change valamire? - azonnal latszik belole hogy mikor ad a masik oldal */
	    /* TOD: ilyen IT most nem is jon, mert ki van maszkolva az interrupt, biztosan kell ez? */

	    if (int_info.all & PCM_INT_FO)
		/* Internal buffer (RB) overrun */
		ch->stats.rx_over_errors++;	/* TOD: Ez azt jelenti hogy a belso RB nem volt hozzaferheto, es ezert kihagyott valamit. De nem csak csomag lehetett, hanem esemeny, stb. is. lasd page 247. Ezzel a 'cat status'-hoz igazodok, de a netdevice.h szerint nem egyertelmu hogy ide ez kellene. Nem lehet hogy rx_missed ? */
		/* DE: nem gotozok sehova, elvileg jo igy */
		/* kesobb meg visszaterek az FO-ra, ha packet-FO volt. Keresd a "packet-FO"-t. */
	    if (int_info.all & PCM_INT_FI)	/* frame received, but we do not trust the int_info queue       */
		if (int_info.all & PCM_INT_SF)
		{		/* Short Frame: rovidebb mint a CRC */
		    /* "rovidebb mint CRC+2byte" vizsgalat a "CRC+2"-nel */
		    ch->stats.rx_length_errors++;	/* TOD: noveljem? ne noveljem? */
		    goto go_for_next_interrupt;
		}

	    go_for_next_interrupt:	/* One step in the interrupt queue */
	    board->riq[board->riq_ptr].all = 0;	/* megjelolom hogy itt meg nem jart a hw */
	    board->riq_ptr = (board->riq_ptr + 1) % MUNICH_INTQMAX;

	}

	/* Check every Rx ring for incomed packets: */

	for (channel = 0; channel < 32; channel++)
	{
	    dev = board->twins[channel];

	    if (dev != NULL)
	    {
		ch = (struct comx_channel *)dev->priv;
		hw = (struct slicecom_privdata *)ch->HW_privdata;

		rx_status = hw->rx_desc[hw->rx_desc_ptr].status;

		if (!(rx_status & 0x80))	/* mar jart itt a hardver */
		{
		    ack &= ~STAT_PRI;	/* Don't ack, we had some work          */

		    /* Ez most egy kicsit zuros, mert itt mar nem latom az int_infot        */
		    if (rx_status & RX_STATUS_ROF)
			ch->stats.rx_over_errors++;	/* TOD: 'cat status'-hoz igazodok */

		    if (rx_status & RX_STATUS_RA)
			/* Abort received or issued on channel  */
			ch->stats.rx_frame_errors++;	/* or HOLD bit in the descriptor                */
			/* TOD: 'cat status'-hoz igazodok */

		    if (rx_status & RX_STATUS_LFD)
		    {		/* Long Frame (longer then MFL in the MODE1) */
			ch->stats.rx_length_errors++;
			goto go_for_next_frame;
		    }

		    if (rx_status & RX_STATUS_NOB)
		    {		/* Not n*8 bits long frame - frame alignment */
			ch->stats.rx_frame_errors++;	/* ez viszont nem igazodik a 'cat status'-hoz */
			goto go_for_next_frame;
		    }

		    if (rx_status & RX_STATUS_CRCO)
		    {		/* CRC error */
			ch->stats.rx_crc_errors++;
			goto go_for_next_frame;
		    }

		    if (rx_status & RX_STATUS_SF)
		    {		/* Short Frame: rovidebb mint CRC+2byte */
			ch->stats.rx_errors++;	/* The HW does not set PCI_INT_ERR bit for this one, see page 246 */
			ch->stats.rx_length_errors++;
			goto go_for_next_frame;
		    }

		    if (rx_status != 0)
		    {
			printk("SliceCOM: %s: unhandled rx_status: 0x%02x\n",
			       dev->name, rx_status);
			goto go_for_next_frame;
		    }

		    /* frame received without errors: */

		    length = hw->rx_desc[hw->rx_desc_ptr].bno;
		    ch->stats.rx_packets++;	/* Count only 'good' packets */
		    ch->stats.rx_bytes += length;

		    /* Allocate a larger skb and reserve the heading for efficiency: */

		    if ((skb = dev_alloc_skb(length + 16)) == NULL)
		    {
			ch->stats.rx_dropped++;
			goto go_for_next_frame;
		    }

		    /* Do bookkeeping: */

		    skb_reserve(skb, 16);
		    skb_put(skb, length);
		    skb->dev = dev;

		    /* Now copy the data into the buffer: */

		    memcpy(skb->data, &(hw->rx_data[hw->rx_desc_ptr][0]), length);

		    /* DEL: UGLY HACK!!!! */
		    if (*((int *)skb->data) == 0x02000000 &&
			*(((int *)skb->data) + 1) == 0x3580008f)
		    {
			printk("%s: swapping hack\n", dev->name);
			*((int *)skb->data) = 0x3580008f;
			*(((int *)skb->data) + 1) = 0x02000000;
		    }

		    if (ch->debug_flags & DEBUG_HW_RX)
			comx_debug_skb(dev, skb, "MUNICH_interrupt receiving");

		    /* Pass it to the protocol entity: */

		    ch->LINE_rx(dev, skb);

		    go_for_next_frame:
		    /* DEL: rafutott-e a HOLD bitre -detektalas */
		    {
			if( ((rx_desc_t*)phys_to_virt(board->ccb->current_rx_desc[channel]))->hold
			    && ((rx_desc_t*)phys_to_virt(board->ccb->current_rx_desc[channel]))->status != 0xff)
			    hw->rafutott++;	/* rafutott: hanyszor volt olyan hogy a current descriptoron HOLD bit volt, es a hw mar befejezte az irast (azaz a hw rafutott a HOLD bitre) */
		    }

		    //      if( jiffies % 2 )               /* DELL: okozzunk egy kis Rx ring slipet :) */
		    //      {
		    /* Step forward with the receive descriptors: */
		    /* if you change this, change the copy of it below too! Search for: "RxSlip" */
		    hw->rx_desc[(hw->rx_desc_ptr + RX_DESC_MAX - 1) % RX_DESC_MAX].hold = 1;
		    hw->rx_desc[hw->rx_desc_ptr].status = 0xFF;	/* megjelolom hogy itt meg nem jart a hw */
		    hw->rx_desc[(hw->rx_desc_ptr + RX_DESC_MAX - 2) % RX_DESC_MAX].hold = 0;
		    hw->rx_desc_ptr = (hw->rx_desc_ptr + 1) % RX_DESC_MAX;
		    //      }
		}
	    }
	}

	stat &= ~STAT_PRI;

//      }

//      if( stat & STAT_PTI )   /* TOD: primko megvalositas: mindig csak egy esemenyt dolgozok fel, */
	/* es nem torlom a STAT-ot, ezert ujra visszajon ide a rendszer. Amikor */
	/* jon interrupt, de nincs mit feldolgozni, akkor torlom a STAT-ot.     */
	/* 'needs a rewrite', de elso megoldasnak jo lesz                       */
//              {
	int_info = board->tiq[board->tiq_ptr];
	if (int_info.all & 0xF0000000)	/* ha ez nem 0, akkor itt interrupt_info van    */
	{
	    ack &= ~STAT_PTI;	/* don't ack the interrupt, we had some work to do      */

	    channel = PCM_INT_CHANNEL(int_info.all);
	    dev = board->twins[channel];

	    if (dev == NULL)
	    {
		printk("MUNICH_interrupt: got a Tx interrupt for NULL device "
		       "%s.twins[%d], int_info = 0x%08x\n",
		       board->isx21 ? "pcicom" : "slicecom", channel, int_info.all);
		goto go_for_next_tx_interrupt;
	    }

	    ch = (struct comx_channel *)dev->priv;
	    hw = (struct slicecom_privdata *)ch->HW_privdata;

	    //      printk("Tx STAT=0x%08x int_info=0x%08x tiq_ptr=%d\n", stat, int_info.all, board->tiq_ptr );

	    if (int_info.all & PCM_INT_FE2)
	    {			/* "Tx available"                               */
		/* do nothing */
	    }
	    else if (int_info.all & PCM_INT_FO)
	    {			/* Internal buffer (RB) overrun */
		ch->stats.rx_over_errors++;
	    }
	    else
	    {
		printk("slicecom: %s: unhandled Tx int_info: 0x%08x\n",
		       dev->name, int_info.all);
	    }

	    go_for_next_tx_interrupt:
	    board->tiq[board->tiq_ptr].all = 0;
	    board->tiq_ptr = (board->tiq_ptr + 1) % MUNICH_INTQMAX;
	}

	/* Check every Tx ring for incoming packets: */

	for (channel = 0; channel < 32; channel++)
	{
	    dev = board->twins[channel];

	    if (dev != NULL)
	    {
		int newbusy;

		ch = (struct comx_channel *)dev->priv;
		hw = (struct slicecom_privdata *)ch->HW_privdata;

		/* We dont trust the "Tx available" info from the TIQ, but check        */
		/* every ring if there is some free room                                        */

		if (ch->init_status && netif_running(dev))
		{
		    newbusy = ( TX_DESC_MAX + (& hw->tx_desc[ hw->tx_desc_ptr ]) -
			(tx_desc_t*)phys_to_virt(board->ccb->current_tx_desc[ hw->channel ]) ) % TX_DESC_MAX;

		    if(newbusy < 0)
		    {
			printk("slicecom: %s: FATAL: fresly computed busy = %d, HW: 0x%p, SW: 0x%p\n",
			dev->name, newbusy,
			phys_to_virt(board->ccb->current_tx_desc[hw->channel]),
			& hw->tx_desc[hw->tx_desc_ptr]);
		    }

		    /* Fogyott valami a Tx ringbol? */

		    if (newbusy < hw->busy)
		    {
			// ack &= ~STAT_PTI;                            /* Don't ack, we had some work  */
			hw->busy = newbusy;
			if (ch->LINE_tx)
			    ch->LINE_tx(dev);	/* Report it to protocol driver */
		    }
		    else if (newbusy > hw->busy)
			printk("slicecom: %s: newbusy > hw->busy, this should not happen!\n", dev->name);
		}
	    }
	}
	stat &= ~STAT_PTI;

	int_info = board->piq[board->piq_ptr];
	if (int_info.all & 0xF0000000)	/* ha ez nem 0, akkor itt interrupt_info van            */
	{
	    ack &= ~STAT_LBII;	/* don't ack the interrupt, we had some work to do      */

	    /* We do not really use (yet) the interrupt info from this queue, */

	    // printk("slicecom: %s: LBI Interrupt event: %08x\n", board->devname, int_info.all);

	    if (!board->isx21)
	    {
		slicecom_update_leds(board);
		slicecom_update_line_counters(board);
	    }

	    goto go_for_next_lbi_interrupt;	/* To avoid warning about unused label  */

	    go_for_next_lbi_interrupt:	/* One step in the interrupt queue */
	    board->piq[board->piq_ptr].all = 0;	/* megjelolom hogy itt meg nem jart a hw        */
	    board->piq_ptr = (board->piq_ptr + 1) % MUNICH_PIQMAX;
	}
	stat &= ~STAT_LBII;

	writel(ack, MUNICH_VIRT(STAT));

	if (stat & STAT_TSPA)
	{
	    //      printk("slicecom: %s: PCM TSP Asynchronous\n", board->devname);
	    writel(STAT_TSPA, MUNICH_VIRT(STAT));
	    stat &= ~STAT_TSPA;
	}

	if (stat & STAT_RSPA)
	{
	    //      printk("slicecom: %s: PCM RSP Asynchronous\n", board->devname);
	    writel(STAT_RSPA, MUNICH_VIRT(STAT));
	    stat &= ~STAT_RSPA;
	}
	if (stat)
	{
	    printk("MUNICH_interrupt: unhandled interrupt, STAT=0x%08x\n",
		   stat);
	    writel(stat, MUNICH_VIRT(STAT));	/* ha valamit megsem kezeltunk le, azert ack-ot kuldunk neki */
	}

    }
    board->histogram[work]++;

    /* We can miss these if we reach the MAX_WORK   */
    /* Count it to see how often it happens         */

    if (race_stat & STAT_PRI)
	board->stat_pri_races_missed++;
    if (race_stat & STAT_PTI)
	board->stat_pti_races_missed++;
    return;
}

/* 
 * Hardware open routine.
 * Called by comx (upper) layer when the user wants to bring up the interface
 * with ifconfig.
 * Initializes hardware, allocates resources etc.
 * Returns 0 on OK, or standard error value on error.
 */

static int MUNICH_open(struct net_device *dev)
{
    struct comx_channel *ch = dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;
    struct proc_dir_entry *procfile = ch->procdir->subdir;
    munich_board_t *board;
    munich_ccb_t *ccb;

    u32 *bar1;
    u8 *lbi;
    u32 stat;
    unsigned long flags, jiffs;

    int i, channel;
    u32 timeslots = hw->timeslots;

    board = hw->boardnum + (ch->hardware == &pcicomhw ? pcicom_boards : slicecom_boards);

    bar1 = board->bar1;
    lbi = board->lbi;

    /* TODO: a timeslotok ellenorzese kell majd ide .. hat, biztos? mar a write_proc-ban is
       ellenorzom valamennyire.
       if (!dev->io || !dev->irq) return -ENODEV;
     */

    if (!board->pci)
    {
	printk("MUNICH_open: no %s board with boardnum = %d\n",
	       ch->hardware->name, hw->boardnum);
	return -ENODEV;
    }

    spin_lock_irqsave(&mister_lock, flags);
    /* lock the section to avoid race with multiple opens and make sure
       that no interrupts get called while this lock is active */

    if (board->use_count == 0)	/* bring up the board if it was unused                  */
	/* if fails, frees allocated resources and returns.     */
	/* TOD: is it safe? nem kellene resetelni a kartyat?    */
    {
	printk("MUNICH_open: %s: bringing up board\n", board->devname);

	/* Clean up the board's static struct if messed: */

	for (i = 0; i < 32; i++)
	    board->twins[i] = NULL;
	for (i = 0; i < MAX_WORK; i++)
	    board->histogram[i] = 0;

	board->lineup = 0;

	/* Allocate CCB: */
        board->ccb = kmalloc(sizeof(munich_ccb_t), GFP_KERNEL);
	if (board->ccb == NULL)
	{
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -ENOMEM;
	}
	memset((void *)board->ccb, 0, sizeof(munich_ccb_t));
	board->ccb->csa = virt_to_phys(board->ccb);
	ccb = board->ccb;
	for (i = 0; i < 32; i++)
	{
	    ccb->timeslot_spec[i].tti = 1;
	    ccb->timeslot_spec[i].rti = 1;
	}

	/* Interrupt queues: */

	board->tiq = kmalloc(MUNICH_INTQSIZE, GFP_KERNEL);
	if (board->tiq == NULL)
	{
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -ENOMEM;
	}
	memset((void *)board->tiq, 0, MUNICH_INTQSIZE);

	board->riq = kmalloc(MUNICH_INTQSIZE, GFP_KERNEL);
	if (board->riq == NULL)
	{
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -ENOMEM;
	}
	memset((void *)board->riq, 0, MUNICH_INTQSIZE);

	board->piq = kmalloc(MUNICH_PIQSIZE, GFP_KERNEL);
	if (board->piq == NULL)
	{
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -ENOMEM;
	}
	memset((void *)board->piq, 0, MUNICH_PIQSIZE);

	board->tiq_ptr = 0;
	board->riq_ptr = 0;
	board->piq_ptr = 0;

	/* Request irq: */

	board->irq = 0;

	/* (char*) cast to avoid warning about discarding volatile:             */
	if (request_irq(board->pci->irq, MUNICH_interrupt, 0,
	    (char *)board->devname, (void *)board))
	{
	    printk("MUNICH_open: %s: unable to obtain irq %d\n", board->devname,
		   board->pci->irq);
	    /* TOD: free other resources (a sok malloc feljebb)                     */
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -EAGAIN;
	}
	board->irq = board->pci->irq;	/* csak akkor legyen != 0, ha tenyleg le van foglalva nekunk */

	/* Programming device: */

	/* Reset the board like a power-on: */
	/* TOD:
	   - It is not a real power-on: if a DMA transaction fails with master abort, the board
	   stays in half-dead state.
	   - It doesn't reset the FALC line driver */

	pci_write_config_dword(board->pci, MUNICH_PCI_PCIRES, 0xe0000);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1);
	pci_write_config_dword(board->pci, MUNICH_PCI_PCIRES, 0);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1);

        writel(virt_to_phys(&ccb->csa), MUNICH_VIRT(CCBA));
        writel(virt_to_phys( board->tiq ), MUNICH_VIRT(TIQBA));
        writel(MUNICH_INTQLEN, MUNICH_VIRT(TIQL));
        writel(virt_to_phys( board->riq ), MUNICH_VIRT(RIQBA));
        writel(MUNICH_INTQLEN, MUNICH_VIRT(RIQL));
        writel(virt_to_phys( board->piq ), MUNICH_VIRT(PIQBA));
        writel(MUNICH_PIQLEN, MUNICH_VIRT(PIQL));
        
	/* Put the magic values into the registers: */

	writel(MODE1_MAGIC, MUNICH_VIRT(MODE1));
	writel(MODE2_MAGIC, MUNICH_VIRT(MODE2));

	writel(LREG0_MAGIC, MUNICH_VIRT(LREG0));
	writel(LREG1_MAGIC, MUNICH_VIRT(LREG1));
	writel(LREG2_MAGIC, MUNICH_VIRT(LREG2));
	writel(LREG3_MAGIC, MUNICH_VIRT(LREG3));
	writel(LREG4_MAGIC, MUNICH_VIRT(LREG4));
	writel(LREG5_MAGIC, MUNICH_VIRT(LREG5));

	writel(LCONF_MAGIC1, MUNICH_VIRT(LCONF));	/* reset the DMSM */
	writel(LCONF_MAGIC2, MUNICH_VIRT(LCONF));	/* enable the DMSM */

	writel(~0, MUNICH_VIRT(TXPOLL));
	writel(board->isx21 ? 0x1400 : 0xa000, MUNICH_VIRT(GPDIR));

	if (readl(MUNICH_VIRT(STAT))) writel(readl(MUNICH_VIRT(STAT)), MUNICH_VIRT(STAT));

	ccb->action_spec = CCB_ACTIONSPEC_RES | CCB_ACTIONSPEC_IA;
	writel(CMD_ARPCM, MUNICH_VIRT(CMD));	/* Start the PCM core reset */
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1);

	stat = 0;		/* Wait for the action to complete max. 1 second */
	jiffs = jiffies;
	while (!((stat = readl(MUNICH_VIRT(STAT))) & (STAT_PCMA | STAT_PCMF)) && time_before(jiffies, jiffs + HZ))
	{
	    set_current_state(TASK_UNINTERRUPTIBLE);
	    schedule_timeout(1);
	}

	if (stat & STAT_PCMF)
	{
	    printk(KERN_ERR
		   "MUNICH_open: %s: Initial ARPCM failed. STAT=0x%08x\n",
		   board->devname, stat);
	    writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMF, MUNICH_VIRT(STAT));
	    free_irq(board->irq, (void *)board);	/* TOD: free other resources too *//* maybe shut down hw? */
	    board->irq = 0;
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -EAGAIN;
	}
	else if (!(stat & STAT_PCMA))
	{
	    printk(KERN_ERR
		   "MUNICH_open: %s: Initial ARPCM timeout. STAT=0x%08x\n",
		   board->devname, stat);
	    free_irq(board->irq, (void *)board);	/* TOD: free other resources too *//* maybe shut off the hw? */
	    board->irq = 0;
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -EIO;
	}

	writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMA, MUNICH_VIRT(STAT));	/* Acknowledge */

	if (board->isx21) writel(0, MUNICH_VIRT(GPDATA));

	printk("MUNICH_open: %s: succesful HW-open took %ld jiffies\n",
	       board->devname, jiffies - jiffs);

	/* Set up the FALC hanging on the Local Bus: */

	if (!board->isx21)
	{
	    writeb(0x0e, lbi + FMR1);
	    writeb(0, lbi + LIM0);
	    writeb(0xb0, lbi + LIM1);	/* TODO: input threshold */
	    writeb(0xf7, lbi + XPM0);
	    writeb(0x02, lbi + XPM1);
	    writeb(0x00, lbi + XPM2);
	    writeb(0xf0, lbi + FMR0);
	    writeb(0x80, lbi + PCD);
	    writeb(0x80, lbi + PCR);
	    writeb(0x00, lbi + LIM2);
	    writeb(0x07, lbi + XC0);
	    writeb(0x3d, lbi + XC1);
	    writeb(0x05, lbi + RC0);
	    writeb(0x00, lbi + RC1);
	    writeb(0x83, lbi + FMR2);
	    writeb(0x9f, lbi + XSW);
	    writeb(0x0f, lbi + XSP);
	    writeb(0x00, lbi + TSWM);
	    writeb(0xe0, lbi + MODE);
	    writeb(0xff, lbi + IDLE);	/* Idle Code to send in unused timeslots        */
	    writeb(0x83, lbi + IPC);	/* interrupt query line mode: Push/pull output, active high     */
	    writeb(0xbf, lbi + IMR3);	/* send an interrupt every second               */

	    slicecom_set_framing(hw->boardnum, board->framing);
	    slicecom_set_linecode(hw->boardnum, board->linecode);
	    slicecom_set_clock_source(hw->boardnum, board->clock_source);
	    slicecom_set_loopback(hw->boardnum, board->loopback);

	    memset((void *)board->intervals, 0, sizeof(board->intervals));
	    board->current_interval = 0;
	    board->elapsed_seconds = 0;
	    board->ses_seconds = 0;
	    board->is_unavailable = 0;
	    board->no_ses_seconds = 0;
	    board->deg_elapsed_seconds = 0;
	    board->deg_cumulated_errors = 0;
	}

	/* Enable the interrupts last                                                   */
	/* These interrupts will be enabled. We do not need the others. */

	writel(readl(MUNICH_VIRT(IMASK)) & ~(STAT_PTI | STAT_PRI | STAT_LBII | STAT_TSPA | STAT_RSPA), MUNICH_VIRT(IMASK));
    }

    spin_unlock_irqrestore(&mister_lock, flags);

    dev->irq = board->irq;	/* hogy szep legyen az ifconfig outputja */
    ccb = board->ccb;		/* TODO: ez igy csunya egy kicsit hogy benn is meg kinn is beletoltom :( */

    spin_lock_irqsave(&mister_lock, flags);

    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(1);

    /* Check if the selected timeslots aren't used already */

    for (i = 0; i < 32; i++)
	if (((1 << i) & timeslots) && !ccb->timeslot_spec[i].tti)
	{
	    printk("MUNICH_open: %s: timeslot %d already used by %s\n",
		   dev->name, i, board->twins[ccb->timeslot_spec[i].txchannel]->name);
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -EBUSY;	/* TODO: lehet hogy valami mas errno kellene? */
	}

    /* find a free channel: */
    /* TODO: ugly, rewrite it  */

    for (channel = 0; channel <= 32; channel++)
    {
	if (channel == 32)
	{			/* not found a free one */
	    printk
		("MUNICH_open: %s: FATAL: can not find a free channel - this should not happen!\n",
		 dev->name);
	    spin_unlock_irqrestore(&mister_lock, flags);
	    return -ENODEV;
	}
	if (board->twins[channel] == NULL)
	    break;		/* found the first free one */
    }

    board->lastcheck = jiffies;	/* avoid checking uninitialized hardware channel */

    /* Open the channel. If fails, calls MUNICH_close() to properly free resources and stop the HW */

    hw->channel = channel;
    board->twins[channel] = dev;

    board->use_count++;		/* meg nem nyitottuk meg a csatornat, de a twins-ben
				   mar elfoglaltunk egyet, es ha a _close-t akarjuk hivni, akkor ez kell. */
    for (i = 0; i < 32; i++)
	if ((1 << i) & timeslots)
	{
	    ccb->timeslot_spec[i].tti = 0;
	    ccb->timeslot_spec[i].txchannel = channel;
	    ccb->timeslot_spec[i].txfillmask = ~0;

	    ccb->timeslot_spec[i].rti = 0;
	    ccb->timeslot_spec[i].rxchannel = channel;
	    ccb->timeslot_spec[i].rxfillmask = ~0;
	}

    if (!board->isx21) rework_idle_channels(dev);

    memset((void *)&(hw->tx_desc), 0, TX_DESC_MAX * sizeof(tx_desc_t));
    memset((void *)&(hw->rx_desc), 0, RX_DESC_MAX * sizeof(rx_desc_t));

    for (i = 0; i < TX_DESC_MAX; i++)
    {
	hw->tx_desc[i].fe = 1;
	hw->tx_desc[i].fnum = 2;
                hw->tx_desc[i].data     = virt_to_phys( & (hw->tx_data[i][0]) );
                hw->tx_desc[i].next     = virt_to_phys( & (hw->tx_desc[ (i+1) % TX_DESC_MAX ]) );

    }
    hw->tx_desc_ptr = 0;	/* we will send an initial packet so it is correct: "oda irtunk utoljara" */
    hw->busy = 0;
    hw->tx_desc[hw->tx_desc_ptr].hold = 1;
    hw->tx_desc[hw->tx_desc_ptr].no = 1;	/* TOD: inkabb csak 0 hosszut kuldjunk ki az initkor? */

    for (i = 0; i < RX_DESC_MAX; i++)
    {
	hw->rx_desc[i].no = RXBUFFER_SIZE;
	hw->rx_desc[i].data = virt_to_phys(&(hw->rx_data[i][0]));
	hw->rx_desc[i].next = virt_to_phys(&(hw->rx_desc[(i+1) % RX_DESC_MAX]));
	hw->rx_desc[i].status = 0xFF;
    }
    hw->rx_desc_ptr = 0;

    hw->rx_desc[(hw->rx_desc_ptr + RX_DESC_MAX - 2) % RX_DESC_MAX].hold = 1;

    memset((void *)&ccb->channel_spec[channel], 0, sizeof(channel_spec_t));

    ccb->channel_spec[channel].ti = 0;	/* Transmit off */
    ccb->channel_spec[channel].to = 1;
    ccb->channel_spec[channel].ta = 0;

    ccb->channel_spec[channel].th = 1;	/* Transmit hold        */

    ccb->channel_spec[channel].ri = 0;	/* Receive off  */
    ccb->channel_spec[channel].ro = 1;
    ccb->channel_spec[channel].ra = 0;

    ccb->channel_spec[channel].mode = 3;	/* HDLC */

    ccb->action_spec = CCB_ACTIONSPEC_IN | (channel << 8);
    writel(CMD_ARPCM, MUNICH_VIRT(CMD));
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(1);

    spin_unlock_irqrestore(&mister_lock, flags);

    stat = 0;
    jiffs = jiffies;
    while (!((stat = readl(MUNICH_VIRT(STAT))) & (STAT_PCMA | STAT_PCMF)) && time_before(jiffies, jiffs + HZ))
    {
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1);
    }

    if (stat & STAT_PCMF)
    {
	printk(KERN_ERR "MUNICH_open: %s: %s channel %d off failed\n",
	       dev->name, board->devname, channel);
	writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMF, MUNICH_VIRT(STAT));
	MUNICH_close(dev);
	return -EAGAIN;
    }
    else if (!(stat & STAT_PCMA))
    {
	printk(KERN_ERR "MUNICH_open: %s: %s channel %d off timeout\n",
	       dev->name, board->devname, channel);
	MUNICH_close(dev);
	return -EIO;
    }

    writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMA, MUNICH_VIRT(STAT));
    //      printk("MUNICH_open: %s: succesful channel off took %ld jiffies\n", board->devname, jiffies-jiffs);

    spin_lock_irqsave(&mister_lock, flags);

    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(1);

    ccb->channel_spec[channel].ifc = 1;	/* 1 .. 'Idle/Flag change' interrupt letiltva   */
    ccb->channel_spec[channel].fit = 1;
    ccb->channel_spec[channel].nitbs = 1;
    ccb->channel_spec[channel].itbs = 2;

    /* TODOO: lehet hogy jo lenne igy, de utana kellene nezni hogy nem okoz-e fragmentaciot */
    //      ccb->channel_spec[channel].itbs = 2 * number_of_timeslots;
    //      printk("open: %s: number_of_timeslots: %d\n", dev->name, number_of_timeslots);

    ccb->channel_spec[channel].mode = 3;	/* HDLC */
    ccb->channel_spec[channel].ftda = virt_to_phys(&(hw->tx_desc));
    ccb->channel_spec[channel].frda = virt_to_phys(&(hw->rx_desc[0]));

    ccb->channel_spec[channel].ti = 1;	/* Transmit init        */
    ccb->channel_spec[channel].to = 0;
    ccb->channel_spec[channel].ta = 1;

    ccb->channel_spec[channel].th = 0;

    ccb->channel_spec[channel].ri = 1;	/* Receive init */
    ccb->channel_spec[channel].ro = 0;
    ccb->channel_spec[channel].ra = 1;

    ccb->action_spec = CCB_ACTIONSPEC_ICO | (channel << 8);
    writel(CMD_ARPCM, MUNICH_VIRT(CMD));	/* Start the channel init */
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(1);

    spin_unlock_irqrestore(&mister_lock, flags);

    stat = 0;			/* Wait for the action to complete max. 1 second */
    jiffs = jiffies;
    while (!((stat = readl(MUNICH_VIRT(STAT))) & (STAT_PCMA | STAT_PCMF)) && time_before(jiffies, jiffs + HZ))
    {
	set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1);
    }

    if (stat & STAT_PCMF)
    {
	printk(KERN_ERR "MUNICH_open: %s: channel open ARPCM failed\n",
	       board->devname);
	writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMF, MUNICH_VIRT(STAT));
	MUNICH_close(dev);
	return -EAGAIN;
    }
    else if (!(stat & STAT_PCMA))
    {
	printk(KERN_ERR "MUNICH_open: %s: channel open ARPCM timeout\n",
	       board->devname);
	MUNICH_close(dev);
	return -EIO;
    }

    writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMA, MUNICH_VIRT(STAT));
    //      printk("MUNICH_open: %s: succesful channel open took %ld jiffies\n", board->devname, jiffies-jiffs);

    spin_lock_irqsave(&mister_lock, flags);

    ccb->channel_spec[channel].nitbs = 0;	/* once ITBS defined, these must be 0   */
    ccb->channel_spec[channel].itbs = 0;

    if (board->isx21)
    {
	board->modemline_timer.data = (unsigned long)board;
	board->modemline_timer.function = pcicom_modemline;
	board->modemline_timer.expires = jiffies + HZ;
	add_timer((struct timer_list *)&board->modemline_timer);
    }

    /* It is done. Declare that we're open: */
    hw->busy = 0;		/* It may be 1 if the frame at Tx init already ended, but it is not     */
    /* a real problem: we compute hw->busy on every interrupt                       */
    hw->rafutott = 0;
    ch->init_status |= HW_OPEN;

    /* Initialize line state: */
    if (board->lineup)
	ch->line_status |= LINE_UP;
    else
	ch->line_status &= ~LINE_UP;

    /* Remove w attribute from /proc files associated to hw parameters:
       no write when the device is open */

    for (; procfile; procfile = procfile->next)
	if (strcmp(procfile->name, FILENAME_BOARDNUM) == 0 ||
	    strcmp(procfile->name, FILENAME_TIMESLOTS) == 0)
	    procfile->mode = S_IFREG | 0444;

    spin_unlock_irqrestore(&mister_lock, flags);

    return 0;
}

/*
 * Hardware close routine.
 * Called by comx (upper) layer when the user wants to bring down the interface
 * with ifconfig.
 * We also call it from MUNICH_open, if the open fails.
 * Brings down hardware, frees resources, stops receiver
 * Returns 0 on OK, or standard error value on error.
 */

static int MUNICH_close(struct net_device *dev)
{
    struct comx_channel *ch = dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;
    struct proc_dir_entry *procfile = ch->procdir->subdir;
    munich_board_t *board;
    munich_ccb_t *ccb;

    u32 *bar1;
    u32 timeslots = hw->timeslots;
    int stat, i, channel = hw->channel;
    unsigned long jiffs;

    board = hw->boardnum + (ch->hardware == &pcicomhw ? pcicom_boards : slicecom_boards);

    ccb = board->ccb;
    bar1 = board->bar1;

    if (board->isx21)
	del_timer((struct timer_list *)&board->modemline_timer);

    spin_lock_irqsave(&mister_lock, flags);

    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(1);

    /* Disable receiver for the channel: */

    for (i = 0; i < 32; i++)
	if ((1 << i) & timeslots)
	{
	    ccb->timeslot_spec[i].tti = 1;
	    ccb->timeslot_spec[i].txfillmask = 0;	/* just to be double-sure :) */

	    ccb->timeslot_spec[i].rti = 1;
	    ccb->timeslot_spec[i].rxfillmask = 0;
	}

    if (!board->isx21) rework_idle_channels(dev);

    ccb->channel_spec[channel].ti = 0;	/* Receive off, Transmit off */
    ccb->channel_spec[channel].to = 1;
    ccb->channel_spec[channel].ta = 0;
    ccb->channel_spec[channel].th = 1;

    ccb->channel_spec[channel].ri = 0;
    ccb->channel_spec[channel].ro = 1;
    ccb->channel_spec[channel].ra = 0;

    board->twins[channel] = NULL;

    ccb->action_spec = CCB_ACTIONSPEC_IN | (channel << 8);
    writel(CMD_ARPCM, MUNICH_VIRT(CMD));
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(1);

    spin_unlock_irqrestore(&mister_lock, flags);

    stat = 0;
    jiffs = jiffies;
    while (!((stat = readl(MUNICH_VIRT(STAT))) & (STAT_PCMA | STAT_PCMF)) && time_before(jiffies, jiffs + HZ))
    {
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(1);
    }

    if (stat & STAT_PCMF)
    {
	printk(KERN_ERR
	       "MUNICH_close: %s: FATAL: channel off ARPCM failed, not closing!\n",
	       dev->name);
	writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMF, MUNICH_VIRT(STAT));
	/* If we return success, the privdata (and the descriptor list) will be freed */
	return -EIO;
    }
    else if (!(stat & STAT_PCMA))
	printk(KERN_ERR "MUNICH_close: %s: channel off ARPCM timeout\n",
	       board->devname);

    writel(readl(MUNICH_VIRT(STAT)) & STAT_PCMA, MUNICH_VIRT(STAT));
    //      printk("MUNICH_close: %s: channel off took %ld jiffies\n", board->devname, jiffies-jiffs);

    spin_lock_irqsave(&mister_lock, flags);

    if (board->use_count) board->use_count--;

    if (!board->use_count)	/* we were the last user of the board */
    {
	printk("MUNICH_close: bringing down board %s\n", board->devname);

	/* program down the board: */

	writel(0x0000FF7F, MUNICH_VIRT(IMASK));	/* do not send any interrupts */
	writel(0, MUNICH_VIRT(CMD));	/* stop the timer if someone started it */
	writel(~0U, MUNICH_VIRT(STAT));	/* if an interrupt came between the cli()-sti(), quiet it */
	if (ch->hardware == &pcicomhw)
	    writel(0x1400, MUNICH_VIRT(GPDATA));

	/* Put the board into 'reset' state: */
	pci_write_config_dword(board->pci, MUNICH_PCI_PCIRES, 0xe0000);

	/* Free irq and other resources: */
	if (board->irq)
	    free_irq(board->irq, (void *)board);	/* Ha nem inicializalta magat, akkor meg nincs irq */
	board->irq = 0;

	/* Free CCB and the interrupt queues */
	if (board->ccb) kfree((void *)board->ccb);
	if (board->tiq) kfree((void *)board->tiq);
	if (board->riq) kfree((void *)board->riq);
	if (board->piq) kfree((void *)board->piq);
	board->ccb = NULL;
	board->tiq = board->riq = board->piq = NULL;
    }

    /* Enable setting of hw parameters */
    for (; procfile; procfile = procfile->next)
	if (strcmp(procfile->name, FILENAME_BOARDNUM) == 0 ||
	    strcmp(procfile->name, FILENAME_TIMESLOTS) == 0)
	    procfile->mode = S_IFREG | 0644;

    /* We're not open anymore */
    ch->init_status &= ~HW_OPEN;

    spin_unlock_irqrestore(&mister_lock, flags);

    return 0;
}

/* 
 * Give (textual) status information.
 * The text it returns will be a part of what appears when the user does a
 * cat /proc/comx/comx[n]/status 
 * Don't write more than PAGESIZE.
 * Return value: number of bytes written (length of the string, incl. 0)
 */

static int MUNICH_minden(struct net_device *dev, char *page)
{
    struct comx_channel *ch = dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;
    munich_board_t *board;
    struct net_device *devp;

    u8 *lbi;
    e1_stats_t *curr_int, *prev_int;
    e1_stats_t last4, last96;	/* sum of last 4, resp. last 96 intervals               */
    unsigned *sump,		/* running pointer for the sum data                     */
     *p;			/* running pointer for the interval data                */

    int len = 0;
    u8 frs0, frs1;
    u8 fmr2;
    int i, j;
    u32 timeslots;

    board = hw->boardnum + (ch->hardware == &pcicomhw ? pcicom_boards : slicecom_boards);

    lbi = board->lbi;
    curr_int = &board->intervals[board->current_interval];
    prev_int =
	&board->
	intervals[(board->current_interval + SLICECOM_BOARD_INTERVALS_SIZE -
		   1) % SLICECOM_BOARD_INTERVALS_SIZE];

    if (!board->isx21)
    {
	frs0 = readb(lbi + FRS0);
	fmr2 = readb(lbi + FMR2);
	len += snprintf(page + len, PAGE_SIZE - len, "Controller status:\n");
	if (frs0 == 0)
	    len += snprintf(page + len, PAGE_SIZE - len, "\tNo alarms\n");
	else
	{
	    if (frs0 & FRS0_LOS)
	            len += snprintf(page + len, PAGE_SIZE - len, "\tLoss Of Signal\n");
	    else
	    {
		if (frs0 & FRS0_AIS)
		    len += snprintf(page + len, PAGE_SIZE - len,
				 "\tAlarm Indication Signal\n");
		else
		{
		    if (frs0 & FRS0_AUXP)
			len += snprintf(page + len, PAGE_SIZE - len,
				     "\tAuxiliary Pattern Indication\n");
		    if (frs0 & FRS0_LFA)
			len += snprintf(page + len, PAGE_SIZE - len,
				     "\tLoss of Frame Alignment\n");
		    else
		    {
			if (frs0 & FRS0_RRA)
			    len += snprintf(page + len, PAGE_SIZE - len,
					 "\tReceive Remote Alarm\n");

			/* You can't set this framing with the /proc interface, but it  */
			/* may be good to have here this alarm if you set it by hand:   */

			if ((board->framing == SLICECOM_FRAMING_CRC4) &&
			    (frs0 & FRS0_LMFA))
			    len += snprintf(page + len, PAGE_SIZE - len,
					 "\tLoss of CRC4 Multiframe Alignment\n");

			if (((fmr2 & 0xc0) == 0xc0) && (frs0 & FRS0_NMF))
			    len += snprintf(page + len, PAGE_SIZE - len,
				 "\tNo CRC4 Multiframe alignment Found after 400 msec\n");
		    }
		}
	    }
	}

	frs1 = readb(lbi + FRS1);
	if (FRS1_XLS & frs1)
	    len += snprintf(page + len, PAGE_SIZE - len,
		 "\tTransmit Line Short\n");

	/* debug Rx ring: DEL: - vagy meghagyni, de akkor legyen kicsit altalanosabb */
    }

    len += snprintf(page + len, PAGE_SIZE - len, "Rx ring:\n");
    len += snprintf(page + len, PAGE_SIZE - len, "\trafutott: %d\n", hw->rafutott);
    len += snprintf(page + len, PAGE_SIZE - len,
		 "\tlastcheck: %ld, jiffies: %ld\n", board->lastcheck, jiffies);
    len += snprintf(page + len, PAGE_SIZE - len, "\tbase: %08x\n",
	(u32) virt_to_phys(&hw->rx_desc[0]));
    len += snprintf(page + len, PAGE_SIZE - len, "\trx_desc_ptr: %d\n",
		 hw->rx_desc_ptr);
    len += snprintf(page + len, PAGE_SIZE - len, "\trx_desc_ptr: %08x\n",
	(u32) virt_to_phys(&hw->rx_desc[hw->rx_desc_ptr]));
    len += snprintf(page + len, PAGE_SIZE - len, "\thw_curr_ptr: %08x\n",
		 board->ccb->current_rx_desc[hw->channel]);

    for (i = 0; i < RX_DESC_MAX; i++)
	len += snprintf(page + len, PAGE_SIZE - len, "\t%08x %08x %08x %08x\n",
		     *((u32 *) & hw->rx_desc[i] + 0),
		     *((u32 *) & hw->rx_desc[i] + 1),
		     *((u32 *) & hw->rx_desc[i] + 2),
		     *((u32 *) & hw->rx_desc[i] + 3));

    if (!board->isx21)
    {
	len += snprintf(page + len, PAGE_SIZE - len,
		     "Interfaces using this board: (channel-group, interface, timeslots)\n");
	for (i = 0; i < 32; i++)
	{
	    devp = board->twins[i];
	    if (devp != NULL)
	    {
		timeslots =
		    ((struct slicecom_privdata *)((struct comx_channel *)devp->
						  priv)->HW_privdata)->
		    timeslots;
		len += snprintf(page + len, PAGE_SIZE - len, "\t%2d %s: ", i,
			     devp->name);
		for (j = 0; j < 32; j++)
		    if ((1 << j) & timeslots)
			len += snprintf(page + len, PAGE_SIZE - len, "%d ", j);
		len += snprintf(page + len, PAGE_SIZE - len, "\n");
	    }
	}
    }

    len += snprintf(page + len, PAGE_SIZE - len, "Interrupt work histogram:\n");
    for (i = 0; i < MAX_WORK; i++)
	len += snprintf(page + len, PAGE_SIZE - len, "hist[%2d]: %8u%c", i,
		     board->histogram[i], (i &&
					   ((i + 1) % 4 == 0 ||
					    i == MAX_WORK - 1)) ? '\n' : ' ');

    len += snprintf(page + len, PAGE_SIZE - len, "Tx ring histogram:\n");
    for (i = 0; i < TX_DESC_MAX; i++)
	len += snprintf(page + len, PAGE_SIZE - len, "hist[%2d]: %8u%c", i,
		     hw->tx_ring_hist[i], (i &&
					   ((i + 1) % 4 == 0 ||
					    i ==
					    TX_DESC_MAX - 1)) ? '\n' : ' ');

    if (!board->isx21)
    {

	memset((void *)&last4, 0, sizeof(last4));
	memset((void *)&last96, 0, sizeof(last96));

	/* Calculate the sum of last 4 intervals: */

	for (i = 1; i <= 4; i++)
	{
	    p = (unsigned *)&board->intervals[(board->current_interval +
			   SLICECOM_BOARD_INTERVALS_SIZE -
			   i) % SLICECOM_BOARD_INTERVALS_SIZE];
	    sump = (unsigned *)&last4;
	    for (j = 0; j < (sizeof(e1_stats_t) / sizeof(unsigned)); j++)
		sump[j] += p[j];
	}

	/* Calculate the sum of last 96 intervals: */

	for (i = 1; i <= 96; i++)
	{
	    p = (unsigned *)&board->intervals[(board->current_interval +
			   SLICECOM_BOARD_INTERVALS_SIZE -
			   i) % SLICECOM_BOARD_INTERVALS_SIZE];
	    sump = (unsigned *)&last96;
	    for (j = 0; j < (sizeof(e1_stats_t) / sizeof(unsigned)); j++)
		sump[j] += p[j];
	}

	len += snprintf(page + len, PAGE_SIZE - len,
		     "Data in current interval (%d seconds elapsed):\n",
		     board->elapsed_seconds);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Line Code Violations, %d Path Code Violations, %d E-Bit Errors\n",
		     curr_int->line_code_violations,
		     curr_int->path_code_violations, curr_int->e_bit_errors);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Slip Secs, %d Fr Loss Secs, %d Line Err Secs, %d Degraded Mins\n",
		     curr_int->slip_secs, curr_int->fr_loss_secs,
		     curr_int->line_err_secs, curr_int->degraded_mins);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Errored Secs, %d Bursty Err Secs, %d Severely Err Secs, %d Unavail Secs\n",
		     curr_int->errored_secs, curr_int->bursty_err_secs,
		     curr_int->severely_err_secs, curr_int->unavail_secs);

	len += snprintf(page + len, PAGE_SIZE - len,
		     "Data in Interval 1 (15 minutes):\n");
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Line Code Violations, %d Path Code Violations, %d E-Bit Errors\n",
		     prev_int->line_code_violations,
		     prev_int->path_code_violations, prev_int->e_bit_errors);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Slip Secs, %d Fr Loss Secs, %d Line Err Secs, %d Degraded Mins\n",
		     prev_int->slip_secs, prev_int->fr_loss_secs,
		     prev_int->line_err_secs, prev_int->degraded_mins);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Errored Secs, %d Bursty Err Secs, %d Severely Err Secs, %d Unavail Secs\n",
		     prev_int->errored_secs, prev_int->bursty_err_secs,
		     prev_int->severely_err_secs, prev_int->unavail_secs);

	len += snprintf(page + len, PAGE_SIZE - len,
		     "Data in last 4 intervals (1 hour):\n");
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Line Code Violations, %d Path Code Violations, %d E-Bit Errors\n",
		     last4.line_code_violations, last4.path_code_violations,
		     last4.e_bit_errors);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Slip Secs, %d Fr Loss Secs, %d Line Err Secs, %d Degraded Mins\n",
		     last4.slip_secs, last4.fr_loss_secs, last4.line_err_secs,
		     last4.degraded_mins);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Errored Secs, %d Bursty Err Secs, %d Severely Err Secs, %d Unavail Secs\n",
		     last4.errored_secs, last4.bursty_err_secs,
		     last4.severely_err_secs, last4.unavail_secs);

	len += snprintf(page + len, PAGE_SIZE - len,
		     "Data in last 96 intervals (24 hours):\n");
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Line Code Violations, %d Path Code Violations, %d E-Bit Errors\n",
		     last96.line_code_violations, last96.path_code_violations,
		     last96.e_bit_errors);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Slip Secs, %d Fr Loss Secs, %d Line Err Secs, %d Degraded Mins\n",
		     last96.slip_secs, last96.fr_loss_secs,
		     last96.line_err_secs, last96.degraded_mins);
	len += snprintf(page + len, PAGE_SIZE - len,
		     "   %d Errored Secs, %d Bursty Err Secs, %d Severely Err Secs, %d Unavail Secs\n",
		     last96.errored_secs, last96.bursty_err_secs,
		     last96.severely_err_secs, last96.unavail_secs);

    }

//      len +=snprintf( page + len, PAGE_SIZE - len, "Special events:\n" );
//      len +=snprintf( page + len, PAGE_SIZE - len, "\tstat_pri/missed: %u / %u\n", board->stat_pri_races, board->stat_pri_races_missed );
//      len +=snprintf( page + len, PAGE_SIZE - len, "\tstat_pti/missed: %u / %u\n", board->stat_pti_races, board->stat_pti_races_missed );
    return len;
}

/*
 * Memory dump function. Not used currently.
 */
static int BOARD_dump(struct net_device *dev)
{
    printk
	("BOARD_dump() requested. It is unimplemented, it should not be called\n");
    return (-1);
}

/* 
 * /proc file read function for the files registered by this module.
 * This function is called by the procfs implementation when a user
 * wants to read from a file registered by this module.
 * page is the workspace, start should point to the real start of data,
 * off is the file offset, data points to the file's proc_dir_entry
 * structure.
 * Returns the number of bytes copied to the request buffer.
 */

static int munich_read_proc(char *page, char **start, off_t off, int count,
			    int *eof, void *data)
{
    struct proc_dir_entry *file = (struct proc_dir_entry *)data;
    struct net_device *dev = file->parent->data;
    struct comx_channel *ch = dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;
    munich_board_t *board;

    int len = 0, i;
    u32 timeslots = hw->timeslots;

    board = hw->boardnum + (ch->hardware == &pcicomhw ? pcicom_boards : slicecom_boards);

    if (!strcmp(file->name, FILENAME_BOARDNUM))
	len = sprintf(page, "%d\n", hw->boardnum);
    else if (!strcmp(file->name, FILENAME_TIMESLOTS))
    {
	for (i = 0; i < 32; i++)
	    if ((1 << i) & timeslots)
		len += snprintf(page + len, PAGE_SIZE - len, "%d ", i);
	len += snprintf(page + len, PAGE_SIZE - len, "\n");
    }
    else if (!strcmp(file->name, FILENAME_FRAMING))
    {
	i = 0;
	while (slicecom_framings[i].value &&
	       slicecom_framings[i].value != board->framing)
	    i++;
	len += snprintf(page + len, PAGE_SIZE - len, "%s\n",
		     slicecom_framings[i].name);
    }
    else if (!strcmp(file->name, FILENAME_LINECODE))
    {
	i = 0;
	while (slicecom_linecodes[i].value &&
	       slicecom_linecodes[i].value != board->linecode)
	    i++;
	len += snprintf(page + len, PAGE_SIZE - len, "%s\n",
		     slicecom_linecodes[i].name);
    }
    else if (!strcmp(file->name, FILENAME_CLOCK_SOURCE))
    {
	i = 0;
	while (slicecom_clock_sources[i].value &&
	       slicecom_clock_sources[i].value != board->clock_source)
	    i++;
	len +=
	    snprintf(page + len, PAGE_SIZE - len, "%s\n",
		     slicecom_clock_sources[i].name);
    }
    else if (!strcmp(file->name, FILENAME_LOOPBACK))
    {
	i = 0;
	while (slicecom_loopbacks[i].value &&
	       slicecom_loopbacks[i].value != board->loopback)
	    i++;
	len += snprintf(page + len, PAGE_SIZE - len, "%s\n",
		     slicecom_loopbacks[i].name);
    }
    /* We set permissions to write-only for REG and LBIREG, but root can read them anyway: */
    else if (!strcmp(file->name, FILENAME_REG))
    {
	len += snprintf(page + len, PAGE_SIZE - len,
		     "%s: " FILENAME_REG ": write-only file\n", dev->name);
    }
    else if (!strcmp(file->name, FILENAME_LBIREG))
    {
	len += snprintf(page + len, PAGE_SIZE - len,
		     "%s: " FILENAME_LBIREG ": write-only file\n", dev->name);
    }
    else
    {
	printk("slicecom_read_proc: internal error, filename %s\n", file->name);
	return -EBADF;
    }
    /* file handling administration: count eof status, offset, start address
       and count: */

    if (off >= len)
    {
	*eof = 1;
	return 0;
    }

    *start = page + off;
    if (count >= len - off)
	*eof = 1;
    return min((off_t) count, (off_t) len - off);
}

/* 
 * Write function for /proc files registered by us.
 * See the comment on read function above.
 * Beware! buffer is in userspace!!!
 * Returns the number of bytes written
 */

static int munich_write_proc(struct file *file, const char *buffer,
			     u_long count, void *data)
{
    struct proc_dir_entry *entry = (struct proc_dir_entry *)data;
    struct net_device *dev = (struct net_device *)entry->parent->data;
    struct comx_channel *ch = dev->priv;
    struct slicecom_privdata *hw = ch->HW_privdata;
    munich_board_t *board;

    unsigned long ts, tmp_boardnum;

    u32 tmp_timeslots = 0;
    char *page, *p;
    int i;

    board = hw->boardnum + (ch->hardware == &pcicomhw ? pcicom_boards : slicecom_boards);

    /* Paranoia checking: */

    if (file->f_dentry->d_inode->i_ino != entry->low_ino)
    {
	printk(KERN_ERR "munich_write_proc: file <-> data internal error\n");
	return -EIO;
    }

    /* Request tmp buffer */
    if (!(page = (char *)__get_free_page(GFP_KERNEL)))
	return -ENOMEM;

    /* Copy user data and cut trailing \n */
    copy_from_user(page, buffer, count = min(count, PAGE_SIZE));
    if (*(page + count - 1) == '\n')
	*(page + count - 1) = 0;
    *(page + PAGE_SIZE - 1) = 0;

    if (!strcmp(entry->name, FILENAME_BOARDNUM))
    {
	tmp_boardnum = simple_strtoul(page, NULL, 0);
	if (0 <= tmp_boardnum && tmp_boardnum < MAX_BOARDS)
	    hw->boardnum = tmp_boardnum;
	else
	{
	    printk("%s: " FILENAME_BOARDNUM " range is 0...%d\n", dev->name,
		   MAX_BOARDS - 1);
	    free_page((unsigned long)page);
	    return -EINVAL;
	}
    }
    else if (!strcmp(entry->name, FILENAME_TIMESLOTS))
    {
	p = page;
	while (*p)
	{
	    if (isspace(*p))
		p++;
	    else
	    {
		ts = simple_strtoul(p, &p, 10);	/* base = 10: Don't read 09 as an octal number */
		/* ts = 0 ha nem tudta beolvasni a stringet, erre egy kicsit epitek itt: */
		if (0 <= ts && ts < 32)
		{
		    tmp_timeslots |= (1 << ts);
		}
		else
		{
		    printk("%s: " FILENAME_TIMESLOTS " range is 1...31\n",
			   dev->name);
		    free_page((unsigned long)page);
		    return -EINVAL;
		}
	    }
	}
	hw->timeslots = tmp_timeslots;
    }
    else if (!strcmp(entry->name, FILENAME_FRAMING))
    {
	i = 0;
	while (slicecom_framings[i].value &&
	       strncmp(slicecom_framings[i].name, page,
		       strlen(slicecom_framings[i].name)))
	    i++;
	if (!slicecom_framings[i].value)
	{
	    printk("slicecom: %s: Invalid " FILENAME_FRAMING " '%s'\n",
		   dev->name, page);
	    free_page((unsigned long)page);
	    return -EINVAL;
	}
	else
	{			/*
				 * If somebody says:
				 *      echo >boardnum  0
				 *      echo >framing   no-crc4
				 *      echo >boardnum  1
				 * - when the framing was set, hw->boardnum was 0, so it would set the framing for board 0
				 * Workaround: allow to set it only if interface is administrative UP
				 */
	    if (netif_running(dev))
		slicecom_set_framing(hw->boardnum, slicecom_framings[i].value);
	    else
	    {
		printk("%s: " FILENAME_FRAMING
		       " can not be set while the interface is DOWN\n",
		       dev->name);
		free_page((unsigned long)page);
		return -EINVAL;
	    }
	}
    }
    else if (!strcmp(entry->name, FILENAME_LINECODE))
    {
	i = 0;
	while (slicecom_linecodes[i].value &&
	       strncmp(slicecom_linecodes[i].name, page,
		       strlen(slicecom_linecodes[i].name)))
	    i++;
	if (!slicecom_linecodes[i].value)
	{
	    printk("slicecom: %s: Invalid " FILENAME_LINECODE " '%s'\n",
		   dev->name, page);
	    free_page((unsigned long)page);
	    return -EINVAL;
	}
	else
	{			/*
				 * Allow to set it only if interface is administrative UP,
				 * for the same reason as FILENAME_FRAMING
				 */
	    if (netif_running(dev))
		slicecom_set_linecode(hw->boardnum,
				      slicecom_linecodes[i].value);
	    else
	    {
		printk("%s: " FILENAME_LINECODE
		       " can not be set while the interface is DOWN\n",
		       dev->name);
		free_page((unsigned long)page);
		return -EINVAL;
	    }
	}
    }
    else if (!strcmp(entry->name, FILENAME_CLOCK_SOURCE))
    {
	i = 0;
	while (slicecom_clock_sources[i].value &&
	       strncmp(slicecom_clock_sources[i].name, page,
		       strlen(slicecom_clock_sources[i].name)))
	    i++;
	if (!slicecom_clock_sources[i].value)
	{
	    printk("%s: Invalid " FILENAME_CLOCK_SOURCE " '%s'\n", dev->name,
		   page);
	    free_page((unsigned long)page);
	    return -EINVAL;
	}
	else
	{			/*
				 * Allow to set it only if interface is administrative UP,
				 * for the same reason as FILENAME_FRAMING
				 */
	    if (netif_running(dev))
		slicecom_set_clock_source(hw->boardnum,
					  slicecom_clock_sources[i].value);
	    else
	    {
		printk("%s: " FILENAME_CLOCK_SOURCE
		       " can not be set while the interface is DOWN\n",
		       dev->name);
		free_page((unsigned long)page);
		return -EINVAL;
	    }
	}
    }
    else if (!strcmp(entry->name, FILENAME_LOOPBACK))
    {
	i = 0;
	while (slicecom_loopbacks[i].value &&
	       strncmp(slicecom_loopbacks[i].name, page,
		       strlen(slicecom_loopbacks[i].name)))
	    i++;
	if (!slicecom_loopbacks[i].value)
	{
	    printk("%s: Invalid " FILENAME_LOOPBACK " '%s'\n", dev->name, page);
	    free_page((unsigned long)page);
	    return -EINVAL;
	}
	else
	{			/*
				 * Allow to set it only if interface is administrative UP,
				 * for the same reason as FILENAME_FRAMING
				 */
	    if (netif_running(dev))
		slicecom_set_loopback(hw->boardnum,
				      slicecom_loopbacks[i].value);
	    else
	    {
		printk("%s: " FILENAME_LOOPBACK
		       " can not be set while the interface is DOWN\n",
		       dev->name);
		free_page((unsigned long)page);
		return -EINVAL;
	    }
	}
    }
    else if (!strcmp(entry->name, FILENAME_REG))
    {				/* DEL: 'reg' csak tmp */
	char *p;
	u32 *bar1 = board->bar1;

	reg = simple_strtoul(page, &p, 0);
	reg_ertek = simple_strtoul(p + 1, NULL, 0);

	if (reg < 0x100)
	{
	    printk("reg(0x%02x) := 0x%08x  jiff: %lu\n", reg, reg_ertek, jiffies);
	    writel(reg_ertek, MUNICH_VIRT(reg >> 2));
	}
	else
	{
	    printk("reg(0x%02x) is 0x%08x  jiff: %lu\n", reg - 0x100,
		   readl(MUNICH_VIRT((reg - 0x100) >> 2)), jiffies);
	}
    }
    else if (!strcmp(entry->name, FILENAME_LBIREG))
    {				/* DEL: 'lbireg' csak tmp */
	char *p;
	u8 *lbi = board->lbi;

	lbireg = simple_strtoul(page, &p, 0);
	lbireg_ertek = simple_strtoul(p + 1, NULL, 0);

	if (lbireg < 0x100)
	{
	    printk("lbireg(0x%02x) := 0x%02x  jiff: %lu\n", lbireg,
		   lbireg_ertek, jiffies);
	    writeb(lbireg_ertek, lbi + lbireg);
	}
	else
	    printk("lbireg(0x%02x) is 0x%02x  jiff: %lu\n", lbireg - 0x100,
		   readb(lbi + lbireg - 0x100), jiffies);
    }
    else
    {
	printk(KERN_ERR "munich_write_proc: internal error, filename %s\n",
	       entry->name);
	free_page((unsigned long)page);
	return -EBADF;
    }

    /* Don't forget to free the workspace */
    free_page((unsigned long)page);
    return count;
}

/* 
 * Boardtype init function.
 * Called by the comx (upper) layer, when you set boardtype.
 * Allocates resources associated to using munich board for this device,
 * initializes ch_struct pointers etc.
 * Returns 0 on success and standard error codes on error.
 */

static int init_escape(struct comx_channel *ch)
{
    kfree(ch->HW_privdata);
    return -EIO;
}

static int BOARD_init(struct net_device *dev)
{
    struct comx_channel *ch = (struct comx_channel *)dev->priv;
    struct slicecom_privdata *hw;
    struct proc_dir_entry *new_file;

    /* Alloc data for private structure */
    if ((ch->HW_privdata =
	kmalloc(sizeof(struct slicecom_privdata), GFP_KERNEL)) == NULL)
        return -ENOMEM;
        
    memset(hw = ch->HW_privdata, 0, sizeof(struct slicecom_privdata));

    /* Register /proc files */
    if ((new_file = create_proc_entry(FILENAME_BOARDNUM, S_IFREG | 0644,
			   ch->procdir)) == NULL)
	return init_escape(ch);
    new_file->data = (void *)new_file;
    new_file->read_proc = &munich_read_proc;
    new_file->write_proc = &munich_write_proc;
//      new_file->proc_iops = &comx_normal_inode_ops;
    new_file->nlink = 1;

    if (ch->hardware == &slicecomhw)
    {
	if ((new_file = create_proc_entry(FILENAME_TIMESLOTS, S_IFREG | 0644,
			       ch->procdir)) == NULL)
	    return init_escape(ch);
	new_file->data = (void *)new_file;
	new_file->read_proc = &munich_read_proc;
	new_file->write_proc = &munich_write_proc;
//              new_file->proc_iops = &comx_normal_inode_ops;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_FRAMING, S_IFREG | 0644,
			       ch->procdir)) == NULL)
	    return init_escape(ch);
	new_file->data = (void *)new_file;
	new_file->read_proc = &munich_read_proc;
	new_file->write_proc = &munich_write_proc;
//              new_file->proc_iops = &comx_normal_inode_ops;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_LINECODE, S_IFREG | 0644,
			       ch->procdir)) == NULL)
	    return init_escape(ch);
	new_file->data = (void *)new_file;
	new_file->read_proc = &munich_read_proc;
	new_file->write_proc = &munich_write_proc;
//              new_file->proc_iops = &comx_normal_inode_ops;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_CLOCK_SOURCE, S_IFREG | 0644,
			       ch->procdir)) == NULL)
	    return init_escape(ch);
	new_file->data = (void *)new_file;
	new_file->read_proc = &munich_read_proc;
	new_file->write_proc = &munich_write_proc;
//              new_file->proc_iops = &comx_normal_inode_ops;
	new_file->nlink = 1;

	if ((new_file = create_proc_entry(FILENAME_LOOPBACK, S_IFREG | 0644,
			       ch->procdir)) == NULL)
	    return init_escape(ch);
	new_file->data = (void *)new_file;
	new_file->read_proc = &munich_read_proc;
	new_file->write_proc = &munich_write_proc;
//              new_file->proc_iops = &comx_normal_inode_ops;
	new_file->nlink = 1;
    }

    /* DEL: ez itt csak fejlesztesi celokra!! */
    if ((new_file = create_proc_entry(FILENAME_REG, S_IFREG | 0200, ch->procdir)) == NULL)
	return init_escape(ch);
    new_file->data = (void *)new_file;
    new_file->read_proc = &munich_read_proc;
    new_file->write_proc = &munich_write_proc;
//      new_file->proc_iops = &comx_normal_inode_ops;
    new_file->nlink = 1;

    /* DEL: ez itt csak fejlesztesi celokra!! */
    if ((new_file = create_proc_entry(FILENAME_LBIREG, S_IFREG | 0200,
			   ch->procdir)) == NULL)
	return init_escape(ch);
    new_file->data = (void *)new_file;
    new_file->read_proc = &munich_read_proc;
    new_file->write_proc = &munich_write_proc;
//      new_file->proc_iops = &comx_normal_inode_ops;
    new_file->nlink = 1;

    /* Fill in ch_struct hw specific pointers: */

    ch->HW_txe = MUNICH_txe;
    ch->HW_open = MUNICH_open;
    ch->HW_close = MUNICH_close;
    ch->HW_send_packet = MUNICH_send_packet;
#ifndef COMX_NEW
    ch->HW_minden = MUNICH_minden;
#else
    ch->HW_statistics = MUNICH_minden;
#endif

    hw->boardnum = SLICECOM_BOARDNUM_DEFAULT;
    hw->timeslots = ch->hardware == &pcicomhw ?  0xffffffff : 2;

    /* O.K. Count one more user on this module */
    MOD_INC_USE_COUNT;
    return 0;
}

/* 
 * Boardtype exit function.
 * Called by the comx (upper) layer, when you clear boardtype from munich.
 * Frees resources associated to using munich board for this device,
 * resets ch_struct pointers etc.
 */
static int BOARD_exit(struct net_device *dev)
{
    struct comx_channel *ch = (struct comx_channel *)dev->priv;

    /* Free private data area */
//    board = hw->boardnum + (ch->hardware == &pcicomhw ? pcicom_boards : slicecom_boards);

    kfree(ch->HW_privdata);
    /* Remove /proc files */
    remove_proc_entry(FILENAME_BOARDNUM, ch->procdir);
    if (ch->hardware == &slicecomhw)
    {
	remove_proc_entry(FILENAME_TIMESLOTS, ch->procdir);
	remove_proc_entry(FILENAME_FRAMING, ch->procdir);
	remove_proc_entry(FILENAME_LINECODE, ch->procdir);
	remove_proc_entry(FILENAME_CLOCK_SOURCE, ch->procdir);
	remove_proc_entry(FILENAME_LOOPBACK, ch->procdir);
    }
    remove_proc_entry(FILENAME_REG, ch->procdir);
    remove_proc_entry(FILENAME_LBIREG, ch->procdir);

    /* Minus one user for the module accounting */
    MOD_DEC_USE_COUNT;
    return 0;
}

static struct comx_hardware slicecomhw =
{
    "slicecom",
#ifdef COMX_NEW
    VERSION,
#endif
    BOARD_init,
    BOARD_exit,
    BOARD_dump,
    NULL
};

static struct comx_hardware pcicomhw =
{
    "pcicom",
#ifdef COMX_NEW
    VERSION,
#endif
    BOARD_init,
    BOARD_exit,
    BOARD_dump,
    NULL
};

/* Module management */

int __init init_mister(void)
{
    printk(VERSIONSTR);
    comx_register_hardware(&slicecomhw);
    comx_register_hardware(&pcicomhw);
    return munich_probe();
}

static void __exit cleanup_mister(void)
{
    int i;

    comx_unregister_hardware("slicecom");
    comx_unregister_hardware("pcicom");

    for (i = 0; i < MAX_BOARDS; i++)
    {
	if (slicecom_boards[i].bar1)
	    iounmap((void *)slicecom_boards[i].bar1);
	if (slicecom_boards[i].lbi)
	    iounmap((void *)slicecom_boards[i].lbi);
	if (pcicom_boards[i].bar1)
	    iounmap((void *)pcicom_boards[i].bar1);
	if (pcicom_boards[i].lbi)
	    iounmap((void *)pcicom_boards[i].lbi);
    }
}

module_init(init_mister);
module_exit(cleanup_mister);
