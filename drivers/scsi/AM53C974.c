#include <linux/module.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/spinlock.h>

#include <asm/io.h>
#include <asm/system.h>

#include "scsi.h"
#include "hosts.h"
#include "AM53C974.h"
#include "constants.h"
#include "sd.h"

/* AM53/79C974 (PCscsi) driver release 0.5

 * The architecture and much of the code of this device
 * driver was originally developed by Drew Eckhardt for
 * the NCR5380. The following copyrights apply:
 *  For the architecture and all pieces of code which can also be found 
 *    in the NCR5380 device driver:
 *   Copyright 1993, Drew Eckhardt
 *      Visionary Computing 
 *      (Unix and Linux consulting and custom programming)
 *      drew@colorado.edu
 *      +1 (303) 666-5836
 *
 *  The AM53C974_nobios_detect code was originally developed by
 *   Robin Cutshaw (robin@xfree86.org) and is used here in a 
 *   slightly modified form.
 *
 *  PCI detection rewritten by Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 *  For the remaining code:
 *    Copyright 1994, D. Frieauff
 *    EMail: fri@rsx42sun0.dofn.de
 *    Phone: x49-7545-8-2256 , x49-7541-42305
 */

/*
 * $Log: AM53C974.c,v $
 */

#ifdef AM53C974_DEBUG
#define DEB(x) x
#ifdef AM53C974_DEBUG_KEYWAIT
#define KEYWAIT() AM53C974_keywait()
#else
#define KEYWAIT()
#endif
#ifdef AM53C974_DEBUG_INIT
#define DEB_INIT(x) x
#else
#define DEB_INIT(x)
#endif
#ifdef AM53C974_DEBUG_MSG
#define DEB_MSG(x) x
#else
#define DEB_MSG(x)
#endif
#ifdef AM53C974_DEB_RESEL
#define DEB_RESEL(x) x
#else
#define DEB_RESEL(x)
#endif
#ifdef AM53C974_DEBUG_QUEUE
#define DEB_QUEUE(x) x
#define LIST(x,y) {printk("LINE:%d   Adding %p to %p\n", __LINE__, (void*)(x), (void*)(y)); if ((x)==(y)) udelay(5); }
#define REMOVE(w,x,y,z) {printk("LINE:%d   Removing: %p->%p  %p->%p \n", __LINE__, (void*)(w), (void*)(x), (void*)(y), (void*)(z)); if ((x)==(y)) udelay(5); }
#else
#define DEB_QUEUE(x)
#define LIST(x,y)
#define REMOVE(w,x,y,z)
#endif
#ifdef AM53C974_DEBUG_INFO
#define DEB_INFO(x) x
#else
#define DEB_INFO(x)
#endif
#ifdef AM53C974_DEBUG_LINKED
#define DEB_LINKED(x) x
#else
#define DEB_LINKED(x)
#endif
#ifdef AM53C974_DEBUG_INTR
#define DEB_INTR(x) x
#else
#define DEB_INTR(x)
#endif
#else
#define DEB_INIT(x)
#define DEB(x)
#define DEB_QUEUE(x)
#define LIST(x,y)
#define REMOVE(w,x,y,z)
#define DEB_INFO(x)
#define DEB_LINKED(x)
#define DEB_INTR(x)
#define DEB_MSG(x)
#define DEB_RESEL(x)
#define KEYWAIT()
#endif
#ifdef AM53C974_DEBUG_ABORT
#define DEB_ABORT(x) x
#else
#define DEB_ABORT(x)
#endif

#ifdef VERBOSE_AM53C974_DEBUG
#define VDEB(x) x
#else
#define VDEB(x)
#endif

#define INSIDE(x,l,h) ( ((x) >= (l)) && ((x) <= (h)) )


#include <scsi/scsicam.h>

/***************************************************************************************
* Default setting of the controller's SCSI id. Edit and uncomment this only if your    *
* BIOS does not correctly initialize the controller's SCSI id.                         *
* If you don't get a warning during boot, it is correctly initialized.                 *
****************************************************************************************/
/* #define AM53C974_SCSI_ID 7 */

/***************************************************************************************
* Default settings for sync. negotiation enable, transfer rate and sync. offset.       *
* These settings can be replaced by LILO overrides (append) with the following syntax:          *
* AM53C974=host-scsi-id, target-scsi-id, max-rate, max-offset                          *
* Sync. negotiation is disabled by default and will be enabled for those targets which *
* are specified in the LILO override                                                   *
****************************************************************************************/
#define DEFAULT_SYNC_NEGOTIATION_ENABLED 0	/* 0 or 1 */
#define DEFAULT_RATE			 5	/* MHz, min: 3; max: 10 */
#define DEFAULT_SYNC_OFFSET		 0	/* bytes, min: 0; max: 15; use 0 for async. mode */

/***************************************************************************************
* If defined, don't allow targets to disconnect during commands.  This will reduce     *
* performance, but may be worthwhile if you suspect the driver of corrupting data when *
* a disconnect happens.                                                                *
***************************************************************************************/
#define AM53C974_PROHIBIT_DISCONNECT

/* --------------------- don't edit below here  --------------------- */

#define AM53C974_DRIVER_REVISION_MAJOR 0
#define AM53C974_DRIVER_REVISION_MINOR 5
#define SEPARATOR_LINE  \
"--------------------------------------------------------------------------\n"

/* debug control */
/* #define AM53C974_DEBUG */
/* #define AM53C974_DEBUG_MSG */
/* #define AM53C974_DEBUG_KEYWAIT */
/* #define AM53C974_DEBUG_INIT */
/* #define AM53C974_DEBUG_QUEUE */
/* #define AM53C974_DEBUG_INFO */
/* #define AM53C974_DEBUG_LINKED */
/* #define VERBOSE_AM53C974_DEBUG */
/* #define AM53C974_DEBUG_INTR */
/* #define AM53C974_DEB_RESEL */
#define AM53C974_DEBUG_ABORT
/* #define AM53C974_OPTION_DEBUG_PROBE_ONLY */

/* special options/constants */
#define DEF_CLK                 40	/* chip clock freq. in MHz */
#define MIN_PERIOD               4	/* for negotiation: min. number of clocks per cycle */
#define MAX_PERIOD              13	/* for negotiation: max. number of clocks per cycle */
#define MAX_OFFSET              15	/* for negotiation: max. offset (0=async) */

#define DEF_SCSI_TIMEOUT        245	/* STIMREG value, 40 Mhz */
#define DEF_STP                 8	/* STPREG value assuming 5.0 MB/sec, FASTCLK, FASTSCSI */
#define DEF_SOF_RAD             0	/* REQ/ACK deassertion delay */
#define DEF_SOF_RAA             0	/* REQ/ACK assertion delay */
#define DEF_ETM                 0	/* CNTLREG1, ext. timing mode */
#define DEF_PERE                1	/* CNTLREG1, parity error reporting */
#define DEF_CLKF                0	/* CLKFREG,  0=40 Mhz */
#define DEF_ENF                 1	/* CNTLREG2, enable features */
#define DEF_ADIDCHK             0	/* CNTLREG3, additional ID check */
#define DEF_FASTSCSI            1	/* CNTLREG3, fast SCSI */
#define DEF_FASTCLK             1	/* CNTLREG3, fast clocking, 5 MB/sec at 40MHz chip clk */
#define DEF_GLITCH              1	/* CNTLREG4, glitch eater, 0=12ns, 1=35ns, 2=25ns, 3=off */
#define DEF_PWD                 0	/* CNTLREG4, reduced power feature */
#define DEF_RAE                 0	/* CNTLREG4, RAE active negation on REQ, ACK only */
#define DEF_RADE                1	/* 1CNTLREG4, active negation on REQ, ACK and data */

/*** SCSI block ***/
#define CTCLREG		    	0x00	/* r      current transf. count, low byte    */
#define CTCMREG		   	0x04	/* r      current transf. count, middle byte */
#define CTCHREG		    	0x38	/* r      current transf. count, high byte   */
#define STCLREG		    	0x00	/* w      start transf. count, low byte      */
#define STCMREG		    	0x04	/* w      start transf. count, middle byte   */
#define STCHREG		    	0x38	/* w      start transf. count, high byte     */
#define FFREG		    	0x08	/* rw     SCSI FIFO reg.                     */
#define STIMREG		    	0x14	/* w      SCSI timeout reg.                  */

#define SDIDREG		    	0x10	/* w      SCSI destination ID reg.           */
#define SDIREG_MASK		0x07	/* mask                                      */

#define STPREG		    	0x18	/* w      synchronous transf. period reg.    */
#define STPREG_STP		0x1F	/* synchr. transfer period                   */

#define CLKFREG		    	0x24	/* w      clock factor reg.                  */
#define CLKFREG_MASK		0x07	/* mask                                      */

#define CMDREG		    	0x0C	/* rw     SCSI command reg.                  */
#define CMDREG_DMA         	0x80	/* set DMA mode (set together with opcodes below) */
#define CMDREG_IT          	0x10	/* information transfer              */
#define CMDREG_ICCS		0x11	/* initiator command complete steps          */
#define CMDREG_MA		0x12	/* message accepted                          */
#define CMDREG_TPB		0x98	/* transfer pad bytes, DMA mode only         */
#define CMDREG_SATN		0x1A	/* set ATN                                   */
#define CMDREG_RATN		0x1B	/* reset ATN                                 */
#define CMDREG_SOAS		0x41	/* select without ATN steps                  */
#define CMDREG_SAS		0x42	/* select with ATN steps (1 msg byte)        */
#define CMDREG_SASS		0x43	/* select with ATN and stop steps            */
#define CMDREG_ESR		0x44	/* enable selection/reselection      */
#define CMDREG_DSR		0x45	/* disable selection/reselection     */
#define CMDREG_SA3S		0x46	/* select with ATN 3 steps  (3 msg bytes)  */
#define CMDREG_NOP		0x00	/* no operation                      */
#define CMDREG_CFIFO		0x01	/* clear FIFO                                */
#define CMDREG_RDEV		0x02	/* reset device                      */
#define CMDREG_RBUS		0x03	/* reset SCSI bus                            */

#define STATREG		    	0x10	/* r      SCSI status reg.                   */
#define STATREG_INT		0x80	/* SCSI interrupt condition detected         */
#define STATREG_IOE		0x40	/* SCSI illegal operation error detected   */
#define STATREG_PE		0x20	/* SCSI parity error detected                */
#define STATREG_CTZ		0x10	/* CTC reg decremented to zero               */
#define STATREG_MSG		0x04	/* SCSI MSG phase (latched?)                 */
#define STATREG_CD		0x02	/* SCSI C/D phase (latched?)                 */
#define STATREG_IO		0x01	/* SCSI I/O phase (latched?)                 */
#define STATREG_PHASE           0x07	/* SCSI phase mask                           */

#define INSTREG		    	0x14	/* r      interrupt status reg.              */
#define INSTREG_SRST		0x80	/* SCSI reset detected                       */
#define INSTREG_ICMD		0x40	/* SCSI invalid command detected     */
#define INSTREG_DIS		0x20	/* target disconnected or sel/resel timeout */
#define INSTREG_SR		0x10	/* device on bus has service request       */
#define INSTREG_SO		0x08	/* successful operation                      */
#define INSTREG_RESEL		0x04	/* device reselected as initiator    */

#define ISREG		    	0x18	/* r      internal state reg.                */
#define ISREG_SOF		0x08	/* synchronous offset flag (act. low)        */
#define ISREG_IS		0x07	/* status of intermediate op.                */
#define ISREG_OK_NO_STOP        0x04	/* selection successful                    */
#define ISREG_OK_STOP           0x01	/* selection successful                    */

#define CFIREG		    	0x1C	/* r      current FIFO/internal state reg.   */
#define CFIREG_IS		0xE0	/* status of intermediate op.                */
#define CFIREG_CF		0x1F	/* number of bytes in SCSI FIFO              */

#define SOFREG		    	0x1C	/* w      synchr. offset reg.                */
#define SOFREG_RAD		0xC0	/* REQ/ACK deassertion delay (sync.)         */
#define SOFREG_RAA		0x30	/* REQ/ACK assertion delay (sync.)           */
#define SOFREG_SO		0x0F	/* synch. offset (sync.)             */

#define CNTLREG1	    	0x20	/* rw     control register one               */
#define CNTLREG1_ETM		0x80	/* set extended timing mode                  */
#define CNTLREG1_DISR		0x40	/* disable interrupt on SCSI reset           */
#define CNTLREG1_PERE		0x10	/* enable parity error reporting     */
#define CNTLREG1_SID		0x07	/* host adapter SCSI ID                      */

#define CNTLREG2	    	0x2C	/* rw     control register two               */
#define CNTLREG2_ENF		0x40	/* enable features                           */

#define CNTLREG3	    	0x30	/* rw     control register three             */
#define CNTLREG3_ADIDCHK	0x80	/* additional ID check                       */
#define CNTLREG3_FASTSCSI	0x10	/* fast SCSI                                 */
#define CNTLREG3_FASTCLK	0x08	/* fast SCSI clocking                        */

#define CNTLREG4	    	0x34	/* rw     control register four              */
#define CNTLREG4_GLITCH		0xC0	/* glitch eater                              */
#define CNTLREG4_PWD		0x20	/* reduced power feature             */
#define CNTLREG4_RAE		0x08	/* write only, active negot. ctrl.           */
#define CNTLREG4_RADE		0x04	/* active negot. ctrl.                       */
#define CNTLREG4_RES		0x10	/* reserved bit, must be 1                   */

/*** DMA block ***/
#define DMACMD		    	0x40	/* rw     command                            */
#define DMACMD_DIR		0x80	/* transfer direction (1=read from device) */
#define DMACMD_INTE_D		0x40	/* DMA transfer interrupt enable     */
#define DMACMD_INTE_P		0x20	/* page transfer interrupt enable            */
#define DMACMD_MDL		0x10	/* map to memory descriptor list     */
#define DMACMD_DIAG		0x04	/* diagnostics, set to 0             */
#define DMACMD_IDLE 		0x00	/* idle cmd                                  */
#define DMACMD_BLAST		0x01	/* flush FIFO to memory                      */
#define DMACMD_ABORT		0x02	/* terminate DMA                     */
#define DMACMD_START		0x03	/* start DMA                                 */

#define DMASTATUS	      	0x54	/* r      status register                    */
#define DMASTATUS_BCMPLT	0x20	/* BLAST complete                    */
#define DMASTATUS_SCSIINT	0x10	/* SCSI interrupt pending            */
#define DMASTATUS_DONE		0x08	/* DMA transfer terminated                   */
#define DMASTATUS_ABORT		0x04	/* DMA transfer aborted                      */
#define DMASTATUS_ERROR		0x02	/* DMA transfer error                        */
#define DMASTATUS_PWDN		0x02	/* power down indicator                      */

#define DMASTC		    	0x44	/* rw     starting transfer count            */
#define DMASPA		    	0x48	/* rw     starting physical address          */
#define DMAWBC		    	0x4C	/* r      working byte counter               */
#define DMAWAC		    	0x50	/* r      working address counter            */
#define DMASMDLA	    	0x58	/* rw     starting MDL address               */
#define DMAWMAC		    	0x5C	/* r      working MDL counter                */

/*** SCSI phases ***/
#define PHASE_MSGIN             0x07
#define PHASE_MSGOUT            0x06
#define PHASE_RES_1             0x05
#define PHASE_RES_0             0x04
#define PHASE_STATIN            0x03
#define PHASE_CMDOUT            0x02
#define PHASE_DATAIN            0x01
#define PHASE_DATAOUT           0x00


#define AM53C974_local_declare()	unsigned long io_port
#define AM53C974_setio(instance)	io_port = instance->io_port
#define AM53C974_read_8(addr)           inb(io_port + (addr))
#define AM53C974_write_8(addr,x)        outb((x), io_port + (addr))
#define AM53C974_read_16(addr)          inw(io_port + (addr))
#define AM53C974_write_16(addr,x)       outw((x), io_port + (addr))
#define AM53C974_read_32(addr)          inl(io_port + (addr))
#define AM53C974_write_32(addr,x)       outl((x), io_port + (addr))

#define AM53C974_poll_int()             { do { statreg = AM53C974_read_8(STATREG); } \
                                             while (!(statreg & STATREG_INT)) ; \
                                          AM53C974_read_8(INSTREG) ; }	/* clear int */
#define AM53C974_cfifo()		(AM53C974_read_8(CFIREG) & CFIREG_CF)

/* These are "special" values for the tag parameter passed to AM53C974_select. */
#define TAG_NEXT	-1	/* Use next free tag */
#define TAG_NONE	-2	/* Establish I_T_L nexus instead of I_T_L_Q
				   * even on SCSI-II devices */

/************ LILO overrides *************/
typedef struct _override_t {
	int host_scsi_id;	/* SCSI id of the bus controller */
	int target_scsi_id;	/* SCSI id of target */
	int max_rate;		/* max. transfer rate */
	int max_offset;		/* max. sync. offset, 0 = asynchronous */
} override_t;


#ifdef AM53C974_DEBUG
static void AM53C974_print_phase(struct Scsi_Host *instance);
static void AM53C974_print_queues(struct Scsi_Host *instance);
#endif				/* AM53C974_DEBUG */
static void AM53C974_print(struct Scsi_Host *instance);
static void AM53C974_keywait(void);
static __inline__ int AM53C974_pci_detect(Scsi_Host_Template * tpnt);
static int AM53C974_init(Scsi_Host_Template * tpnt, struct pci_dev *pdev);
static void AM53C974_config_after_reset(struct Scsi_Host *instance);
static __inline__ void initialize_SCp(Scsi_Cmnd * cmd);
static __inline__ void run_main(void);
static void AM53C974_main(void);
static void AM53C974_intr(int irq, void *dev_id, struct pt_regs *regs);
static void do_AM53C974_intr(int irq, void *dev_id, struct pt_regs *regs);
static void AM53C974_intr_disconnect(struct Scsi_Host *instance);
static int AM53C974_sync_neg(struct Scsi_Host *instance, int target, unsigned char *msg);
static __inline__ void AM53C974_set_async(struct Scsi_Host *instance, int target);
static __inline__ void AM53C974_set_sync(struct Scsi_Host *instance, int target);
static void AM53C974_information_transfer(struct Scsi_Host *instance,
			      unsigned char statreg, unsigned char isreg,
			      unsigned char instreg, unsigned char cfifo,
					  unsigned char dmastatus);
static int AM53C974_message(struct Scsi_Host *instance, Scsi_Cmnd * cmd, unsigned char msg);
static void AM53C974_select(struct Scsi_Host *instance, Scsi_Cmnd * cmd, int tag);
static void AM53C974_intr_reselect(struct Scsi_Host *instance, unsigned char statreg);
static __inline__ void AM53C974_transfer_dma(struct Scsi_Host *instance, short dir,
				       unsigned long length, char *data);
static void AM53C974_dma_blast(struct Scsi_Host *instance, unsigned char dmastatus,
			       unsigned char statreg);
static void AM53C974_intr_bus_reset(struct Scsi_Host *instance);

static struct Scsi_Host *first_instance;
static Scsi_Host_Template *the_template;
static struct Scsi_Host *first_host;	/* Head of list of AMD boards */
static volatile int main_running;
static int commandline_current;
override_t overrides[7] =
{
	{-1, 0, 0, 0},};	/* LILO overrides */

#ifdef AM53C974_DEBUG
static int deb_stop = 1;

static struct {
	unsigned char value;
	char *name;
} phases[] = {

	{
		PHASE_DATAOUT, "DATAOUT"
	}, {
		PHASE_DATAIN, "DATAIN"
	}, {
		PHASE_CMDOUT, "CMDOUT"
	},
	{
		PHASE_STATIN, "STATIN"
	}, {
		PHASE_MSGOUT, "MSGOUT"
	}, {
		PHASE_MSGIN, "MSGIN"
	},
	{
		PHASE_RES_0, "RESERVED 0"
	}, {
		PHASE_RES_1, "RESERVED 1"
	}
};

/************************************************************************** 
 * Function : void AM53C974_print_phase(struct Scsi_Host *instance)
 *
 * Purpose : print the current SCSI phase for debugging purposes
 *
 * Input : instance - which AM53C974
 **************************************************************************/
static void AM53C974_print_phase(struct Scsi_Host *instance)
{
	AM53C974_local_declare();
	unsigned char statreg, latched;
	int i;
	AM53C974_setio(instance);

	latched = (AM53C974_read_8(CNTLREG2)) & CNTLREG2_ENF;
	statreg = AM53C974_read_8(STATREG);
	for (i = 0; (phases[i].value != PHASE_RES_1) &&
	     (phases[i].value != (statreg & STATREG_PHASE)); ++i);
	if (latched)
		printk("scsi%d : phase %s, latched at end of last command\n", instance->host_no, phases[i].name);
	else
		printk("scsi%d : phase %s, real time\n", instance->host_no, phases[i].name);
}

/**************************************************************************
 * Function : void AM53C974_print_queues(struct Scsi_Host *instance)
 *
 * Purpose : print commands in the various queues
 *
 * Inputs : instance - which AM53C974
 **************************************************************************/
static void AM53C974_print_queues(struct Scsi_Host *instance)
{
	unsigned long flags;
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	Scsi_Cmnd *ptr;

	printk("AM53C974: coroutine is%s running.\n", main_running ? "" : "n't");

	save_flags(flags);
	cli();

	if (!hostdata->connected) {
		printk("scsi%d: no currently connected command\n", instance->host_no);
	} else {
		print_Scsi_Cmnd((Scsi_Cmnd *) hostdata->connected);
	}
	if (!hostdata->sel_cmd) {
		printk("scsi%d: no currently arbitrating command\n", instance->host_no);
	} else {
		print_Scsi_Cmnd((Scsi_Cmnd *) hostdata->sel_cmd);
	}

	printk("scsi%d: issue_queue ", instance->host_no);
	if (!hostdata->issue_queue)
		printk("empty\n");
	else {
		printk(":\n");
		for (ptr = (Scsi_Cmnd *) hostdata->issue_queue; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
			print_Scsi_Cmnd(ptr);
	}

	printk("scsi%d: disconnected_queue ", instance->host_no);
	if (!hostdata->disconnected_queue)
		printk("empty\n");
	else {
		printk(":\n");
		for (ptr = (Scsi_Cmnd *) hostdata->disconnected_queue; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
			print_Scsi_Cmnd(ptr);
	}

	restore_flags(flags);
}

#endif				/* AM53C974_DEBUG */

/**************************************************************************
 * Function : void AM53C974_print(struct Scsi_Host *instance)
 *
 * Purpose : dump the chip registers for debugging purposes
 *
 * Input : instance - which AM53C974
 **************************************************************************/
static void AM53C974_print(struct Scsi_Host *instance)
{
	AM53C974_local_declare();
	unsigned long flags;
	unsigned long ctcreg, dmastc, dmaspa, dmawbc, dmawac;
	unsigned char cmdreg, statreg, isreg, cfireg, cntlreg[4], dmacmd,
	 dmastatus;
	AM53C974_setio(instance);

	save_flags(flags);
	cli();
	ctcreg = AM53C974_read_8(CTCHREG) << 16;
	ctcreg |= AM53C974_read_8(CTCMREG) << 8;
	ctcreg |= AM53C974_read_8(CTCLREG);
	cmdreg = AM53C974_read_8(CMDREG);
	statreg = AM53C974_read_8(STATREG);
	isreg = AM53C974_read_8(ISREG);
	cfireg = AM53C974_read_8(CFIREG);
	cntlreg[0] = AM53C974_read_8(CNTLREG1);
	cntlreg[1] = AM53C974_read_8(CNTLREG2);
	cntlreg[2] = AM53C974_read_8(CNTLREG3);
	cntlreg[3] = AM53C974_read_8(CNTLREG4);
	dmacmd = AM53C974_read_8(DMACMD);
	dmastc = AM53C974_read_32(DMASTC);
	dmaspa = AM53C974_read_32(DMASPA);
	dmawbc = AM53C974_read_32(DMAWBC);
	dmawac = AM53C974_read_32(DMAWAC);
	dmastatus = AM53C974_read_8(DMASTATUS);
	restore_flags(flags);

	printk("AM53C974 register dump:\n");
	printk("IO base: 0x%04lx; CTCREG: 0x%04lx; CMDREG: 0x%02x; STATREG: 0x%02x; ISREG: 0x%02x\n",
	       io_port, ctcreg, cmdreg, statreg, isreg);
	printk("CFIREG: 0x%02x; CNTLREG1-4: 0x%02x; 0x%02x; 0x%02x; 0x%02x\n",
	       cfireg, cntlreg[0], cntlreg[1], cntlreg[2], cntlreg[3]);
	printk("DMACMD: 0x%02x; DMASTC: 0x%04lx; DMASPA: 0x%04lx\n", dmacmd, dmastc, dmaspa);
	printk("DMAWBC: 0x%04lx; DMAWAC: 0x%04lx; DMASTATUS: 0x%02x\n", dmawbc, dmawac, dmastatus);
	printk("---------------------------------------------------------\n");
}

/**************************************************************************
* Function : void AM53C974_keywait(void)
*
* Purpose : wait until a key is pressed, if it was the 'r' key leave singlestep mode;
*           this function is used for debugging only
*
* Input : none
**************************************************************************/
static void AM53C974_keywait(void)
{
	unsigned long flags;
#ifdef AM53C974_DEBUG
	int key;

	if (!deb_stop)
		return;
#endif

	save_flags(flags);
	cli();
	while ((inb_p(0x64) & 0x01) != 0x01);
#ifdef AM53C974_DEBUG
	key = inb(0x60);
	if (key == 0x93)
		deb_stop = 0;	/* don't stop if 'r' was pressed */
#endif
	restore_flags(flags);
}

#ifndef MODULE
/**************************************************************************
* Function : AM53C974_setup(char *str)
*
* Purpose : LILO command line initialization of the overrides array,
* 
* Input : str - parameter string.
*
* Returns : 1.
*
* NOTE : this function needs to be declared as an external function
*         in init/main.c and included there in the bootsetups list
***************************************************************************/
static int AM53C974_setup(char *str)
{
	int ints[5];

	get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] < 4)
		printk("AM53C974_setup: wrong number of parameters;\n correct syntax is: AM53C974=host-scsi-id, target-scsi-id, max-rate, max-offset\n");
	else {
		if (commandline_current < (sizeof(overrides) / sizeof(override_t))) {
			if ((ints[1] < 0) || (ints[1] > 7) ||
			    (ints[2] < 0) || (ints[2] > 7) ||
			    (ints[1] == ints[2]) ||
			    (ints[3] < (DEF_CLK / MAX_PERIOD)) || (ints[3] > (DEF_CLK / MIN_PERIOD)) ||
			    (ints[4] < 0) || (ints[4] > MAX_OFFSET))
				printk("AM53C974_setup: illegal parameter\n");
			else {
				overrides[commandline_current].host_scsi_id = ints[1];
				overrides[commandline_current].target_scsi_id = ints[2];
				overrides[commandline_current].max_rate = ints[3];
				overrides[commandline_current].max_offset = ints[4];
				commandline_current++;
			}
		} else
			printk("AM53C974_setup: too many overrides\n");
	}

	return 1;
}
__setup("AM53C974=", AM53C974_setup);

#endif /* !MODULE */

/**************************************************************************
* Function : int AM53C974_pci_detect(Scsi_Host_Template *tpnt)
*
* Purpose : detects and initializes AM53C974 SCSI chips with PCI Bios
*
* Inputs : tpnt - host template
* 
* Returns : number of host adapters detected
**************************************************************************/
static int __init AM53C974_pci_detect(Scsi_Host_Template * tpnt)
{
	int count = 0;		/* number of boards detected */
	struct pci_dev *pdev = NULL;
	unsigned short command;

	while ((pdev = pci_find_device(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_SCSI, pdev))) {
		if (pci_enable_device(pdev))
			continue;
		pci_read_config_word(pdev, PCI_COMMAND, &command);

		/* check whether device is I/O mapped -- should be */
		if (!(command & PCI_COMMAND_IO))
			continue;

		pci_set_master (pdev);

		/* everything seems OK now, so initialize */
		if (AM53C974_init(tpnt, pdev))
			count++;
	}
	return (count);
}

/**************************************************************************
* Function : int AM53C974_init(Scsi_Host_Template *tpnt, struct pci_dev *pdev)
*
* Purpose : initializes instance and corresponding AM53/79C974 chip,
*
* Inputs : tpnt - template, pci_config - PCI configuration,
* 
* Returns : 1 on success, 0 on failure.
* 
* NOTE: If no override for the controller's SCSI id is given and AM53C974_SCSI_ID 
*       is not defined we assume that the SCSI address of this controller is correctly
*       set up by the BIOS (as reflected by contents of register CNTLREG1).
*       This is the only BIOS assistance we need.
**************************************************************************/
static int __init  AM53C974_init(Scsi_Host_Template * tpnt, struct pci_dev *pdev)
{
	AM53C974_local_declare();
	int i, j;
	struct Scsi_Host *instance, *search;
	struct AM53C974_hostdata *hostdata;

#ifdef AM53C974_OPTION_DEBUG_PROBE_ONLY
	printk("AM53C974: probe only enabled, aborting initialization\n");
	return 0;
#endif

	instance = scsi_register(tpnt, sizeof(struct AM53C974_hostdata));
	if (!instance) {
		printk(KERN_WARNING "AM53C974: Unable to register host, aborting.\n");
		return 0;
	}
	scsi_set_pci_device(instance, pdev);
	hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	instance->base = 0;
	instance->io_port = pci_resource_start(pdev, 0);
	instance->irq = pdev->irq;
	instance->dma_channel = -1;
	AM53C974_setio(instance);

#ifdef AM53C974_SCSI_ID
	instance->this_id = AM53C974_SCSI_ID;
	AM53C974_write_8(CNTLREG1, instance->this_id & CNTLREG1_SID);
#else
	instance->this_id = AM53C974_read_8(CNTLREG1) & CNTLREG1_SID;
	if (instance->this_id != 7)
		printk("scsi%d: WARNING: unusual hostadapter SCSI id %d; please verify!\n",
		       instance->host_no, instance->this_id);
#endif

	for (i = 0; i < sizeof(hostdata->msgout); i++) {
		hostdata->msgout[i] = NOP;
		hostdata->last_message[i] = NOP;
	}
	for (i = 0; i < 8; i++) {
		hostdata->busy[i] = 0;
		hostdata->sync_per[i] = DEF_STP;
		hostdata->sync_off[i] = 0;
		hostdata->sync_neg[i] = 0;
		hostdata->sync_en[i] = DEFAULT_SYNC_NEGOTIATION_ENABLED;
		hostdata->max_rate[i] = DEFAULT_RATE;
		hostdata->max_offset[i] = DEFAULT_SYNC_OFFSET;
	}

/* overwrite defaults by LILO overrides */
	for (i = 0; i < commandline_current; i++) {
		if (overrides[i].host_scsi_id == instance->this_id) {
			j = overrides[i].target_scsi_id;
			hostdata->sync_en[j] = 1;
			hostdata->max_rate[j] = overrides[i].max_rate;
			hostdata->max_offset[j] = overrides[i].max_offset;
		}
	}

	hostdata->sel_cmd = NULL;
	hostdata->connected = NULL;
	hostdata->issue_queue = NULL;
	hostdata->disconnected_queue = NULL;
	hostdata->in_reset = 0;
	hostdata->aborted = 0;
	hostdata->selecting = 0;
	hostdata->disconnecting = 0;
	hostdata->dma_busy = 0;

/* Set up an interrupt handler if we aren't already sharing an IRQ with another board */
	for (search = first_host;
	     search && (((the_template != NULL) && (search->hostt != the_template)) ||
		 (search->irq != instance->irq) || (search == instance));
	     search = search->next);
	if (!search) {
		if (request_irq(instance->irq, do_AM53C974_intr, SA_SHIRQ, "AM53C974", instance)) {
			printk("scsi%d: IRQ%d not free, detaching\n", instance->host_no, instance->irq);
			scsi_unregister(instance);
			return 0;
		}
	} else {
		printk("scsi%d: using interrupt handler previously installed for scsi%d\n",
		       instance->host_no, search->host_no);
	}

	if (!the_template) {
		the_template = instance->hostt;
		first_instance = instance;
	}
/* do hard reset */
	AM53C974_write_8(CMDREG, CMDREG_RDEV);	/* reset device */
	udelay(5);
	AM53C974_write_8(CMDREG, CMDREG_NOP);
	AM53C974_write_8(CNTLREG1, CNTLREG1_DISR | instance->this_id);
	AM53C974_write_8(CMDREG, CMDREG_RBUS);	/* reset SCSI bus */
	udelay(10);
	AM53C974_config_after_reset(instance);
	mdelay(500);
	return (1);
}

/*********************************************************************
* Function : AM53C974_config_after_reset(struct Scsi_Host *instance) *
*                                                                    *
* Purpose : initializes chip registers after reset                   *
*                                                                    *
* Inputs : instance - which AM53C974                                 *
*                                                                    *
* Returns : nothing                                                  *
**********************************************************************/
static void AM53C974_config_after_reset(struct Scsi_Host *instance)
{
	AM53C974_local_declare();
	AM53C974_setio(instance);

/* clear SCSI FIFO */
	AM53C974_write_8(CMDREG, CMDREG_CFIFO);

/* configure device */
	AM53C974_write_8(STIMREG, DEF_SCSI_TIMEOUT);
	AM53C974_write_8(STPREG, DEF_STP & STPREG_STP);
	AM53C974_write_8(SOFREG, (DEF_SOF_RAD << 6) | (DEF_SOF_RAA << 4));
	AM53C974_write_8(CLKFREG, DEF_CLKF & CLKFREG_MASK);
	AM53C974_write_8(CNTLREG1, (DEF_ETM << 7) | CNTLREG1_DISR | (DEF_PERE << 4) | instance->this_id);
	AM53C974_write_8(CNTLREG2, (DEF_ENF << 6));
	AM53C974_write_8(CNTLREG3, (DEF_ADIDCHK << 7) | (DEF_FASTSCSI << 4) | (DEF_FASTCLK << 3));
	AM53C974_write_8(CNTLREG4, (DEF_GLITCH << 6) | (DEF_PWD << 5) | (DEF_RAE << 3) | (DEF_RADE << 2) | CNTLREG4_RES);
}

/***********************************************************************
* Function : const char *AM53C974_info(struct Scsi_Host *instance)     *
*                                                                      *
* Purpose : return device driver information                           *
*                                                                      *
* Inputs : instance - which AM53C974                                   *
*                                                                      *
* Returns : info string                                                *
************************************************************************/
static const char *AM53C974_info(struct Scsi_Host *instance)
{
	static char info[100];

	sprintf(info, "AM53/79C974 PCscsi driver rev. %d.%d; host I/O address: 0x%lx; irq: %d\n",
	  AM53C974_DRIVER_REVISION_MAJOR, AM53C974_DRIVER_REVISION_MINOR,
		instance->io_port, instance->irq);
	return (info);
}

/************************************************************************** 
* Function : int AM53C974_command (Scsi_Cmnd *SCpnt)                      *
*                                                                         *
* Purpose : the unqueued SCSI command function, replaced by the           *
*           AM53C974_queue_command function                               *
*                                                                         *
* Inputs : SCpnt - pointer to command structure                           *
*                                                                         *
* Returns :status, see hosts.h for details                                *
***************************************************************************/
static int AM53C974_command(Scsi_Cmnd * SCpnt)
{
	DEB(printk("AM53C974_command called\n"));
	return 0;
}

/**************************************************************************
* Function : void initialize_SCp(Scsi_Cmnd *cmd)                          *
*                                                                         *
* Purpose : initialize the saved data pointers for cmd to point to the    *
*	    start of the buffer.                                          *                              
*                                                                         *
* Inputs : cmd - Scsi_Cmnd structure to have pointers reset.              *
*                                                                         *
* Returns : nothing                                                       *
**************************************************************************/
static __inline__ void initialize_SCp(Scsi_Cmnd * cmd)
{
	if (cmd->use_sg) {
		cmd->SCp.buffer = (struct scatterlist *) cmd->buffer;
		cmd->SCp.buffers_residual = cmd->use_sg - 1;
		cmd->SCp.ptr = (char *) cmd->SCp.buffer->address;
		cmd->SCp.this_residual = cmd->SCp.buffer->length;
	} else {
		cmd->SCp.buffer = NULL;
		cmd->SCp.buffers_residual = 0;
		cmd->SCp.ptr = (char *) cmd->request_buffer;
		cmd->SCp.this_residual = cmd->request_bufflen;
	}
}

/**************************************************************************
* Function : run_main(void)                                               *
*                                                                         *
* Purpose : insure that the coroutine is running and will process our     *
* 	    request.  main_running is checked/set here (in an inline      *
*           function rather than in AM53C974_main itself to reduce the    *
*           chances of stack overflow.                                    *
*                                                                         *
*                                                                         *
* Inputs : none                                                           *
*                                                                         *
* Returns : nothing                                                       *
**************************************************************************/
static __inline__ void run_main(void)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	if (!main_running) {
		/* main_running is cleared in AM53C974_main once it can't do 
		   more work, and AM53C974_main exits with interrupts disabled. */
		main_running = 1;
		AM53C974_main();
	}
	restore_flags(flags);
}

/************************************************************************** 
* Function : int AM53C974_queue_command(Scsi_Cmnd *cmd, void (*done)(Scsi_Cmnd *))
*
* Purpose : writes SCSI command into AM53C974 FIFO 
*
* Inputs : cmd - SCSI command, done - function called on completion, with
*	a pointer to the command descriptor.
* 
* Returns : status, see hosts.h for details
*
* Side effects : 
*      cmd is added to the per instance issue_queue, with minor 
*	twiddling done to the host specific fields of cmd.  If the 
*	main coroutine is not running, it is restarted.
**************************************************************************/
static int AM53C974_queue_command(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
	unsigned long flags;
	struct Scsi_Host *instance = cmd->host;
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	Scsi_Cmnd *tmp;

	save_flags(flags);
	cli();
	DEB_QUEUE(printk(SEPARATOR_LINE));
	DEB_QUEUE(printk("scsi%d: AM53C974_queue_command called\n", instance->host_no));
	DEB_QUEUE(printk("cmd=%02x target=%02x lun=%02x bufflen=%d use_sg = %02x\n",
			 cmd->cmnd[0], cmd->target, cmd->lun, cmd->request_bufflen, cmd->use_sg));

/* We use the host_scribble field as a pointer to the next command in a queue */
	cmd->host_scribble = NULL;
	cmd->scsi_done = done;
	cmd->result = 0;
	cmd->device->disconnect = 0;

/* Insert the cmd into the issue queue. Note that REQUEST SENSE 
 * commands are added to the head of the queue since any command will
 * clear the contingent allegiance condition that exists and the 
 * sense data is only guaranteed to be valid while the condition exists. */
	if (!(hostdata->issue_queue) || (cmd->cmnd[0] == REQUEST_SENSE)) {
		LIST(cmd, hostdata->issue_queue);
		cmd->host_scribble = (unsigned char *) hostdata->issue_queue;
		hostdata->issue_queue = cmd;
	} else {
		for (tmp = (Scsi_Cmnd *) hostdata->issue_queue; tmp->host_scribble;
		     tmp = (Scsi_Cmnd *) tmp->host_scribble);
		LIST(cmd, tmp);
		tmp->host_scribble = (unsigned char *) cmd;
	}

	DEB_QUEUE(printk("scsi%d : command added to %s of queue\n", instance->host_no,
		     (cmd->cmnd[0] == REQUEST_SENSE) ? "head" : "tail"));

/* Run the coroutine if it isn't already running. */
	run_main();
	restore_flags(flags);
	return 0;
}

/**************************************************************************
 * Function : AM53C974_main (void) 
 *
 * Purpose : AM53C974_main is a coroutine that runs as long as more work can 
 *	be done on the AM53C974 host adapters in a system.  Both 
 *	AM53C974_queue_command() and AM53C974_intr() will try to start it 
 *	in case it is not running.
 * 
 * NOTE : AM53C974_main exits with interrupts *disabled*, the caller should 
 *  reenable them.  This prevents reentrancy and kernel stack overflow.
 **************************************************************************/
static void AM53C974_main(void)
{
	AM53C974_local_declare();
	unsigned long flags;
	Scsi_Cmnd *tmp, *prev;
	struct Scsi_Host *instance;
	struct AM53C974_hostdata *hostdata;
	int done;

/* We run (with interrupts disabled) until we're sure that none of 
 * the host adapters have anything that can be done, at which point 
 * we set main_running to 0 and exit. */

	save_flags(flags);
	cli();		/* Freeze request queues */
	do {
		done = 1;
		for (instance = first_instance; instance && instance->hostt == the_template;
		     instance = instance->next) {
			hostdata = (struct AM53C974_hostdata *) instance->hostdata;
			AM53C974_setio(instance);
			/* start to select target if we are not connected and not in the 
			   selection process */
			if (!hostdata->connected && !hostdata->sel_cmd) {
				/* Search through the issue_queue for a command destined for a target 
				   that is not busy. */
				for (tmp = (Scsi_Cmnd *) hostdata->issue_queue, prev = NULL; tmp;
				     prev = tmp, tmp = (Scsi_Cmnd *) tmp->host_scribble) {
					/*  When we find one, remove it from the issue queue. */
					if (!(hostdata->busy[tmp->target] & (1 << tmp->lun))) {
						if (prev) {
							REMOVE(prev, (Scsi_Cmnd *) (prev->host_scribble), tmp,
							       (Scsi_Cmnd *) (tmp->host_scribble));
							prev->host_scribble = tmp->host_scribble;
						} else {
							REMOVE(-1, hostdata->issue_queue, tmp, tmp->host_scribble);
							hostdata->issue_queue = (Scsi_Cmnd *) tmp->host_scribble;
						}
						tmp->host_scribble = NULL;

						/* go into selection mode, disable reselection and wait for
						   SO interrupt which will continue with the selection process */
						hostdata->selecting = 1;
						hostdata->sel_cmd = tmp;
						AM53C974_write_8(CMDREG, CMDREG_DSR);
						break;
					}	/* if target/lun is not busy */
				}	/* for */
			}
			/* if (!hostdata->connected) */ 
			else {
				DEB(printk("main: connected; cmd = 0x%lx, sel_cmd = 0x%lx\n",
					   (long) hostdata->connected, (long) hostdata->sel_cmd));
			}
		}		/* for instance */
	} while (!done);
	main_running = 0;
	restore_flags(flags);
}

/************************************************************************
* Function : AM53C974_intr(int irq, void *dev_id, struct pt_regs *regs) *
*                                                                       *
* Purpose : interrupt handler                                           *
*                                                                       *
* Inputs : irq - interrupt line, regs - ?                               *
*                                                                       *
* Returns : nothing                                                     *
************************************************************************/
static void do_AM53C974_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	spin_lock_irqsave(&io_request_lock, flags);
	AM53C974_intr(irq, dev_id, regs);
	spin_unlock_irqrestore(&io_request_lock, flags);
}

/************************************************************************
* Function : AM53C974_intr(int irq, void *dev_id, struct pt_regs *regs) *
*                                                                       *
* Purpose : interrupt handler                                           *
*                                                                       *
* Inputs : irq - interrupt line, regs - ?                               *
*                                                                       *
* Returns : nothing                                                     *
************************************************************************/
static void AM53C974_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	AM53C974_local_declare();
	struct Scsi_Host *instance;
	struct AM53C974_hostdata *hostdata;
	unsigned char cmdreg, dmastatus, statreg, isreg, instreg, cfifo;

/* find AM53C974 hostadapter responsible for this interrupt */
	for (instance = first_instance; instance; instance = instance->next)
		if ((instance->irq == irq) && (instance->hostt == the_template))
			goto FOUND;
	return;

/* found; now decode and process */
	FOUND:
	    hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	AM53C974_setio(instance);
	dmastatus = AM53C974_read_8(DMASTATUS);

	DEB_INTR(printk(SEPARATOR_LINE));
	DEB_INTR(printk("AM53C974 interrupt; dmastatus=0x%02x\n", dmastatus));
	KEYWAIT();

/*** DMA related interrupts ***/
	if (hostdata->connected && (dmastatus & (DMASTATUS_ERROR | DMASTATUS_PWDN |
						 DMASTATUS_ABORT))) {
		/* DMA error or POWERDOWN */
		printk("scsi%d: DMA error or powerdown; dmastatus: 0x%02x\n",
		       instance->host_no, dmastatus);
#ifdef AM53C974_DEBUG
		deb_stop = 1;
#endif
		panic("scsi%d: cannot recover\n", instance->host_no);
	}
	if (hostdata->connected && (dmastatus & DMASTATUS_DONE)) {
		/* DMA transfer done */
		unsigned long residual;
		unsigned long flags;
		save_flags(flags);
		cli();
		if (!(AM53C974_read_8(DMACMD) & DMACMD_DIR)) {
			do {
				dmastatus = AM53C974_read_8(DMASTATUS);
				residual = AM53C974_read_8(CTCLREG) | (AM53C974_read_8(CTCMREG) << 8) |
				    (AM53C974_read_8(CTCHREG) << 16);
				residual += AM53C974_read_8(CFIREG) & CFIREG_CF;
			} while (!(dmastatus & DMASTATUS_SCSIINT) && residual);
			residual = AM53C974_read_8(CTCLREG) | (AM53C974_read_8(CTCMREG) << 8) |
			    (AM53C974_read_8(CTCHREG) << 16);
			residual += AM53C974_read_8(CFIREG) & CFIREG_CF;
		} else
			residual = 0;
		hostdata->connected->SCp.ptr += hostdata->connected->SCp.this_residual - residual;
		hostdata->connected->SCp.this_residual = residual;

		AM53C974_write_8(DMACMD, DMACMD_IDLE);

		/* if service request missed before, process it now (ugly) */
		if (hostdata->dma_busy) {
			hostdata->dma_busy = 0;
			cmdreg = AM53C974_read_8(CMDREG);
			statreg = AM53C974_read_8(STATREG);
			isreg = AM53C974_read_8(ISREG);
			instreg = AM53C974_read_8(INSTREG);
			cfifo = AM53C974_cfifo();
			AM53C974_information_transfer(instance, statreg, isreg, instreg, cfifo,
						      dmastatus);
		}
		restore_flags(flags);
	}
	if (!(dmastatus & DMASTATUS_SCSIINT)) {
		return;
	}
/*** SCSI related interrupts ***/
	cmdreg = AM53C974_read_8(CMDREG);
	statreg = AM53C974_read_8(STATREG);
	isreg = AM53C974_read_8(ISREG);
	instreg = AM53C974_read_8(INSTREG);
	cfifo = AM53C974_cfifo();

	DEB_INTR(printk("scsi%d: statreg: 0x%02x; isreg: 0x%02x; instreg: 0x%02x; cfifo: 0x%02x\n",
		     instance->host_no, statreg, isreg, instreg, cfifo));

	if (statreg & STATREG_PE) {
		/* parity error */
#ifdef AM53C974_DEBUG
		deb_stop = 1;
#endif
		printk("scsi%d : PARITY error\n", instance->host_no);
		if (hostdata->connected)
			hostdata->sync_off[hostdata->connected->target] = 0;	/* setup asynchronous transfer */
		hostdata->aborted = 1;
	}
	if (statreg & STATREG_IOE) {
		/* illegal operation error */
#ifdef AM53C974_DEBUG
		deb_stop = 1;
#endif
		printk("scsi%d : ILLEGAL OPERATION error\n", instance->host_no);
		printk("cmdreg:  0x%02x; dmacmd:  0x%02x; statreg: 0x%02x; \n"
		   "isreg:   0x%02x; instreg: 0x%02x; cfifo:   0x%02x\n",
		       cmdreg, AM53C974_read_8(DMACMD), statreg, isreg, instreg, cfifo);
	}
	if (hostdata->in_reset && (instreg & INSTREG_SRST)) {
		unsigned long flags;
		/* RESET INTERRUPT */
#ifdef AM53C974_DEBUG
		deb_stop = 1;
#endif
		DEB(printk("Bus reset interrupt received\n"));
		AM53C974_intr_bus_reset(instance);
		save_flags(flags);
		cli();
		if (hostdata->connected) {
			hostdata->connected->result = DID_RESET << 16;
			hostdata->connected->scsi_done((Scsi_Cmnd *) hostdata->connected);
			hostdata->connected = NULL;
		} else {
			if (hostdata->sel_cmd) {
				hostdata->sel_cmd->result = DID_RESET << 16;
				hostdata->sel_cmd->scsi_done((Scsi_Cmnd *) hostdata->sel_cmd);
				hostdata->sel_cmd = NULL;
			}
		}
		restore_flags(flags);
		if (hostdata->in_reset == 1)
			goto EXIT;
		else
			return;
	}
	if (instreg & INSTREG_ICMD) {
		/* INVALID COMMAND INTERRUPT */
#ifdef AM53C974_DEBUG
		deb_stop = 1;
#endif
		printk("scsi%d: Invalid command interrupt\n", instance->host_no);
		printk("cmdreg:  0x%02x; dmacmd:  0x%02x; statreg: 0x%02x; dmastatus: 0x%02x; \n"
		   "isreg:   0x%02x; instreg: 0x%02x; cfifo:   0x%02x\n",
		       cmdreg, AM53C974_read_8(DMACMD), statreg, dmastatus, isreg, instreg, cfifo);
		panic("scsi%d: cannot recover\n", instance->host_no);
	}
	if (instreg & INSTREG_DIS) {
		unsigned long flags;
		/* DISCONNECT INTERRUPT */
		DEB_INTR(printk("Disconnect interrupt received; "));
		save_flags(flags);
		cli();
		AM53C974_intr_disconnect(instance);
		restore_flags(flags);
		goto EXIT;
	}
	if (instreg & INSTREG_RESEL) {
		unsigned long flags;
		/* RESELECTION INTERRUPT */
		DEB_INTR(printk("Reselection interrupt received\n"));
		save_flags(flags);
		cli();
		AM53C974_intr_reselect(instance, statreg);
		restore_flags(flags);
		goto EXIT;
	}
	if (instreg & INSTREG_SO) {
		DEB_INTR(printk("Successful operation interrupt received\n"));
		if (hostdata->selecting) {
			unsigned long flags;
			DEB_INTR(printk("DSR completed, starting select\n"));
			save_flags(flags);
			cli();
			AM53C974_select(instance, (Scsi_Cmnd *) hostdata->sel_cmd,
			  (hostdata->sel_cmd->cmnd[0] == REQUEST_SENSE) ?
					TAG_NONE : TAG_NEXT);
			hostdata->selecting = 0;
			AM53C974_set_sync(instance, hostdata->sel_cmd->target);
			restore_flags(flags);
			return;
		}
		if (hostdata->sel_cmd != NULL) {
			if (((isreg & ISREG_IS) != ISREG_OK_NO_STOP) &&
			    ((isreg & ISREG_IS) != ISREG_OK_STOP)) {
				unsigned long flags;
				/* UNSUCCESSFUL SELECTION */
				DEB_INTR(printk("unsuccessful selection\n"));
				save_flags(flags);
				cli();
				hostdata->dma_busy = 0;
				LIST(hostdata->sel_cmd, hostdata->issue_queue);
				hostdata->sel_cmd->host_scribble = (unsigned char *) hostdata->issue_queue;
				hostdata->issue_queue = hostdata->sel_cmd;
				hostdata->sel_cmd = NULL;
				hostdata->selecting = 0;
				restore_flags(flags);
				goto EXIT;
			} else {
				unsigned long flags;
				/* SUCCESSFUL SELECTION */
				DEB(printk("successful selection; cmd=0x%02lx\n", (long) hostdata->sel_cmd));
				save_flags(flags);
				cli();
				hostdata->dma_busy = 0;
				hostdata->disconnecting = 0;
				hostdata->connected = hostdata->sel_cmd;
				hostdata->sel_cmd = NULL;
				hostdata->selecting = 0;
#ifdef SCSI2
				if (!hostdata->connected->device->tagged_queue)
#endif
					hostdata->busy[hostdata->connected->target] |= (1 << hostdata->connected->lun);
				/* very strange -- use_sg is sometimes nonzero for request sense commands !! */
				if ((hostdata->connected->cmnd[0] == REQUEST_SENSE) && hostdata->connected->use_sg) {
					DEB(printk("scsi%d: REQUEST_SENSE command with nonzero use_sg\n", instance->host_no));
					KEYWAIT();
					hostdata->connected->use_sg = 0;
				}
				initialize_SCp((Scsi_Cmnd *) hostdata->connected);
				hostdata->connected->SCp.phase = PHASE_CMDOUT;
				AM53C974_information_transfer(instance, statreg, isreg, instreg, cfifo, dmastatus);
				restore_flags(flags);
				return;
			}
		} else {
			unsigned long flags;
			save_flags(flags);
			cli();
			AM53C974_information_transfer(instance, statreg, isreg, instreg, cfifo, dmastatus);
			restore_flags(flags);
			return;
		}
	}
	if (instreg & INSTREG_SR) {
		DEB_INTR(printk("Service request interrupt received, "));
		if (hostdata->connected) {
			unsigned long flags;
			DEB_INTR(printk("calling information_transfer\n"));
			save_flags(flags);
			cli();
			AM53C974_information_transfer(instance, statreg, isreg, instreg, cfifo, dmastatus);
			restore_flags(flags);
		} else {
			printk("scsi%d: weird: service request when no command connected\n", instance->host_no);
			AM53C974_write_8(CMDREG, CMDREG_CFIFO);
		}		/* clear FIFO */
		return;
	}
	EXIT:
	    DEB_INTR(printk("intr: starting main\n"));
	run_main();
	DEB_INTR(printk("end of intr\n"));
}

/************************************************************************** 
* Function : AM53C974_intr_disconnect(struct Scsi_Host *instance)
*
* Purpose : manage target disconnection
*
* Inputs : instance -- which AM53C974
* 
* Returns : nothing
**************************************************************************/
static void AM53C974_intr_disconnect(struct Scsi_Host *instance)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	Scsi_Cmnd *cmd;
	AM53C974_setio(instance);

	if (hostdata->sel_cmd != NULL) {
		/* normal selection timeout, typical for nonexisting targets */
		cmd = (Scsi_Cmnd *) hostdata->sel_cmd;
		DEB_INTR(printk("bad target\n"));
		cmd->result = DID_BAD_TARGET << 16;
		goto EXIT_FINISHED;
	}
	if (!hostdata->connected) {
		/* can happen if controller was reset, a device tried to reconnect,
		   failed and disconnects now */
		AM53C974_write_8(CMDREG, CMDREG_CFIFO);
		return;
	}
	if (hostdata->disconnecting) {
		/* target sent disconnect message, so we are prepared */
		cmd = (Scsi_Cmnd *) hostdata->connected;
		AM53C974_set_async(instance, cmd->target);
		DEB_INTR(printk("scsi%d : disc. from cmnd %d for ta %d, lun %d\n",
		instance->host_no, cmd->cmnd[0], cmd->target, cmd->lun));
		if (cmd->device->disconnect) {
			/* target wants to reselect later */
			DEB_INTR(printk("ok, re-enabling selection\n"));
			LIST(cmd, hostdata->disconnected_queue);
			cmd->host_scribble = (unsigned char *) hostdata->disconnected_queue;
			hostdata->disconnected_queue = cmd;
			DEB_QUEUE(printk("scsi%d : command for target %d lun %d this %d was moved from connected to"
					 "  the disconnected_queue\n", instance->host_no, cmd->target,
					 cmd->lun, hostdata->disconnected_queue->SCp.this_residual));
			DEB_QUEUE(AM53C974_print_queues(instance));
			goto EXIT_UNFINISHED;
		} else {
			/* target does not want to reselect later, we are really finished */
#ifdef AM53C974_DEBUG
			if (cmd->cmnd[0] == REQUEST_SENSE) {
				int i;
				printk("Request sense data dump:\n");
				for (i = 0; i < cmd->request_bufflen; i++) {
					printk("%02x ", *((char *) (cmd->request_buffer) + i));
					if (i && !(i % 16))
						printk("\n");
				}
				printk("\n");
			}
#endif
			goto EXIT_FINISHED;
		}		/* !cmd->device->disconnect */
	}			/* if (hostdata->disconnecting) */
	/* no disconnect message received; unexpected disconnection */
	cmd = (Scsi_Cmnd *) hostdata->connected;
	if (cmd) {
#ifdef AM53C974_DEBUG
		deb_stop = 1;
#endif
		AM53C974_set_async(instance, cmd->target);
		printk("scsi%d: Unexpected disconnect; phase: %d; target: %d; this_residual: %d; buffers_residual: %d; message: %d\n",
		       instance->host_no, cmd->SCp.phase, cmd->target, cmd->SCp.this_residual, cmd->SCp.buffers_residual,
		       cmd->SCp.Message);
		printk("cmdreg: 0x%02x; statreg: 0x%02x; isreg: 0x%02x; cfifo: 0x%02x\n",
		       AM53C974_read_8(CMDREG), AM53C974_read_8(STATREG), AM53C974_read_8(ISREG),
		       AM53C974_read_8(CFIREG) & CFIREG_CF);

		if ((hostdata->last_message[0] == EXTENDED_MESSAGE) &&
		    (hostdata->last_message[2] == EXTENDED_SDTR)) {
			/* sync. negotiation was aborted, setup asynchronous transfer with target */
			hostdata->sync_off[cmd->target] = 0;
		}
		if (hostdata->aborted || hostdata->msgout[0] == ABORT)
			cmd->result = DID_ABORT << 16;
		else
			cmd->result = DID_ERROR << 16;
		goto EXIT_FINISHED;
	}
	EXIT_FINISHED:
	    hostdata->aborted = 0;
	hostdata->msgout[0] = NOP;
	hostdata->sel_cmd = NULL;
	hostdata->connected = NULL;
	hostdata->selecting = 0;
	hostdata->disconnecting = 0;
	hostdata->dma_busy = 0;
	hostdata->busy[cmd->target] &= ~(1 << cmd->lun);
	AM53C974_write_8(CMDREG, CMDREG_CFIFO);
	DEB(printk("disconnect; issue_queue: 0x%lx, disconnected_queue: 0x%lx\n",
		   (long) hostdata->issue_queue, (long) hostdata->disconnected_queue));
	cmd->scsi_done(cmd);

	if (!hostdata->selecting) {
		AM53C974_set_async(instance, cmd->target);
		AM53C974_write_8(CMDREG, CMDREG_ESR);
	}			/* allow reselect */
	return;

	EXIT_UNFINISHED:
	    hostdata->msgout[0] = NOP;
	hostdata->sel_cmd = NULL;
	hostdata->connected = NULL;
	hostdata->aborted = 0;
	hostdata->selecting = 0;
	hostdata->disconnecting = 0;
	hostdata->dma_busy = 0;
	DEB(printk("disconnect; issue_queue: 0x%lx, disconnected_queue: 0x%lx\n",
		   (long) hostdata->issue_queue, (long) hostdata->disconnected_queue));
	if (!hostdata->selecting) {
		AM53C974_set_async(instance, cmd->target);
		AM53C974_write_8(CMDREG, CMDREG_ESR);
	}			/* allow reselect */
	return;
}

/************************************************************************** 
* Function : int AM53C974_sync_neg(struct Scsi_Host *instance, int target, unsigned char *msg)
*
* Purpose : setup message string for sync. negotiation
*
* Inputs : instance -- which AM53C974
*          target -- which SCSI target to deal with
*          msg -- input message string
* 
* Returns : 0 if parameters accepted or 1 if not accepted
*
* Side effects: hostdata is changed
*
* Note: we assume here that fastclk is enabled
**************************************************************************/
static int AM53C974_sync_neg(struct Scsi_Host *instance, int target, unsigned char *msg)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	int period, offset, i, rate, rate_rem;
	AM53C974_setio(instance);

	period = (DEF_CLK * msg[3] * 8 + 1000) / 2000;
	if (period < MIN_PERIOD) {
		period = MIN_PERIOD;
		hostdata->msgout[3] = period / 4;
	} else if (period > MAX_PERIOD) {
		period = MAX_PERIOD;
		hostdata->msgout[3] = period / 4;
	} else
		hostdata->msgout[3] = msg[3];
	offset = msg[4];
	if (offset > MAX_OFFSET)
		offset = MAX_OFFSET;
	hostdata->msgout[4] = offset;
	hostdata->sync_per[target] = period;
	hostdata->sync_off[target] = offset;
	for (i = 0; i < 3; i++)
		hostdata->msgout[i] = msg[i];
	if ((hostdata->msgout[3] != msg[3]) || (msg[4] != offset))
		return (1);

	rate = DEF_CLK / period;
	rate_rem = 10 * (DEF_CLK - period * rate) / period;

	if (offset)
		printk("\ntarget %d: rate=%d.%d Mhz, synchronous, sync offset=%d bytes\n",
		       target, rate, rate_rem, offset);
	else
		printk("\ntarget %d: rate=%d.%d Mhz, asynchronous\n", target, rate, rate_rem);

	return (0);
}

/************************************************************************** 
* Function : AM53C974_set_async(struct Scsi_Host *instance, int target)
*
* Purpose : put controller into async. mode
*
* Inputs : instance -- which AM53C974
*          target -- which SCSI target to deal with
* 
* Returns : nothing
**************************************************************************/
static __inline__ void AM53C974_set_async(struct Scsi_Host *instance, int target)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	AM53C974_setio(instance);

	AM53C974_write_8(STPREG, hostdata->sync_per[target]);
	AM53C974_write_8(SOFREG, (DEF_SOF_RAD << 6) | (DEF_SOF_RAA << 4));
}

/************************************************************************** 
* Function : AM53C974_set_sync(struct Scsi_Host *instance, int target)
*
* Purpose : put controller into sync. mode
*
* Inputs : instance -- which AM53C974
*          target -- which SCSI target to deal with
* 
* Returns : nothing
**************************************************************************/
static __inline__ void AM53C974_set_sync(struct Scsi_Host *instance, int target)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	AM53C974_setio(instance);

	AM53C974_write_8(STPREG, hostdata->sync_per[target]);
	AM53C974_write_8(SOFREG, (SOFREG_SO & hostdata->sync_off[target]) |
			 (DEF_SOF_RAD << 6) | (DEF_SOF_RAA << 4));
}

/***********************************************************************
* Function : AM53C974_information_transfer(struct Scsi_Host *instance, *
*                          unsigned char statreg, unsigned char isreg, *
*                         unsigned char instreg, unsigned char cfifo,  *
*                         unsigned char dmastatus)                     *
*                                                                      *
* Purpose : handle phase changes                                       *
*                                                                      *
* Inputs : instance - which AM53C974                                   *
*          statreg - status register                                     *
*          isreg - internal state register                             *
*          instreg - interrupt status register                         *
*          cfifo - number of bytes in FIFO                             *
*          dmastatus - dma status register                             *
*                                                                      *
* Returns : nothing                                                    *
************************************************************************/
static void AM53C974_information_transfer(struct Scsi_Host *instance,
			      unsigned char statreg, unsigned char isreg,
			      unsigned char instreg, unsigned char cfifo,
					  unsigned char dmastatus)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	Scsi_Cmnd *cmd = (Scsi_Cmnd *) hostdata->connected;
	int ret, i, len, residual = -1;
	AM53C974_setio(instance);

	DEB_INFO(printk(SEPARATOR_LINE));
	switch (statreg & STATREG_PHASE) {	/* scsi phase */
		case PHASE_DATAOUT:
		    DEB_INFO(printk("Dataout phase; cmd=0x%lx, sel_cmd=0x%lx, this_residual=%d, buffers_residual=%d\n",
				    (long) hostdata->connected, (long) hostdata->sel_cmd, cmd->SCp.this_residual, cmd->SCp.buffers_residual));
		cmd->SCp.phase = PHASE_DATAOUT;
		goto PHASE_DATA_IO;

		case PHASE_DATAIN:
		    DEB_INFO(printk("Datain phase; cmd=0x%lx, sel_cmd=0x%lx, this_residual=%d, buffers_residual=%d\n",
				    (long) hostdata->connected, (long) hostdata->sel_cmd, cmd->SCp.this_residual, cmd->SCp.buffers_residual));
		cmd->SCp.phase = PHASE_DATAIN;
		PHASE_DATA_IO:
		    if (hostdata->aborted) {
			AM53C974_write_8(DMACMD, DMACMD_IDLE);
			AM53C974_write_8(CMDREG, CMDREG_CFIFO);
			AM53C974_write_8(CMDREG, CMDREG_SATN);
			return;
		}
		if ((!cmd->SCp.this_residual) && cmd->SCp.buffers_residual) {
			cmd->SCp.buffer++;
			cmd->SCp.buffers_residual--;
			cmd->SCp.ptr = (unsigned char *) cmd->SCp.buffer->address;
			cmd->SCp.this_residual = cmd->SCp.buffer->length;
		}
		if (cmd->SCp.this_residual) {
			if (!(AM53C974_read_8(DMACMD) & DMACMD_START)) {
				hostdata->dma_busy = 0;
				AM53C974_transfer_dma(instance, statreg & STATREG_IO,
				  (unsigned long) cmd->SCp.this_residual,
						      cmd->SCp.ptr);
			} else
				hostdata->dma_busy = 1;
		}
		return;

		case PHASE_MSGIN:
		    DEB_INFO(printk("Message-In phase; cmd=0x%lx, sel_cmd=0x%lx\n",
		  (long) hostdata->connected, (long) hostdata->sel_cmd));
		AM53C974_set_async(instance, cmd->target);
		if (cmd->SCp.phase == PHASE_DATAIN)
			AM53C974_dma_blast(instance, dmastatus, statreg);
		if ((cmd->SCp.phase == PHASE_DATAOUT) && (AM53C974_read_8(DMACMD) & DMACMD_START)) {
			AM53C974_write_8(DMACMD, DMACMD_IDLE);
			residual = cfifo + (AM53C974_read_8(CTCLREG) | (AM53C974_read_8(CTCMREG) << 8) |
				       (AM53C974_read_8(CTCHREG) << 16));
			cmd->SCp.ptr += cmd->SCp.this_residual - residual;
			cmd->SCp.this_residual = residual;
			if (cfifo) {
				AM53C974_write_8(CMDREG, CMDREG_CFIFO);
				cfifo = 0;
			}
		}
		if (cmd->SCp.phase == PHASE_STATIN) {
			while ((AM53C974_read_8(CFIREG) & CFIREG_CF) < 2);
			cmd->SCp.Status = AM53C974_read_8(FFREG);
			cmd->SCp.Message = AM53C974_read_8(FFREG);
			DEB_INFO(printk("Message-In phase; status=0x%02x, message=0x%02x\n",
				     cmd->SCp.Status, cmd->SCp.Message));
			ret = AM53C974_message(instance, cmd, cmd->SCp.Message);
		} else {
			if (!cfifo) {
				AM53C974_write_8(CMDREG, CMDREG_IT);
				AM53C974_poll_int();
				cmd->SCp.Message = AM53C974_read_8(FFREG);
			}
			ret = AM53C974_message(instance, cmd, cmd->SCp.Message);
		}
		cmd->SCp.phase = PHASE_MSGIN;
		AM53C974_set_sync(instance, cmd->target);
		break;
		case PHASE_MSGOUT:
		    DEB_INFO(printk("Message-Out phase; cfifo=%d; msgout[0]=0x%02x\n",
				    AM53C974_read_8(CFIREG) & CFIREG_CF, hostdata->msgout[0]));
		AM53C974_write_8(DMACMD, DMACMD_IDLE);
		AM53C974_set_async(instance, cmd->target);
		for (i = 0; i < sizeof(hostdata->last_message); i++)
			hostdata->last_message[i] = hostdata->msgout[i];
		if ((hostdata->msgout[0] == 0) || INSIDE(hostdata->msgout[0], 0x02, 0x1F) ||
		    INSIDE(hostdata->msgout[0], 0x80, 0xFF))
			len = 1;
		else {
			if (hostdata->msgout[0] == EXTENDED_MESSAGE) {
#ifdef AM53C974_DEBUG_INFO
				printk("Extended message dump:\n");
				for (i = 0; i < hostdata->msgout[1] + 2; i++) {
					printk("%02x ", hostdata->msgout[i]);
					if (i && !(i % 16))
						printk("\n");
				}
				printk("\n");
#endif
				len = hostdata->msgout[1] + 2;
			} else
				len = 2;
		}
		for (i = 0; i < len; i++)
			AM53C974_write_8(FFREG, hostdata->msgout[i]);
		AM53C974_write_8(CMDREG, CMDREG_IT);
		cmd->SCp.phase = PHASE_MSGOUT;
		hostdata->msgout[0] = NOP;
		AM53C974_set_sync(instance, cmd->target);
		break;

		case PHASE_CMDOUT:
		    DEB_INFO(printk("Command-Out phase\n"));
		AM53C974_set_async(instance, cmd->target);
		for (i = 0; i < cmd->cmd_len; i++)
			AM53C974_write_8(FFREG, cmd->cmnd[i]);
		AM53C974_write_8(CMDREG, CMDREG_IT);
		cmd->SCp.phase = PHASE_CMDOUT;
		AM53C974_set_sync(instance, cmd->target);
		break;

		case PHASE_STATIN:
		    DEB_INFO(printk("Status phase\n"));
		if (cmd->SCp.phase == PHASE_DATAIN)
			AM53C974_dma_blast(instance, dmastatus, statreg);
		AM53C974_set_async(instance, cmd->target);
		if (cmd->SCp.phase == PHASE_DATAOUT) {
			unsigned long residual;

			if (AM53C974_read_8(DMACMD) & DMACMD_START) {
				AM53C974_write_8(DMACMD, DMACMD_IDLE);
				residual = cfifo + (AM53C974_read_8(CTCLREG) | (AM53C974_read_8(CTCMREG) << 8) |
				       (AM53C974_read_8(CTCHREG) << 16));
				cmd->SCp.ptr += cmd->SCp.this_residual - residual;
				cmd->SCp.this_residual = residual;
			}
			if (cfifo) {
				AM53C974_write_8(CMDREG, CMDREG_CFIFO);
				cfifo = 0;
			}
		}
		cmd->SCp.phase = PHASE_STATIN;
		AM53C974_write_8(CMDREG, CMDREG_ICCS);	/* command complete */
		break;

		case PHASE_RES_0:
		    case PHASE_RES_1:
#ifdef AM53C974_DEBUG
		    deb_stop = 1;
#endif
		DEB_INFO(printk("Reserved phase\n"));
		break;
	}
	KEYWAIT();
}

/******************************************************************************
* Function : int AM53C974_message(struct Scsi_Host *instance, Scsi_Cmnd *cmd,
*                                 unsigned char msg)                
*
* Purpose : handle SCSI messages
*
* Inputs : instance -- which AM53C974
*          cmd -- SCSI command the message belongs to
*          msg -- message id byte
* 
* Returns : 1 on success, 0 on failure.
**************************************************************************/
static int AM53C974_message(struct Scsi_Host *instance, Scsi_Cmnd * cmd,
			    unsigned char msg)
{
	AM53C974_local_declare();
	static unsigned char extended_msg[10];
	unsigned char statreg;
	int len, ret = 0;
	unsigned char *p;
#ifdef AM53C974_DEBUG_MSG
	int j;
#endif
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	AM53C974_setio(instance);

	DEB_MSG(printk(SEPARATOR_LINE));

/* Linking lets us reduce the time required to get the 
 * next command out to the device, hopefully this will
 * mean we don't waste another revolution due to the delays
 * required by ARBITRATION and another SELECTION.
 * In the current implementation proposal, low level drivers
 * merely have to start the next command, pointed to by 
 * next_link, done() is called as with unlinked commands. */
	switch (msg) {
#ifdef LINKED
		case LINKED_CMD_COMPLETE:
		    case LINKED_FLG_CMD_COMPLETE:
		/* Accept message by releasing ACK */
		    DEB_LINKED(printk("scsi%d : target %d lun %d linked command complete.\n",
			      instance->host_no, cmd->target, cmd->lun));
		/* Sanity check : A linked command should only terminate with
		 * one of these messages if there are more linked commands available. */
		if (!cmd->next_link) {
			printk("scsi%d : target %d lun %d linked command complete, no next_link\n"
			       instance->host_no, cmd->target, cmd->lun);
			hostdata->aborted = 1;
			AM53C974_write_8(CMDREG, CMDREG_SATN);
			AM53C974_write_8(CMDREG, CMDREG_MA);
			break;
		}
		if (hostdata->aborted) {
			DEB_ABORT(printk("ATN set for cmnd %d upon reception of LINKED_CMD_COMPLETE or"
					 "LINKED_FLG_CMD_COMPLETE message\n", cmd->cmnd[0]));
			AM53C974_write_8(CMDREG, CMDREG_SATN);
		}
		AM53C974_write_8(CMDREG, CMDREG_MA);

		initialize_SCp(cmd->next_link);
		/* The next command is still part of this process */
		cmd->next_link->tag = cmd->tag;
		cmd->result = cmd->SCp.Status | (cmd->SCp.Message << 8);
		DEB_LINKED(printk("scsi%d : target %d lun %d linked request done, calling scsi_done().\n",
			      instance->host_no, cmd->target, cmd->lun));
		cmd->scsi_done(cmd);
		cmd = hostdata->connected;
		break;

#endif				/* def LINKED */

		case ABORT:
		    case COMMAND_COMPLETE:
		    DEB_MSG(printk("scsi%d: command complete message received; cmd %d for target %d, lun %d\n",
		instance->host_no, cmd->cmnd[0], cmd->target, cmd->lun));
		hostdata->disconnecting = 1;
		cmd->device->disconnect = 0;

		/* I'm not sure what the correct thing to do here is : 

		 * If the command that just executed is NOT a request 
		 * sense, the obvious thing to do is to set the result
		 * code to the values of the stored parameters.
		 * If it was a REQUEST SENSE command, we need some way 
		 * to differentiate between the failure code of the original
		 * and the failure code of the REQUEST sense - the obvious
		 * case is success, where we fall through and leave the result
		 * code unchanged.
		 * 
		 * The non-obvious place is where the REQUEST SENSE failed  */
		if (cmd->cmnd[0] != REQUEST_SENSE)
			cmd->result = cmd->SCp.Status | (cmd->SCp.Message << 8);
		else if (cmd->SCp.Status != GOOD)
			cmd->result = (cmd->result & 0x00ffff) | (DID_ERROR << 16);
		if (hostdata->aborted) {
			AM53C974_write_8(CMDREG, CMDREG_SATN);
			AM53C974_write_8(CMDREG, CMDREG_MA);
			DEB_ABORT(printk("ATN set for cmnd %d upon reception of ABORT or"
			    "COMMAND_COMPLETE message\n", cmd->cmnd[0]));
			break;
		}
		if ((cmd->cmnd[0] != REQUEST_SENSE) && (cmd->SCp.Status == CHECK_CONDITION)) {
			DEB_MSG(printk("scsi%d : performing request sense\n", instance->host_no));
			cmd->cmnd[0] = REQUEST_SENSE;
			cmd->cmnd[1] &= 0xe0;
			cmd->cmnd[2] = 0;
			cmd->cmnd[3] = 0;
			cmd->cmnd[4] = sizeof(cmd->sense_buffer);
			cmd->cmnd[5] = 0;
			cmd->SCp.buffer = NULL;
			cmd->SCp.buffers_residual = 0;
			cmd->SCp.ptr = (char *) cmd->sense_buffer;
			cmd->SCp.this_residual = sizeof(cmd->sense_buffer);
			LIST(cmd, hostdata->issue_queue);
			cmd->host_scribble = (unsigned char *) hostdata->issue_queue;
			hostdata->issue_queue = (Scsi_Cmnd *) cmd;
			DEB_MSG(printk("scsi%d : REQUEST SENSE added to head of issue queue\n", instance->host_no));
		}
		/* Accept message by clearing ACK */
		AM53C974_write_8(CMDREG, CMDREG_MA);
		break;

		case MESSAGE_REJECT:
		    DEB_MSG(printk("scsi%d: reject message received; cmd %d for target %d, lun %d\n",
		instance->host_no, cmd->cmnd[0], cmd->target, cmd->lun));
		switch (hostdata->last_message[0]) {
			case EXTENDED_MESSAGE:
			    if (hostdata->last_message[2] == EXTENDED_SDTR) {
				/* sync. negotiation was rejected, setup asynchronous transfer with target */
				printk("\ntarget %d: rate=%d Mhz, asynchronous (sync. negotiation rejected)\n",
				       cmd->target, DEF_CLK / DEF_STP);
				hostdata->sync_off[cmd->target] = 0;
				hostdata->sync_per[cmd->target] = DEF_STP;
			}
			break;
			case HEAD_OF_QUEUE_TAG:
			    case ORDERED_QUEUE_TAG:
			    case SIMPLE_QUEUE_TAG:
			    cmd->device->tagged_queue = 0;
			hostdata->busy[cmd->target] |= (1 << cmd->lun);
			break;
			default:
			    break;
		}
		if (hostdata->aborted)
			AM53C974_write_8(CMDREG, CMDREG_SATN);
		AM53C974_write_8(CMDREG, CMDREG_MA);
		break;

		case DISCONNECT:
		    DEB_MSG(printk("scsi%d: disconnect message received; cmd %d for target %d, lun %d\n",
		instance->host_no, cmd->cmnd[0], cmd->target, cmd->lun));
		cmd->device->disconnect = 1;
		hostdata->disconnecting = 1;
		AM53C974_write_8(CMDREG, CMDREG_MA);	/* Accept message by clearing ACK */
		break;

		case SAVE_POINTERS:
		    case RESTORE_POINTERS:
		    DEB_MSG(printk("scsi%d: save/restore pointers message received; cmd %d for target %d, lun %d\n",
		instance->host_no, cmd->cmnd[0], cmd->target, cmd->lun));
		/* The SCSI data pointer is *IMPLICITLY* saved on a disconnect
		 * operation, in violation of the SCSI spec so we can safely 
		 * ignore SAVE/RESTORE pointers calls.
		 *
		 * Unfortunately, some disks violate the SCSI spec and 
		 * don't issue the required SAVE_POINTERS message before
		 * disconnecting, and we have to break spec to remain 
		 * compatible. */
		if (hostdata->aborted) {
			DEB_ABORT(printk("ATN set for cmnd %d upon reception of SAVE/REST. POINTERS message\n",
					 cmd->cmnd[0]));
			AM53C974_write_8(CMDREG, CMDREG_SATN);
		}
		AM53C974_write_8(CMDREG, CMDREG_MA);
		break;

		case EXTENDED_MESSAGE:
		    DEB_MSG(printk("scsi%d: extended message received; cmd %d for target %d, lun %d\n",
		instance->host_no, cmd->cmnd[0], cmd->target, cmd->lun));
		/* Extended messages are sent in the following format :
		 * Byte    
		 * 0           EXTENDED_MESSAGE == 1
		 * 1           length (includes one byte for code, doesn't include first two bytes)
		 * 2           code
		 * 3..length+1 arguments
		 */
		/* BEWARE!! THIS CODE IS EXTREMELY UGLY */
		extended_msg[0] = EXTENDED_MESSAGE;
		AM53C974_read_8(INSTREG);	/* clear int */
		AM53C974_write_8(CMDREG, CMDREG_MA);	/* ack. msg byte, then wait for SO */
		AM53C974_poll_int();
		/* get length */
		AM53C974_write_8(CMDREG, CMDREG_IT);
		AM53C974_poll_int();
		AM53C974_write_8(CMDREG, CMDREG_MA);	/* ack. msg byte, then wait for SO */
		AM53C974_poll_int();
		extended_msg[1] = len = AM53C974_read_8(FFREG);		/* get length */
		p = extended_msg + 2;
		/* read the remaining (len) bytes */
		while (len) {
			AM53C974_write_8(CMDREG, CMDREG_IT);
			AM53C974_poll_int();
			if (len > 1) {
				AM53C974_write_8(CMDREG, CMDREG_MA);	/* ack. msg byte, then wait for SO */
				AM53C974_poll_int();
			}
			*p = AM53C974_read_8(FFREG);
			p++;
			len--;
		}

#ifdef AM53C974_DEBUG_MSG
		printk("scsi%d: received extended message: ", instance->host_no);
		for (j = 0; j < extended_msg[1] + 2; j++) {
			printk("0x%02x ", extended_msg[j]);
			if (j && !(j % 16))
				printk("\n");
		}
		printk("\n");
#endif

		/* check message */
		if (extended_msg[2] == EXTENDED_SDTR)
			ret = AM53C974_sync_neg(instance, cmd->target, extended_msg);
		if (ret || hostdata->aborted)
			AM53C974_write_8(CMDREG, CMDREG_SATN);

		AM53C974_write_8(CMDREG, CMDREG_MA);
		break;

		default:
		    printk("scsi%d: unknown message 0x%02x received\n", instance->host_no, msg);
#ifdef AM53C974_DEBUG
		deb_stop = 1;
#endif
		/* reject message */
		hostdata->msgout[0] = MESSAGE_REJECT;
		AM53C974_write_8(CMDREG, CMDREG_SATN);
		AM53C974_write_8(CMDREG, CMDREG_MA);
		return (0);
		break;

	}			/* switch (msg) */
	KEYWAIT();
	return (1);
}

/************************************************************************** 
* Function : AM53C974_select(struct Scsi_Host *instance, Scsi_Cmnd *cmd, int tag)
*
* Purpose : try to establish nexus for the command;
*           start sync negotiation via start stop and transfer the command in 
*           cmdout phase in case of an inquiry or req. sense command with no 
*           sync. neg. performed yet
*
* Inputs : instance -- which AM53C974
*          cmd -- command which requires the selection
*          tag -- tagged queueing
* 
* Returns : nothing
*        
* Note: this function initializes the selection process, which is continued 
*       in the interrupt handler
**************************************************************************/
static void AM53C974_select(struct Scsi_Host *instance, Scsi_Cmnd * cmd, int tag)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	unsigned char cfifo, tmp[3];
	unsigned int i, len, cmd_size = COMMAND_SIZE(cmd->cmnd[0]);
	AM53C974_setio(instance);

	cfifo = AM53C974_cfifo();
	if (cfifo) {
		printk("scsi%d: select error; %d residual bytes in FIFO\n", instance->host_no, cfifo);
		AM53C974_write_8(CMDREG, CMDREG_CFIFO);		/* clear FIFO */
	}
#ifdef AM53C974_PROHIBIT_DISCONNECT
	tmp[0] = IDENTIFY(0, cmd->lun);
#else
	tmp[0] = IDENTIFY(1, cmd->lun);
#endif

#ifdef SCSI2
	if (cmd->device->tagged_queue && (tag != TAG_NONE)) {
		tmp[1] = SIMPLE_QUEUE_TAG;
		if (tag == TAG_NEXT) {
			/* 0 is TAG_NONE, used to imply no tag for this command */
			if (cmd->device->current_tag == 0)
				cmd->device->current_tag = 1;
			cmd->tag = cmd->device->current_tag;
			cmd->device->current_tag++;
		} else
			cmd->tag = (unsigned char) tag;
		tmp[2] = cmd->tag;
		hostdata->last_message[0] = SIMPLE_QUEUE_TAG;
		len = 3;
		AM53C974_write_8(FFREG, tmp[0]);
		AM53C974_write_8(FFREG, tmp[1]);
		AM53C974_write_8(FFREG, tmp[2]);
	} else
#endif				/* def SCSI2 */
	{
		len = 1;
		AM53C974_write_8(FFREG, tmp[0]);
		cmd->tag = 0;
	}

/* in case of an inquiry or req. sense command with no sync. neg performed yet, we start
   sync negotiation via start stops and transfer the command in cmdout phase */
	if (((cmd->cmnd[0] == INQUIRY) || (cmd->cmnd[0] == REQUEST_SENSE)) &&
	    !(hostdata->sync_neg[cmd->target]) && hostdata->sync_en[cmd->target]) {
		hostdata->sync_neg[cmd->target] = 1;
		hostdata->msgout[0] = EXTENDED_MESSAGE;
		hostdata->msgout[1] = 3;
		hostdata->msgout[2] = EXTENDED_SDTR;
		hostdata->msgout[3] = 250 / (int) hostdata->max_rate[cmd->target];
		hostdata->msgout[4] = hostdata->max_offset[cmd->target];
		len += 5;
	}
	AM53C974_write_8(SDIDREG, SDIREG_MASK & cmd->target);	/* setup dest. id  */
	AM53C974_write_8(STIMREG, DEF_SCSI_TIMEOUT);	/* setup timeout reg */
	switch (len) {
	case 1:
		for (i = 0; i < cmd_size; i++)
			AM53C974_write_8(FFREG, cmd->cmnd[i]);
		AM53C974_write_8(CMDREG, CMDREG_SAS);	/* select with ATN, 1 msg byte */
		hostdata->msgout[0] = NOP;
		break;
	case 3:
		for (i = 0; i < cmd_size; i++)
			AM53C974_write_8(FFREG, cmd->cmnd[i]);
		AM53C974_write_8(CMDREG, CMDREG_SA3S);	/* select with ATN, 3 msg bytes */
		hostdata->msgout[0] = NOP;
		break;
		default:
		    AM53C974_write_8(CMDREG, CMDREG_SASS);	/* select with ATN, stop steps; continue in message out phase */
		break;
	}
}

/************************************************************************** 
* Function : AM53C974_intr_select(struct Scsi_Host *instance, unsigned char statreg)
*
* Purpose : handle reselection 
*
* Inputs : instance -- which AM53C974
*          statreg -- status register
* 
* Returns : nothing
*
* side effects: manipulates hostdata
**************************************************************************/
static void AM53C974_intr_reselect(struct Scsi_Host *instance, unsigned char statreg)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	unsigned char cfifo, msg[3], lun, t, target = 0;
#ifdef SCSI2
	unsigned char tag;
#endif
	Scsi_Cmnd *tmp = NULL, *prev;
	AM53C974_setio(instance);

	cfifo = AM53C974_cfifo();

	if (hostdata->selecting) {
		/* caught reselect interrupt in selection process;
		   put selecting command back into the issue queue and continue with the
		   reselecting command */
		DEB_RESEL(printk("AM53C974_intr_reselect: in selection process\n"));
		LIST(hostdata->sel_cmd, hostdata->issue_queue);
		hostdata->sel_cmd->host_scribble = (unsigned char *) hostdata->issue_queue;
		hostdata->issue_queue = hostdata->sel_cmd;
		hostdata->sel_cmd = NULL;
		hostdata->selecting = 0;
	}
/* 2 bytes must be in the FIFO now */
	if (cfifo != 2) {
		printk("scsi %d: error: %d bytes in fifo, 2 expected\n", instance->host_no, cfifo);
		hostdata->aborted = 1;
		goto EXIT_ABORT;
	}
/* determine target which reselected */
	t = AM53C974_read_8(FFREG);
	if (!(t & (1 << instance->this_id))) {
		printk("scsi %d: error: invalid host id\n", instance->host_no);
		hostdata->aborted = 1;
		goto EXIT_ABORT;
	}
	t ^= (1 << instance->this_id);
	target = 0;
	while (t != 1) {
		t >>= 1;
		target++;
	}
	DEB_RESEL(printk("scsi %d: reselect; target: %d\n", instance->host_no, target));

	if (hostdata->aborted)
		goto EXIT_ABORT;

	if ((statreg & STATREG_PHASE) != PHASE_MSGIN) {
		printk("scsi %d: error: upon reselection interrupt not in MSGIN\n", instance->host_no);
		hostdata->aborted = 1;
		goto EXIT_ABORT;
	}
	msg[0] = AM53C974_read_8(FFREG);
	if (!msg[0] & 0x80) {
		printk("scsi%d: error: expecting IDENTIFY message, got ", instance->host_no);
		print_msg(msg);
		hostdata->aborted = 1;
		goto EXIT_ABORT;
	}
	lun = (msg[0] & 0x07);

/* We need to add code for SCSI-II to track which devices have
 * I_T_L_Q nexuses established, and which have simple I_T_L
 * nexuses so we can chose to do additional data transfer. */
#ifdef SCSI2
#error "SCSI-II tagged queueing is not supported yet"
#endif

/* Find the command corresponding to the I_T_L or I_T_L_Q  nexus we 
 * just reestablished, and remove it from the disconnected queue. */
	for (tmp = (Scsi_Cmnd *) hostdata->disconnected_queue, prev = NULL;
	     tmp; prev = tmp, tmp = (Scsi_Cmnd *) tmp->host_scribble)
		if ((target == tmp->target) && (lun == tmp->lun)
#ifdef SCSI2
		    && (tag == tmp->tag)
#endif
		    ) {
			if (prev) {
				REMOVE(prev, (Scsi_Cmnd *) (prev->host_scribble), tmp,
				     (Scsi_Cmnd *) (tmp->host_scribble));
				prev->host_scribble = tmp->host_scribble;
			} else {
				REMOVE(-1, hostdata->disconnected_queue, tmp, tmp->host_scribble);
				hostdata->disconnected_queue = (Scsi_Cmnd *) tmp->host_scribble;
			}
			tmp->host_scribble = NULL;
			hostdata->connected = tmp;
			break;
		}
	if (!tmp) {
#ifdef SCSI2
		printk("scsi%d: warning : target %d lun %d tag %d not in disconnect_queue.\n",
		       instance->host_no, target, lun, tag);
#else
		printk("scsi%d: warning : target %d lun %d not in disconnect_queue.\n",
		       instance->host_no, target, lun);
#endif
		/* Since we have an established nexus that we can't do anything with, we must abort it. */
		hostdata->aborted = 1;
		DEB(AM53C974_keywait());
		goto EXIT_ABORT;
	} else
		goto EXIT_OK;

	EXIT_ABORT:
	    AM53C974_write_8(CMDREG, CMDREG_SATN);
	AM53C974_write_8(CMDREG, CMDREG_MA);
	return;

	EXIT_OK:
	    DEB_RESEL(printk("scsi%d: nexus established, target = %d, lun = %d, tag = %d\n",
			 instance->host_no, target, tmp->lun, tmp->tag));
	AM53C974_set_sync(instance, target);
	AM53C974_write_8(SDIDREG, SDIREG_MASK & target);	/* setup dest. id  */
	AM53C974_write_8(CMDREG, CMDREG_MA);
	hostdata->dma_busy = 0;
	hostdata->connected->SCp.phase = PHASE_CMDOUT;
}

/************************************************************************** 
* Function : AM53C974_transfer_dma(struct Scsi_Host *instance, short dir,
*                                  unsigned long length, char *data)
*
* Purpose : setup DMA transfer
*
* Inputs : instance -- which AM53C974
*          dir -- direction flag, 0: write to device, read from memory; 
*                                 1: read from device, write to memory
*          length -- number of bytes to transfer to from buffer
*          data -- pointer to data buffer
* 
* Returns : nothing
**************************************************************************/
static __inline__ void AM53C974_transfer_dma(struct Scsi_Host *instance, short dir,
					unsigned long length, char *data)
{
	AM53C974_local_declare();
	AM53C974_setio(instance);

	AM53C974_write_8(CMDREG, CMDREG_NOP);
	AM53C974_write_8(DMACMD, (dir << 7) | DMACMD_INTE_D);	/* idle command */
	AM53C974_write_8(STCLREG, (unsigned char) (length & 0xff));
	AM53C974_write_8(STCMREG, (unsigned char) ((length & 0xff00) >> 8));
	AM53C974_write_8(STCHREG, (unsigned char) ((length & 0xff0000) >> 16));
	AM53C974_write_32(DMASTC, length & 0xffffff);
	AM53C974_write_32(DMASPA, virt_to_bus(data));
	AM53C974_write_8(CMDREG, CMDREG_IT | CMDREG_DMA);
	AM53C974_write_8(DMACMD, (dir << 7) | DMACMD_INTE_D | DMACMD_START);
}

/************************************************************************** 
* Function : AM53C974_dma_blast(struct Scsi_Host *instance, unsigned char dmastatus,
*                               unsigned char statreg)
*
* Purpose : cleanup DMA transfer
*
* Inputs : instance -- which AM53C974
*          dmastatus -- dma status register
*          statreg -- status register
* 
* Returns : nothing
**************************************************************************/
static void AM53C974_dma_blast(struct Scsi_Host *instance, unsigned char dmastatus,
			       unsigned char statreg)
{
	AM53C974_local_declare();
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	unsigned long ctcreg;
	int dir = statreg & STATREG_IO;
	int cfifo, pio, i = 0;
	AM53C974_setio(instance);

	do {
		cfifo = AM53C974_cfifo();
		i++;
	} while (cfifo && (i < 50000));
	pio = (i == 50000) ? 1 : 0;

	if (statreg & STATREG_CTZ) {
		AM53C974_write_8(DMACMD, DMACMD_IDLE);
		return;
	}
	if (dmastatus & DMASTATUS_DONE) {
		AM53C974_write_8(DMACMD, DMACMD_IDLE);
		return;
	}
	AM53C974_write_8(DMACMD, ((dir << 7) & DMACMD_DIR) | DMACMD_BLAST);
	while (!(AM53C974_read_8(DMASTATUS) & DMASTATUS_BCMPLT));
	AM53C974_write_8(DMACMD, DMACMD_IDLE);

	if (pio) {
		/* transfer residual bytes via PIO */
		unsigned char *wac = (unsigned char *) AM53C974_read_32(DMAWAC);
		printk("pio mode, residual=%d\n", AM53C974_read_8(CFIREG) & CFIREG_CF);
		while (AM53C974_read_8(CFIREG) & CFIREG_CF)
			*(wac++) = AM53C974_read_8(FFREG);
	}
	ctcreg = AM53C974_read_8(CTCLREG) | (AM53C974_read_8(CTCMREG) << 8) |
	    (AM53C974_read_8(CTCHREG) << 16);

	hostdata->connected->SCp.ptr += hostdata->connected->SCp.this_residual - ctcreg;
	hostdata->connected->SCp.this_residual = ctcreg;
}

/************************************************************************** 
* Function : AM53C974_intr_bus_reset(struct Scsi_Host *instance)
*
* Purpose : handle bus reset interrupt
*
* Inputs : instance -- which AM53C974
* 
* Returns : nothing
**************************************************************************/
static void AM53C974_intr_bus_reset(struct Scsi_Host *instance)
{
	AM53C974_local_declare();
	unsigned char cntlreg1;
	AM53C974_setio(instance);

	AM53C974_write_8(CMDREG, CMDREG_CFIFO);
	AM53C974_write_8(CMDREG, CMDREG_NOP);

	cntlreg1 = AM53C974_read_8(CNTLREG1);
	AM53C974_write_8(CNTLREG1, cntlreg1 | CNTLREG1_DISR);
}

/**************************************************************************
* Function : int AM53C974_abort(Scsi_Cmnd *cmd)
*
* Purpose : abort a command
*
* Inputs : cmd - the Scsi_Cmnd to abort, code - code to set the 
* 	host byte of the result field to, if zero DID_ABORTED is 
*	used.
*
* Returns : 0 - success, -1 on failure.
 **************************************************************************/
static int AM53C974_abort(Scsi_Cmnd * cmd)
{
	AM53C974_local_declare();
	unsigned long flags;
	struct Scsi_Host *instance = cmd->host;
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	Scsi_Cmnd *tmp, **prev;

#ifdef AM53C974_DEBUG
	deb_stop = 1;
#endif
	save_flags(flags);
	cli();
	AM53C974_setio(instance);

	DEB_ABORT(printk(SEPARATOR_LINE));
	DEB_ABORT(printk("scsi%d : AM53C974_abort called -- trouble starts!!\n", instance->host_no));
	DEB_ABORT(AM53C974_print(instance));
	DEB_ABORT(AM53C974_keywait());

/* Case 1 : If the command is the currently executing command, 
   we'll set the aborted flag and return control so that the
   information transfer routine can exit cleanly. */
	if ((hostdata->connected == cmd) || (hostdata->sel_cmd == cmd)) {
		DEB_ABORT(printk("scsi%d: aborting connected command\n", instance->host_no));
		hostdata->aborted = 1;
		hostdata->msgout[0] = ABORT;
		restore_flags(flags);
		return (SCSI_ABORT_PENDING);
	}
/* Case 2 : If the command hasn't been issued yet,
   we simply remove it from the issue queue. */
	for (prev = (Scsi_Cmnd **) & (hostdata->issue_queue),
	     tmp = (Scsi_Cmnd *) hostdata->issue_queue; tmp;
	     prev = (Scsi_Cmnd **) & (tmp->host_scribble),
	     tmp = (Scsi_Cmnd *) tmp->host_scribble) {
		if (cmd == tmp) {
			DEB_ABORT(printk("scsi%d : abort removed command from issue queue.\n", instance->host_no));
			REMOVE(5, *prev, tmp, tmp->host_scribble);
			(*prev) = (Scsi_Cmnd *) tmp->host_scribble;
			tmp->host_scribble = NULL;
			tmp->result = DID_ABORT << 16;
			restore_flags(flags);
			tmp->done(tmp);
			return (SCSI_ABORT_SUCCESS);
		}
#ifdef AM53C974_DEBUG_ABORT
		else {
			if (prev == (Scsi_Cmnd **) tmp)
				printk("scsi%d : LOOP\n", instance->host_no);
		}
#endif
	}

/* Case 3 : If any commands are connected, we're going to fail the abort
 *        and let the high level SCSI driver retry at a later time or 
 *          issue a reset.
 *
 *          Timeouts, and therefore aborted commands, will be highly unlikely
 *          and handling them cleanly in this situation would make the common
 *          case of noresets less efficient, and would pollute our code.  So,
 *          we fail. */
	if (hostdata->connected || hostdata->sel_cmd) {
		DEB_ABORT(printk("scsi%d : abort failed, other command connected.\n", instance->host_no));
		restore_flags(flags);
		return (SCSI_ABORT_NOT_RUNNING);
	}
/* Case 4: If the command is currently disconnected from the bus, and 
 *       there are no connected commands, we reconnect the I_T_L or 
 *         I_T_L_Q nexus associated with it, go into message out, and send 
 *         an abort message. */
	for (tmp = (Scsi_Cmnd *) hostdata->disconnected_queue; tmp;
	     tmp = (Scsi_Cmnd *) tmp->host_scribble) {
		if (cmd == tmp) {
			DEB_ABORT(printk("scsi%d: aborting disconnected command\n", instance->host_no));
			hostdata->aborted = 1;
			hostdata->msgout[0] = ABORT;
			hostdata->selecting = 1;
			hostdata->sel_cmd = tmp;
			AM53C974_write_8(CMDREG, CMDREG_DSR);
			restore_flags(flags);
			return (SCSI_ABORT_PENDING);
		}
	}

/* Case 5 : If we reached this point, the command was not found in any of 
 *        the queues.
 *
 * We probably reached this point because of an unlikely race condition
 * between the command completing successfully and the abortion code,
 * so we won't panic, but we will notify the user in case something really
 * broke. */
	DEB_ABORT(printk("scsi%d : abort failed, command not found.\n", instance->host_no));
	restore_flags(flags);
	return (SCSI_ABORT_NOT_RUNNING);
}

/************************************************************************** 
* Function : int AM53C974_reset(Scsi_Cmnd *cmd)
*
* Purpose : reset the SCSI controller and bus
*
* Inputs : cmd -- which command within the command block was responsible for the reset
* 
* Returns : status (SCSI_ABORT_SUCCESS)
* 
* FIXME(eric) the reset_flags are ignored.
**************************************************************************/
static int AM53C974_reset(Scsi_Cmnd * cmd, unsigned int reset_flags)
{
	AM53C974_local_declare();
	unsigned long flags;
	int i;
	struct Scsi_Host *instance = cmd->host;
	struct AM53C974_hostdata *hostdata = (struct AM53C974_hostdata *) instance->hostdata;
	AM53C974_setio(instance);

	save_flags(flags);
	cli();
	DEB(printk("AM53C974_reset called; "));

	printk("AM53C974_reset called\n");
	AM53C974_print(instance);
	AM53C974_keywait();

/* do hard reset */
	AM53C974_write_8(CMDREG, CMDREG_RDEV);
	AM53C974_write_8(CMDREG, CMDREG_NOP);
	hostdata->msgout[0] = NOP;
	for (i = 0; i < 8; i++) {
		hostdata->busy[i] = 0;
		hostdata->sync_per[i] = DEF_STP;
		hostdata->sync_off[i] = 0;
		hostdata->sync_neg[i] = 0;
	}
	hostdata->last_message[0] = NOP;
	hostdata->sel_cmd = NULL;
	hostdata->connected = NULL;
	hostdata->issue_queue = NULL;
	hostdata->disconnected_queue = NULL;
	hostdata->in_reset = 0;
	hostdata->aborted = 0;
	hostdata->selecting = 0;
	hostdata->disconnecting = 0;
	hostdata->dma_busy = 0;

/* reset bus */
	AM53C974_write_8(CNTLREG1, CNTLREG1_DISR | instance->this_id);	/* disable interrupt upon SCSI RESET */
	AM53C974_write_8(CMDREG, CMDREG_RBUS);	/* reset SCSI bus */
	udelay(40);
	AM53C974_config_after_reset(instance);

	restore_flags(flags);
	cmd->result = DID_RESET << 16;
	cmd->scsi_done(cmd);
	return SCSI_ABORT_SUCCESS;
}


/*
 * AM53C974_release()
 *
 * Release resources allocated for a single AM53C974 adapter.
 */
static int AM53C974_release(struct Scsi_Host *shp)
{
	free_irq(shp->irq, shp);
	scsi_unregister(shp);
	return 0;
}


/* You can specify overrides=a,b,c,d in the same format at AM53C974=a,b,c,d
   on boot up */
MODULE_PARM(overrides, "1-32i");
MODULE_LICENSE("GPL");


static Scsi_Host_Template driver_template = AM53C974;
#include "scsi_module.c"
