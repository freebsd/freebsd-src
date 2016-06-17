/*
 * PERM_OPTIONS are driver options which will be enabled for all NCR boards
 * in the system at driver initialization time.
 *
 * Don't THINK about touching these in PERM_OPTIONS : 
 *   OPTION_IO_MAPPED 
 * 	Memory mapped IO does not work under i86 Linux. 
 *
 *   OPTION_DEBUG_TEST1
 *	Test 1 does bus mastering and interrupt tests, which will help weed 
 *	out brain damaged main boards.
 *
 * These are development kernel changes.  Code for them included in this
 * driver release may or may not work.  If you turn them on, you should be 
 * running the latest copy of the development sources from
 *
 *	ftp://tsx-11.mit.edu/pub/linux/ALPHA/scsi/53c7,8xx
 *
 * and be subscribed to the ncr53c810@colorado.edu mailing list.  To
 * subscribe, send mail to majordomo@colorado.edu with 
 *
 * 	subscribe ncr53c810
 * 
 * in the text.
 *
 *
 *   OPTION_NO_ASYNC
 *	Don't negotiate for asynchronous transfers on the first command 
 *	when OPTION_ALWAYS_SYNCHRONOUS is set.  Useful for dain bramaged
 *	devices which do something bad rather than sending a MESSAGE 
 *	REJECT back to us like they should if they can't cope.
 *
 *   OPTION_SYNCHRONOUS
 *	Enable support for synchronous transfers.  Target negotiated 
 *	synchronous transfers will be responded to.  To initiate 
 *	a synchronous transfer request,  call 
 *
 *	    request_synchronous (hostno, target) 
 *
 *	from within KGDB.
 *
 *   OPTION_ALWAYS_SYNCHRONOUS
 *	Negotiate for synchronous transfers with every target after
 *	driver initialization or a SCSI bus reset.  This is a bit dangerous, 
 *	since there are some dain bramaged SCSI devices which will accept
 *	SDTR messages but keep talking asynchronously.
 *
 *   OPTION_DISCONNECT
 *	Enable support for disconnect/reconnect.  To change the 
 *	default setting on a given host adapter, call
 *
 *	    request_disconnect (hostno, allow)
 *
 *	where allow is non-zero to allow, 0 to disallow.
 * 
 *  If you really want to run 10MHz FAST SCSI-II transfers, you should 
 *  know that the NCR driver currently ignores parity information.  Most
 *  systems do 5MHz SCSI fine.  I've seen a lot that have problems faster
 *  than 8MHz.  To play it safe, we only request 5MHz transfers.
 *
 *  If you'd rather get 10MHz transfers, edit sdtr_message and change 
 *  the fourth byte from 50 to 25.
 */

#include <linux/config.h>

#ifdef CONFIG_SCSI_NCR53C7xx_sync
#ifdef CONFIG_SCSI_NCR53C7xx_DISCONNECT
#define PERM_OPTIONS (OPTION_IO_MAPPED|OPTION_DEBUG_TEST1|OPTION_DISCONNECT|\
	OPTION_SYNCHRONOUS|OPTION_ALWAYS_SYNCHRONOUS)
#else
#define PERM_OPTIONS (OPTION_IO_MAPPED|OPTION_DEBUG_TEST1|\
	OPTION_SYNCHRONOUS|OPTION_ALWAYS_SYNCHRONOUS)
#endif
#else
#ifdef CONFIG_SCSI_NCR53C7xx_DISCONNECT
#define PERM_OPTIONS (OPTION_IO_MAPPED|OPTION_DEBUG_TEST1|OPTION_DISCONNECT|\
	OPTION_SYNCHRONOUS)
#else
#define PERM_OPTIONS (OPTION_IO_MAPPED|OPTION_DEBUG_TEST1|OPTION_SYNCHRONOUS)
#endif
#endif

/*
 * Sponsored by 
 *	iX Multiuser Multitasking Magazine
 *	Hannover, Germany
 *	hm@ix.de
 *
 * Copyright 1993, 1994, 1995 Drew Eckhardt
 *      Visionary Computing 
 *      (Unix and Linux consulting and custom programming)
 *      drew@PoohSticks.ORG
 *	+1 (303) 786-7975
 *
 * TolerANT and SCSI SCRIPTS are registered trademarks of NCR Corporation.
 * 
 * For more information, please consult 
 *
 * NCR53C810 
 * SCSI I/O Processor
 * Programmer's Guide
 *
 * NCR 53C810
 * PCI-SCSI I/O Processor
 * Data Manual
 *
 * NCR 53C810/53C820
 * PCI-SCSI I/O Processor Design In Guide
 *
 * For literature on Symbios Logic Inc. formerly NCR, SCSI, 
 * and Communication products please call (800) 334-5454 or
 * (719) 536-3300. 
 * 
 * PCI BIOS Specification Revision
 * PCI Local Bus Specification
 * PCI System Design Guide
 *
 * PCI Special Interest Group
 * M/S HF3-15A
 * 5200 N.E. Elam Young Parkway
 * Hillsboro, Oregon 97124-6497
 * +1 (503) 696-2000 
 * +1 (800) 433-5177
 */

/*
 * Design issues : 
 * The cumulative latency needed to propagate a read/write request 
 * through the file system, buffer cache, driver stacks, SCSI host, and 
 * SCSI device is ultimately the limiting factor in throughput once we 
 * have a sufficiently fast host adapter.
 *  
 * So, to maximize performance we want to keep the ratio of latency to data 
 * transfer time to a minimum by
 * 1.  Minimizing the total number of commands sent (typical command latency
 *	including drive and bus mastering host overhead is as high as 4.5ms)
 *	to transfer a given amount of data.  
 *
 *      This is accomplished by placing no arbitrary limit on the number
 *	of scatter/gather buffers supported, since we can transfer 1K
 *	per scatter/gather buffer without Eric's cluster patches, 
 *	4K with.  
 *
 * 2.  Minimizing the number of fatal interrupts serviced, since
 * 	fatal interrupts halt the SCSI I/O processor.  Basically,
 *	this means offloading the practical maximum amount of processing 
 *	to the SCSI chip.
 * 
 *	On the NCR53c810/820/720,  this is accomplished by using 
 *		interrupt-on-the-fly signals when commands complete, 
 *		and only handling fatal errors and SDTR / WDTR 	messages 
 *		in the host code.
 *
 *	On the NCR53c710, interrupts are generated as on the NCR53c8x0,
 *		only the lack of an interrupt-on-the-fly facility complicates
 *		things.   Also, SCSI ID registers and commands are 
 *		bit fielded rather than binary encoded.
 *		
 * 	On the NCR53c700 and NCR53c700-66, operations that are done via 
 *		indirect, table mode on the more advanced chips must be
 *	        replaced by calls through a jump table which 
 *		acts as a surrogate for the DSA.  Unfortunately, this 
 * 		will mean that we must service an interrupt for each 
 *		disconnect/reconnect.
 * 
 * 3.  Eliminating latency by pipelining operations at the different levels.
 * 	
 *	This driver allows a configurable number of commands to be enqueued
 *	for each target/lun combination (experimentally, I have discovered
 *	that two seems to work best) and will ultimately allow for 
 *	SCSI-II tagged queuing.
 * 	
 *
 * Architecture : 
 * This driver is built around a Linux queue of commands waiting to 
 * be executed, and a shared Linux/NCR array of commands to start.  Commands
 * are transferred to the array  by the run_process_issue_queue() function 
 * which is called whenever a command completes.
 *
 * As commands are completed, the interrupt routine is triggered,
 * looks for commands in the linked list of completed commands with
 * valid status, removes these commands from a list of running commands, 
 * calls the done routine, and flags their target/luns as not busy.
 *
 * Due to limitations in the intelligence of the NCR chips, certain
 * concessions are made.  In many cases, it is easier to dynamically 
 * generate/fix-up code rather than calculate on the NCR at run time.  
 * So, code is generated or fixed up for
 *
 * - Handling data transfers, using a variable number of MOVE instructions
 *	interspersed with CALL MSG_IN, WHEN MSGIN instructions.
 *
 * 	The DATAIN and DATAOUT routines	are separate, so that an incorrect
 *	direction can be trapped, and space isn't wasted. 
 *
 *	It may turn out that we're better off using some sort 
 *	of table indirect instruction in a loop with a variable
 *	sized table on the NCR53c710 and newer chips.
 *
 * - Checking for reselection (NCR53c710 and better)
 *
 * - Handling the details of SCSI context switches (NCR53c710 and better),
 *	such as reprogramming appropriate synchronous parameters, 
 *	removing the dsa structure from the NCR's queue of outstanding
 *	commands, etc.
 *
 */

/* 
 * Accommodate differences between stock 1.2.x and 1.3.x asm-i386/types.h
 * so lusers can drop in 53c7,8xx.* and get something which compiles 
 * without warnings.
 */

#include <linux/version.h>

#include <linux/module.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/time.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/spinlock.h>

#include "scsi.h"
#include "hosts.h"
#include "53c7,8xx.h"
#include "constants.h"
#include "sd.h"
#include <linux/stat.h>
#include <linux/stddef.h>

static int check_address (unsigned long addr, int size);
static void dump_events (struct Scsi_Host *host, int count);
static Scsi_Cmnd * return_outstanding_commands (struct Scsi_Host *host, 
    int free, int issue);
static void hard_reset (struct Scsi_Host *host);
static void ncr_scsi_reset (struct Scsi_Host *host);
static void print_lots (struct Scsi_Host *host);
static void set_synchronous (struct Scsi_Host *host, int target, int sxfer, 
    int scntl3, int now_connected);
static int datapath_residual (struct Scsi_Host *host);
static const char * sbcl_to_phase (int sbcl);
static void print_progress (Scsi_Cmnd *cmd);
static void print_queues (struct Scsi_Host *host);
static void process_issue_queue (unsigned long flags);
static int shutdown (struct Scsi_Host *host);
static void abnormal_finished (struct NCR53c7x0_cmd *cmd, int result);
static int disable (struct Scsi_Host *host);
static int NCR53c8xx_run_tests (struct Scsi_Host *host);
static int NCR53c8xx_script_len;
static int NCR53c8xx_dsa_len;
static void NCR53c7x0_intr(int irq, void *dev_id, struct pt_regs * regs);
static void do_NCR53c7x0_intr(int irq, void *dev_id, struct pt_regs * regs);
static int ncr_halt (struct Scsi_Host *host);
static void intr_phase_mismatch (struct Scsi_Host *host, struct NCR53c7x0_cmd 
    *cmd);
static void intr_dma (struct Scsi_Host *host, struct NCR53c7x0_cmd *cmd);
static void print_dsa (struct Scsi_Host *host, u32 *dsa,
    const char *prefix);
static int print_insn (struct Scsi_Host *host, const u32 *insn,
    const char *prefix, int kernel);

static void NCR53c8xx_dsa_fixup (struct NCR53c7x0_cmd *cmd);
static void NCR53c8x0_init_fixup (struct Scsi_Host *host);
static int NCR53c8x0_dstat_sir_intr (struct Scsi_Host *host, struct 
    NCR53c7x0_cmd *cmd);
static void NCR53c8x0_soft_reset (struct Scsi_Host *host);

/* INSMOD variables */
static long long perm_options = PERM_OPTIONS;
/* 14 = .5s; 15 is max; decreasing divides by two. */
static int selection_timeout = 14;
/* Size of event list (per host adapter) */
static int track_events = 0;

static struct Scsi_Host *first_host = NULL;	/* Head of list of NCR boards */
static Scsi_Host_Template *the_template = NULL;	

/*
 * KNOWN BUGS :
 * - There is some sort of conflict when the PPP driver is compiled with 
 * 	support for 16 channels?
 * 
 * - On systems which predate the 1.3.x initialization order change,
 *      the NCR driver will cause Cannot get free page messages to appear.  
 *      These are harmless, but I don't know of an easy way to avoid them.
 *
 * - With OPTION_DISCONNECT, on two systems under unknown circumstances,
 *	we get a PHASE MISMATCH with DSA set to zero (suggests that we 
 *	are occurring somewhere in the reselection code) where 
 *	DSP=some value DCMD|DBC=same value.  
 * 	
 *	Closer inspection suggests that we may be trying to execute
 *	some portion of the DSA?
 * scsi0 : handling residual transfer (+ 0 bytes from DMA FIFO)
 * scsi0 : handling residual transfer (+ 0 bytes from DMA FIFO)
 * scsi0 : no current command : unexpected phase MSGIN.
 *         DSP=0x1c46cc, DCMD|DBC=0x1c46ac, DSA=0x0
 *         DSPS=0x0, TEMP=0x1c3e70, DMODE=0x80
 * scsi0 : DSP->
 * 001c46cc : 0x001c46cc 0x00000000
 * 001c46d4 : 0x001c5ea0 0x000011f8
 *
 *	Changed the print code in the phase_mismatch handler so
 *	that we call print_lots to try to diagnose this.
 *
 */

/* 
 * Possible future direction of architecture for max performance :
 *
 * We're using a single start array for the NCR chip.  This is 
 * sub-optimal, because we cannot add a command which would conflict with 
 * an executing command to this start queue, and therefore must insert the 
 * next command for a given I/T/L combination after the first has completed;
 * incurring our interrupt latency between SCSI commands.
 *
 * To allow further pipelining of the NCR and host CPU operation, we want 
 * to set things up so that immediately on termination of a command destined 
 * for a given LUN, we get that LUN busy again.  
 * 
 * To do this, we need to add a 32 bit pointer to which is jumped to 
 * on completion of a command.  If no new command is available, this 
 * would point to the usual DSA issue queue select routine.
 *
 * If one were, it would point to a per-NCR53c7x0_cmd select routine 
 * which starts execution immediately, inserting the command at the head 
 * of the start queue if the NCR chip is selected or reselected.
 *
 * We would change so that we keep a list of outstanding commands 
 * for each unit, rather than a single running_list.  We'd insert 
 * a new command into the right running list; if the NCR didn't 
 * have something running for that yet, we'd put it in the 
 * start queue as well.  Some magic needs to happen to handle the 
 * race condition between the first command terminating before the 
 * new one is written.
 *
 * Potential for profiling : 
 * Call do_gettimeofday(struct timeval *tv) to get 800ns resolution.
 */


/*
 * TODO : 
 * 1.  To support WIDE transfers, not much needs to happen.  We
 *	should do CHMOVE instructions instead of MOVEs when
 *	we have scatter/gather segments of uneven length.  When
 * 	we do this, we need to handle the case where we disconnect
 *	between segments.
 * 
 * 2.  Currently, when Icky things happen we do a FATAL().  Instead,
 *     we want to do an integrity check on the parts of the NCR hostdata
 *     structure which were initialized at boot time; FATAL() if that 
 *     fails, and otherwise try to recover.  Keep track of how many
 *     times this has happened within a single SCSI command; if it 
 *     gets excessive, then FATAL().
 *
 * 3.  Parity checking is currently disabled, and a few things should 
 *     happen here now that we support synchronous SCSI transfers :
 *     1.  On soft-reset, we should set the EPC (Enable Parity Checking)
 *	   and AAP (Assert SATN/ on parity error) bits in SCNTL0.
 *	
 *     2.  We should enable the parity interrupt in the SIEN0 register.
 * 
 *     3.  intr_phase_mismatch() needs to believe that message out is 
 *	   always an "acceptable" phase to have a mismatch in.  If 
 *	   the old phase was MSG_IN, we should send a MESSAGE PARITY 
 *	   error.  If the old phase was something else, we should send
 *	   a INITIATOR_DETECTED_ERROR message.  Note that this could
 *	   cause a RESTORE POINTERS message; so we should handle that 
 *	   correctly first.  Instead, we should probably do an 
 *	   initiator_abort.
 *
 * 4.  MPEE bit of CTEST4 should be set so we get interrupted if 
 *     we detect an error.
 *
 *  
 * 5.  The initial code has been tested on the NCR53c810.  I don't 
 *     have access to NCR53c700, 700-66 (Forex boards), NCR53c710
 *     (NCR Pentium systems), NCR53c720, NCR53c820, or NCR53c825 boards to 
 *     finish development on those platforms.
 *
 *     NCR53c820/825/720 - need to add wide transfer support, including WDTR 
 *     		negotiation, programming of wide transfer capabilities
 *		on reselection and table indirect selection.
 *
 *     NCR53c710 - need to add fatal interrupt or GEN code for 
 *		command completion signaling.   Need to modify all 
 *		SDID, SCID, etc. registers, and table indirect select code 
 *		since these use bit fielded (ie 1<<target) instead of 
 *		binary encoded target ids.  Need to accommodate
 *		different register mappings, probably scan through
 *		the SCRIPT code and change the non SFBR register operand
 *		of all MOVE instructions.
 * 
 *     NCR53c700/700-66 - need to add code to refix addresses on 
 *		every nexus change, eliminate all table indirect code,
 *		very messy.
 *
 * 6.  The NCR53c7x0 series is very popular on other platforms that 
 *     could be running Linux - ie, some high performance AMIGA SCSI 
 *     boards use it.  
 *	
 *     So, I should include #ifdef'd code so that it is 
 *     compatible with these systems.
 *	
 *     Specifically, the little Endian assumptions I made in my 
 *     bit fields need to change, and if the NCR doesn't see memory
 *     the right way, we need to provide options to reverse words
 *     when the scripts are relocated.
 *
 * 7.  Use ioremap() to access memory mapped boards.  
 */

/* 
 * Allow for simultaneous existence of multiple SCSI scripts so we 
 * can have a single driver binary for all of the family.
 *
 * - one for NCR53c700 and NCR53c700-66 chips	(not yet supported)
 * - one for rest (only the NCR53c810, 815, 820, and 825 are currently 
 *	supported)
 * 
 * So that we only need two SCSI scripts, we need to modify things so
 * that we fixup register accesses in READ/WRITE instructions, and 
 * we'll also have to accommodate the bit vs. binary encoding of IDs
 * with the 7xx chips.
 */

/*
 * Use pci_chips_ids to translate in both directions between PCI device ID 
 * and chip numbers.  
 */

static struct {
    unsigned short pci_device_id;
    int chip;
/* 
 * The revision field of the PCI_CLASS_REVISION register is compared 
 * against each of these fields if the field is not -1.  If it
 * is less than min_revision or larger than max_revision, a warning
 * message is printed.
 */
    int max_revision;
    int min_revision;
} pci_chip_ids[] = { 
    {PCI_DEVICE_ID_NCR_53C810, 810, 2, 1}, 
    {PCI_DEVICE_ID_NCR_53C815, 815, 3, 2},
    {PCI_DEVICE_ID_NCR_53C820, 820, -1, -1},
    {PCI_DEVICE_ID_NCR_53C825, 825, -1, -1}
};

#define NPCI_CHIP_IDS (sizeof (pci_chip_ids) / sizeof(pci_chip_ids[0]))

#define ROUNDUP(adr,type)	\
  ((void *) (((long) (adr) + sizeof(type) - 1) & ~(sizeof(type) - 1)))

/* 
 * Forced detection and autoprobe code for various hardware.  Currently, 
 * entry points for these are not included in init/main.c because if the 
 * PCI BIOS code isn't working right, you're not going to be able to use 
 * the hardware anyways; this way we force users to solve their 
 * problems rather than forcing detection and blaming us when it 
 * does not work.
 */

static struct override {
    int chip;	/* 700, 70066, 710, 720, 810, 820 */
    int board;	/* Any special board level gunk */
    unsigned pci:1;
    union {
	struct {
	    int base;	/* Memory address - indicates memory mapped regs */
	    int io_port;/* I/O port address - indicates I/O mapped regs */
    	    int irq;	/* IRQ line */		
    	    int dma;	/* DMA channel 		- often none */
	} normal;
	struct {
	    int bus;
	    int device;
	    int function;
	} pci;
    } data;
    long long options;
} overrides [4] = {{0,},};
static int commandline_current = 0;
static int no_overrides = 0;

#if 0
#define OVERRIDE_LIMIT (sizeof(overrides) / sizeof(struct override))
#else
#define OVERRIDE_LIMIT commandline_current
#endif

/*
 * Function: issue_to_cmd
 *
 * Purpose: convert jump instruction in issue array to NCR53c7x0_cmd
 *	structure pointer.  
 *
 * Inputs; issue - pointer to start of NOP or JUMP instruction
 *	in issue array.
 *
 * Returns: pointer to command on success; 0 if opcode is NOP.
 */

static inline struct NCR53c7x0_cmd *
issue_to_cmd (struct Scsi_Host *host, struct NCR53c7x0_hostdata *hostdata,
    u32 *issue)
{
    return (issue[0] != hostdata->NOP_insn) ? 
    /* 
     * If the IF TRUE bit is set, it's a JUMP instruction.  The
     * operand is a bus pointer to the dsa_begin routine for this DSA.  The
     * dsa field of the NCR53c7x0_cmd structure starts with the 
     * DSA code template.  By converting to a virtual address,
     * subtracting the code template size, and offset of the 
     * dsa field, we end up with a pointer to the start of the 
     * structure (alternatively, we could use the 
     * dsa_cmnd field, an anachronism from when we weren't
     * sure what the relationship between the NCR structures
     * and host structures were going to be.
     */
	(struct NCR53c7x0_cmd *) ((char *) bus_to_virt (le32_to_cpu(issue[1])) - 
	    (hostdata->E_dsa_code_begin - hostdata->E_dsa_code_template) -
	    offsetof(struct NCR53c7x0_cmd, dsa)) 
    /* If the IF TRUE bit is not set, it's a NOP */
	: NULL;
}


/*
 * Function : static internal_setup(int board, int chip, char *str, int *ints)
 *
 * Purpose : LILO command line initialization of the overrides array,
 * 
 * Inputs : board - currently, unsupported.  chip - 700, 70066, 710, 720
 * 	810, 815, 820, 825, although currently only the NCR53c810 is 
 *	supported.
 * 
 */

static void 
internal_setup(int board, int chip, char *str, int *ints) {
    unsigned char pci;		/* Specifies a PCI override, with bus, device,
				   function */

    pci = (str && !strcmp (str, "pci")) ? 1 : 0;
    
/*
 * Override syntaxes are as follows : 
 * ncr53c700,ncr53c700-66,ncr53c710,ncr53c720=mem,io,irq,dma
 * ncr53c810,ncr53c820,ncr53c825=mem,io,irq or pci,bus,device,function
 */

    if (commandline_current < OVERRIDE_LIMIT) {
	overrides[commandline_current].pci = pci ? 1 : 0;
	if (!pci) {
	    overrides[commandline_current].data.normal.base = ints[1];
	    overrides[commandline_current].data.normal.io_port = ints[2];
	    overrides[commandline_current].data.normal.irq = ints[3];
    	    overrides[commandline_current].data.normal.dma = (ints[0] >= 4) ?
    	    	ints[4] : DMA_NONE;
	    /* FIXME: options is now a long long */
    	    overrides[commandline_current].options = (ints[0] >= 5) ?
    	    	ints[5] : 0;
	} else {
	    overrides[commandline_current].data.pci.bus = ints[1];
	    overrides[commandline_current].data.pci.device = ints[2];
	    overrides[commandline_current].data.pci.function = ints[3];
	    /* FIXME: options is now a long long */
    	    overrides[commandline_current].options = (ints[0] >= 4) ?
    	    	ints[4] : 0;
	}
	overrides[commandline_current].board = board;
	overrides[commandline_current].chip = chip;
	++commandline_current;
    	++no_overrides;
    } else {
	printk ("53c7,7x0.c:internal_setup() : too many overrides\n");
    }
}

/*
 * XXX - we might want to implement a single override function
 *       with a chip type field, revamp the command line configuration,
 * 	 etc.
 */

#define setup_wrapper(x) 				\
void ncr53c##x##_setup (char *str, int *ints) {		\
    internal_setup (BOARD_GENERIC, x, str, ints);	\
}

setup_wrapper(700)
setup_wrapper(70066)
setup_wrapper(710)
setup_wrapper(720)
setup_wrapper(810)
setup_wrapper(815)
setup_wrapper(820)
setup_wrapper(825)

/* 
 * FIXME: we should junk these, in favor of synchronous_want and 
 * wide_want in the NCR53c7x0_hostdata structure.
 */

/* Template for "preferred" synchronous transfer parameters. */

static const unsigned char sdtr_message[] = {
#ifdef CONFIG_SCSI_NCR53C7xx_FAST
    EXTENDED_MESSAGE, 3 /* length */, EXTENDED_SDTR, 25 /* *4ns */, 8 /* off */ 
#else
    EXTENDED_MESSAGE, 3 /* length */, EXTENDED_SDTR, 50 /* *4ns */, 8 /* off */ 
#endif
};

/* Template to request asynchronous transfers */

static const unsigned char async_message[] = {
    EXTENDED_MESSAGE, 3 /* length */, EXTENDED_SDTR, 0, 0 /* asynchronous */
};

/* Template for "preferred" WIDE transfer parameters */

static const unsigned char wdtr_message[] = {
    EXTENDED_MESSAGE, 2 /* length */, EXTENDED_WDTR, 1 /* 2^1 bytes */
};

/*
 * Function : struct Scsi_Host *find_host (int host)
 * 
 * Purpose : KGDB support function which translates a host number 
 * 	to a host structure. 
 *
 * Inputs : host - number of SCSI host
 *
 * Returns : NULL on failure, pointer to host structure on success.
 */

#if 0
static struct Scsi_Host *
find_host (int host) {
    struct Scsi_Host *h;
    for (h = first_host; h && h->host_no != host; h = h->next);
    if (!h) {
	printk (KERN_ALERT "scsi%d not found\n", host);
	return NULL;
    } else if (h->hostt != the_template) {
	printk (KERN_ALERT "scsi%d is not a NCR board\n", host);
	return NULL;
    }
    return h;
}

/*
 * Function : request_synchronous (int host, int target)
 * 
 * Purpose : KGDB interface which will allow us to negotiate for 
 * 	synchronous transfers.  This ill be replaced with a more 
 * 	integrated function; perhaps a new entry in the scsi_host 
 *	structure, accessible via an ioctl() or perhaps /proc/scsi.
 *
 * Inputs : host - number of SCSI host; target - number of target.
 *
 * Returns : 0 when negotiation has been setup for next SCSI command,
 *	-1 on failure.
 */

static int
request_synchronous (int host, int target) {
    struct Scsi_Host *h;
    struct NCR53c7x0_hostdata *hostdata;
    unsigned long flags;
    if (target < 0) {
	printk (KERN_ALERT "target %d is bogus\n", target);
	return -1;
    }
    if (!(h = find_host (host)))
	return -1;
    else if (h->this_id == target) {
	printk (KERN_ALERT "target %d is host ID\n", target);
	return -1;
    } 
    else if (target > h->max_id) {
	printk (KERN_ALERT "target %d exceeds maximum of %d\n", target,
	    h->max_id);
	return -1;
    }
    hostdata = (struct NCR53c7x0_hostdata *)h->hostdata;

    save_flags(flags);
    cli();
    if (hostdata->initiate_sdtr & (1 << target)) {
	restore_flags(flags);
	printk (KERN_ALERT "target %d already doing SDTR\n", target);
	return -1;
    } 
    hostdata->initiate_sdtr |= (1 << target);
    restore_flags(flags);
    return 0;
}

/*
 * Function : request_disconnect (int host, int on_or_off)
 * 
 * Purpose : KGDB support function, tells us to allow or disallow 
 *	disconnections.
 *
 * Inputs : host - number of SCSI host; on_or_off - non-zero to allow,
 *	zero to disallow.
 *
 * Returns : 0 on success, *	-1 on failure.
 */

static int 
request_disconnect (int host, int on_or_off) {
    struct Scsi_Host *h;
    struct NCR53c7x0_hostdata *hostdata;
    if (!(h = find_host (host)))
	return -1;
    hostdata = (struct NCR53c7x0_hostdata *) h->hostdata;
    if (on_or_off) 
	hostdata->options |= OPTION_DISCONNECT;
    else
	hostdata->options &= ~OPTION_DISCONNECT;
    return 0;
}
#endif

/*
 * Function : static void NCR53c7x0_driver_init (struct Scsi_Host *host)
 *
 * Purpose : Initialize internal structures, as required on startup, or 
 *	after a SCSI bus reset.
 * 
 * Inputs : host - pointer to this host adapter's structure
 */

static void 
NCR53c7x0_driver_init (struct Scsi_Host *host) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    int i, j;
    u32 *curr;
    for (i = 0; i < 16; ++i) {
	hostdata->request_sense[i] = 0;
    	for (j = 0; j < 8; ++j) 
	    hostdata->busy[i][j] = 0;
	set_synchronous (host, i, /* sxfer */ 0, hostdata->saved_scntl3, 0);
    }
    hostdata->issue_queue = NULL;
    hostdata->running_list = hostdata->finished_queue = 
	hostdata->curr = NULL;
    for (i = 0, curr = (u32 *) hostdata->schedule; 
	i < host->can_queue; ++i, curr += 2) {
	curr[0] = hostdata->NOP_insn;
	curr[1] = le32_to_cpu(0xdeadbeef);
    }
    curr[0] = le32_to_cpu(((DCMD_TYPE_TCI|DCMD_TCI_OP_JUMP) << 24) | DBC_TCI_TRUE);
    curr[1] = (u32) le32_to_cpu(virt_to_bus (hostdata->script) +
	hostdata->E_wait_reselect);
    hostdata->reconnect_dsa_head = 0;
    hostdata->addr_reconnect_dsa_head = (u32) 
	le32_to_cpu(virt_to_bus((void *) &(hostdata->reconnect_dsa_head)));
    hostdata->expecting_iid = 0;
    hostdata->expecting_sto = 0;
    if (hostdata->options & OPTION_ALWAYS_SYNCHRONOUS) 
	hostdata->initiate_sdtr = le32_to_cpu(0xffff); 
    else
    	hostdata->initiate_sdtr = 0;
    hostdata->talked_to = 0;
    hostdata->idle = 1;
}

/* 
 * Function : static int ccf_to_clock (int ccf)
 *
 * Purpose :  Return the largest SCSI clock allowable for a given
 *	clock conversion factor, allowing us to do synchronous periods
 *	when we don't know what the SCSI clock is by taking at least 
 *	as long as the device says we can.
 *
 * Inputs : ccf
 *
 * Returns : clock on success, -1 on failure.
 */

static int 
ccf_to_clock (int ccf) {
    switch (ccf) {
    case 1: return 25000000; 	/* Divide by 1.0 */
    case 2: return 37500000;	/* Divide by 1.5 */
    case 3: return 50000000;  	/* Divide by 2.0 */
    case 0: 			/* Divide by 3.0 */
    case 4: return 66000000; 
    default: return -1;
    }
}

/* 
 * Function : static int clock_to_ccf (int clock)
 *
 * Purpose :  Return the clock conversion factor for a given SCSI clock.
 *
 * Inputs : clock - SCSI clock expressed in Hz.
 *
 * Returns : ccf on success, -1 on failure.
 */

static int 
clock_to_ccf (int clock) {
    if (clock < 16666666)
	return -1;
    if (clock < 25000000)
	return 1; 	/* Divide by 1.0 */
    else if (clock < 37500000)
	return 2; 	/* Divide by 1.5 */
    else if (clock < 50000000)
	return 3;	/* Divide by 2.0 */
    else if (clock < 66000000)
	return 4;	/* Divide by 3.0 */
    else 
	return -1;
}
    
/* 
 * Function : static int NCR53c7x0_init (struct Scsi_Host *host)
 *
 * Purpose :  initialize the internal structures for a given SCSI host
 *
 * Inputs : host - pointer to this host adapter's structure
 *
 * Preconditions : when this function is called, the chip_type 
 * 	field of the hostdata structure MUST have been set.
 *
 * Returns : 0 on success, -1 on failure.
 */

static int 
NCR53c7x0_init (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    int i, ccf, expected_ccf;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    struct Scsi_Host *search;
    /* 
     * There are some things which we need to know about in order to provide
     * a semblance of support.  Print 'em if they aren't what we expect, 
     * otherwise don't add to the noise.
     * 
     * -1 means we don't know what to expect.
     */
    int expected_id = -1;
    int expected_clock = -1;
    int uninitialized = 0;
    /* 
     * FIXME : this is only on Intel boxes.  On other platforms, this
     * will differ.
     */
    int expected_mapping = OPTION_IO_MAPPED;

    NCR53c7x0_local_setup(host);

    switch (hostdata->chip) {
    case 820:
    case 825:
#ifdef notyet
	host->max_id = 15;
#endif
	/* Fall through */
    case 810:
    case 815:
    	hostdata->dstat_sir_intr = NCR53c8x0_dstat_sir_intr;
    	hostdata->init_save_regs = NULL;
    	hostdata->dsa_fixup = NCR53c8xx_dsa_fixup;
    	hostdata->init_fixup = NCR53c8x0_init_fixup;
    	hostdata->soft_reset = NCR53c8x0_soft_reset;
	hostdata->run_tests = NCR53c8xx_run_tests;
/* Is the SCSI clock ever anything else on these chips? */
	expected_clock = hostdata->scsi_clock = 40000000;
	expected_id = 7;
    	break;
    default:
	printk ("scsi%d : chip type of %d is not supported yet, detaching.\n",
	    host->host_no, hostdata->chip);
	scsi_unregister (host);
	return -1;
    }

    /* Assign constants accessed by NCR */
    hostdata->NCR53c7xx_zero = 0;			
    hostdata->NCR53c7xx_msg_reject = le32_to_cpu(MESSAGE_REJECT);
    hostdata->NCR53c7xx_msg_abort = le32_to_cpu(ABORT);
    hostdata->NCR53c7xx_msg_nop = le32_to_cpu(NOP);
    hostdata->NOP_insn = le32_to_cpu((DCMD_TYPE_TCI|DCMD_TCI_OP_JUMP) << 24);

    if (expected_mapping == -1 || 
	(hostdata->options & (OPTION_MEMORY_MAPPED)) != 
	(expected_mapping & OPTION_MEMORY_MAPPED))
	printk ("scsi%d : using %s mapped access\n", host->host_no, 
	    (hostdata->options & OPTION_MEMORY_MAPPED) ? "memory" : 
	    "io");

    hostdata->dmode = (hostdata->chip == 700 || hostdata->chip == 70066) ? 
	DMODE_REG_00 : DMODE_REG_10;
    hostdata->istat = ((hostdata->chip / 100) == 8) ? 
    	ISTAT_REG_800 : ISTAT_REG_700;

/* Only the ISTAT register is readable when the NCR is running, so make 
   sure it's halted. */
    ncr_halt(host);

/* 
 * XXX - the NCR53c700 uses bitfielded registers for SCID, SDID, etc,
 *	as does the 710 with one bit per SCSI ID.  Conversely, the NCR
 * 	uses a normal, 3 bit binary representation of these values.
 *
 * Get the rest of the NCR documentation, and FIND OUT where the change
 * was.
 */
#if 0
    tmp = hostdata->this_id_mask = NCR53c7x0_read8(SCID_REG);
    for (host->this_id = 0; tmp != 1; tmp >>=1, ++host->this_id);
#else
    host->this_id = NCR53c7x0_read8(SCID_REG) & 15;
    if (host->this_id == 0)
	host->this_id = 7;	/* sanitize hostid---0 doesn't make sense */
    hostdata->this_id_mask = 1 << host->this_id;
#endif

/*
 * Note : we should never encounter a board setup for ID0.  So,
 * 	if we see ID0, assume that it was uninitialized and set it
 * 	to the industry standard 7.
 */
    if (!host->this_id) {
	printk("scsi%d : initiator ID was %d, changing to 7\n",
	    host->host_no, host->this_id);
	host->this_id = 7;
	hostdata->this_id_mask = 1 << 7;
	uninitialized = 1;
    };

    if (expected_id == -1 || host->this_id != expected_id)
    	printk("scsi%d : using initiator ID %d\n", host->host_no,
    	    host->this_id);

    /*
     * Save important registers to allow a soft reset.
     */

    if ((hostdata->chip / 100) == 8) {
    /* 
     * CTEST4 controls burst mode disable.
     */
	hostdata->saved_ctest4 = NCR53c7x0_read8(CTEST4_REG_800) & 
    	    CTEST4_800_SAVE;
    } else {
    /*
     * CTEST7 controls cache snooping, burst mode, and support for 
     * external differential drivers.
     */
	hostdata->saved_ctest7 = NCR53c7x0_read8(CTEST7_REG) & CTEST7_SAVE;
    }

    /*
     * On NCR53c700 series chips, DCNTL controls the SCSI clock divisor,
     * on 800 series chips, it allows for a totem-pole IRQ driver.
     */

    hostdata->saved_dcntl = NCR53c7x0_read8(DCNTL_REG);

    /* 
     * DCNTL_800_IRQM controls weather we are using an open drain
     * driver (reset) or totem pole driver (set).  In all cases, 
     * it's level active.  I suppose this is an issue when we're trying to 
     * wire-or the same PCI INTx line?
     */
    if ((hostdata->chip / 100) == 8)
	hostdata->saved_dcntl &= ~DCNTL_800_IRQM;

    /*
     * DMODE controls DMA burst length, and on 700 series chips,
     * 286 mode and bus width  
     */
    hostdata->saved_dmode = NCR53c7x0_read8(hostdata->dmode);

    /* 
     * Now that burst length and enabled/disabled status is known, 
     * clue the user in on it.  
     */
   
    if ((hostdata->chip / 100) == 8) {
	if (hostdata->saved_ctest4 & CTEST4_800_BDIS) {
	    printk ("scsi%d : burst mode disabled\n", host->host_no);
	} else {
	    switch (hostdata->saved_dmode & DMODE_BL_MASK) {
	    case DMODE_BL_2: i = 2; break;
	    case DMODE_BL_4: i = 4; break;
	    case DMODE_BL_8: i = 8; break;
	    case DMODE_BL_16: i = 16; break;
            default: i = 0;
	    }
	    printk ("scsi%d : burst length %d\n", host->host_no, i);
	}
    }

    /*
     * On NCR53c810 and NCR53c820 chips, SCNTL3 contails the synchronous
     * and normal clock conversion factors.
     */
    if (hostdata->chip / 100 == 8)  {
	expected_ccf = clock_to_ccf (expected_clock);
	hostdata->saved_scntl3 = NCR53c7x0_read8(SCNTL3_REG_800);
	ccf = hostdata->saved_scntl3 & SCNTL3_800_CCF_MASK;
	if (expected_ccf != -1 && ccf != expected_ccf && !ccf) {
	    hostdata->saved_scntl3 = (hostdata->saved_scntl3 &
		~SCNTL3_800_CCF_MASK) | expected_ccf;
	    if (!uninitialized) {
		printk ("scsi%d : reset ccf to %d from %d\n", 
		    host->host_no, expected_ccf, ccf);
		uninitialized = 1;
	    }
	}
    } else
    	ccf = 0;

    /*
     * If we don't have a SCSI clock programmed, pick one on the upper
     * bound of that allowed by NCR so that our transfers err on the 
     * slow side, since transfer period must be >= the agreed 
     * upon period.
     */

    if ((!hostdata->scsi_clock) && (hostdata->scsi_clock = ccf_to_clock (ccf)) 
	== -1) {
	printk ("scsi%d : clock conversion factor %d unknown.\n"
		"         synchronous transfers disabled\n",
		host->host_no, ccf);
	hostdata->options &= ~OPTION_SYNCHRONOUS;
	hostdata->scsi_clock = 0; 
    }

    if (expected_clock == -1 || hostdata->scsi_clock != expected_clock)
    	printk ("scsi%d : using %dMHz SCSI clock\n", host->host_no, 
	    hostdata->scsi_clock / 1000000);

    for (i = 0; i < 16; ++i) 
	hostdata->cmd_allocated[i] = 0;

    if (hostdata->init_save_regs)
    	hostdata->init_save_regs (host);
    if (hostdata->init_fixup)
    	hostdata->init_fixup (host);

    if (!the_template) {
	the_template = host->hostt;
	first_host = host;
    }

    /* 
     * Linux SCSI drivers have always been plagued with initialization 
     * problems - some didn't work with the BIOS disabled since they expected
     * initialization from it, some didn't work when the networking code
     * was enabled and registers got scrambled, etc.
     *
     * To avoid problems like this, in the future, we will do a soft 
     * reset on the SCSI chip, taking it back to a sane state.
     */

    hostdata->soft_reset (host);

#if 1
    hostdata->debug_count_limit = -1;
#else
    hostdata->debug_count_limit = 1;
#endif
    hostdata->intrs = -1;
    hostdata->resets = -1;
    memcpy ((void *) hostdata->synchronous_want, (void *) sdtr_message, 
	sizeof (hostdata->synchronous_want));

    NCR53c7x0_driver_init (host);

    /*
     * Set up an interrupt handler if we aren't already sharing an IRQ
     * with another board.
     */

    for (search = first_host; search && !(search->hostt == the_template &&
	search->irq == host->irq && search != host); search=search->next);

    if (!search) {
#ifdef __powerpc__
	if (request_irq(host->irq, do_NCR53c7x0_intr, SA_SHIRQ, "53c7,8xx", NULL)) 
#else
	if (request_irq(host->irq, do_NCR53c7x0_intr, SA_INTERRUPT, "53c7,8xx", NULL))
#endif
	  {
	  
	  
	    printk("scsi%d : IRQ%d not free, detaching\n"
	           "         You have either a configuration problem, or a\n"
                   "         broken BIOS.  You may wish to manually assign\n"
	           "         an interrupt to the NCR board rather than using\n"
                   "         an automatic setting.\n", 
		host->host_no, host->irq);
	    scsi_unregister (host);
	    return -1;
	} 
    } else {
	printk("scsi%d : using interrupt handler previously installed for scsi%d\n",
	    host->host_no, search->host_no);
    }


    if ((hostdata->run_tests && hostdata->run_tests(host) == -1) ||
        (hostdata->options & OPTION_DEBUG_TESTS_ONLY)) {
    	/* XXX Should disable interrupts, etc. here */
	scsi_unregister (host);
    	return -1;
    } else {
	if (host->io_port)  {
	    host->n_io_port = 128;
	    request_region (host->io_port, host->n_io_port, "ncr53c7,8xx");
	}
    }
    
    if (NCR53c7x0_read8 (SBCL_REG) & SBCL_BSY) {
	printk ("scsi%d : bus wedge, doing SCSI reset\n", host->host_no);
	hard_reset (host);
    }
    return 0;
}

/* 
 * Function : static int normal_init(Scsi_Host_Template *tpnt, int board, 
 *	int chip, u32 base, int io_port, int irq, int dma, int pcivalid,
 *	unsigned char pci_bus, unsigned char pci_device_fn,
 *      struct pci_dev *pci_dev, long long options);
 *
 * Purpose : initializes a NCR53c7,8x0 based on base addresses,
 *	IRQ, and DMA channel.	
 *	
 *	Useful where a new NCR chip is backwards compatible with
 *	a supported chip, but the DEVICE ID has changed so it 
 *	doesn't show up when the autoprobe does a pcibios_find_device.
 *
 * Inputs : tpnt - Template for this SCSI adapter, board - board level
 *	product, chip - 810, 820, or 825, bus - PCI bus, device_fn -
 *	device and function encoding as used by PCI BIOS calls.
 * 
 * Returns : 0 on success, -1 on failure.
 *
 */

static int  __init 
normal_init (Scsi_Host_Template *tpnt, int board, int chip, 
    u32 base, int io_port, int irq, int dma, int pci_valid, 
    unsigned char pci_bus, unsigned char pci_device_fn,
    struct pci_dev *pci_dev, long long options)
{
    struct Scsi_Host *instance;
    struct NCR53c7x0_hostdata *hostdata;
    char chip_str[80];
    int script_len = 0, dsa_len = 0, size = 0, max_cmd_size = 0, 
	schedule_size = 0, ok = 0;
    void *tmp;

    options |= perm_options;

    switch (chip) {
    case 825:
    case 820:
    case 815:
    case 810:
	schedule_size = (tpnt->can_queue + 1) * 8 /* JUMP instruction size */;
	script_len = NCR53c8xx_script_len;
    	dsa_len = NCR53c8xx_dsa_len;
    	options |= OPTION_INTFLY;
    	sprintf (chip_str, "NCR53c%d", chip);
    	break;
    default:
    	printk("scsi-ncr53c7,8xx : unsupported SCSI chip %d\n", chip);
    	return -1;
    }

    printk("scsi-ncr53c7,8xx : %s at memory 0x%x, io 0x%x, irq %d",
    	chip_str, (unsigned) base, io_port, irq);
    if (dma == DMA_NONE)
    	printk("\n");
    else 
    	printk(", dma %d\n", dma);

    if ((chip / 100 == 8) && !pci_valid) 
	printk ("scsi-ncr53c7,8xx : for better reliability and performance, please use the\n" 
		"        PCI override instead.\n"
		"	 Syntax : ncr53c8{10,15,20,25}=pci,<bus>,<device>,<function>\n"
		"                 <bus> and <device> are usually 0.\n");

    if (options & OPTION_DEBUG_PROBE_ONLY) {
    	printk ("scsi-ncr53c7,8xx : probe only enabled, aborting initialization\n");
    	return -1;
    }

    max_cmd_size = sizeof(struct NCR53c7x0_cmd) + dsa_len +
    	/* Size of dynamic part of command structure : */
	2 * /* Worst case : we don't know if we need DATA IN or DATA out */
		( 2 * /* Current instructions per scatter/gather segment */ 
        	  tpnt->sg_tablesize + 
                  3 /* Current startup / termination required per phase */
		) *
	8 /* Each instruction is eight bytes */;

    /* Allocate fixed part of hostdata, dynamic part to hold appropriate
       SCSI SCRIPT(tm) plus a single, maximum-sized NCR53c7x0_cmd structure.

       We need a NCR53c7x0_cmd structure for scan_scsis() when we are 
       not loaded as a module, and when we're loaded as a module, we 
       can't use a non-dynamically allocated structure because modules
       are vmalloc()'d, which can allow structures to cross page 
       boundaries and breaks our physical/virtual address assumptions
       for DMA.

       So, we stick it past the end of our hostdata structure.

       ASSUMPTION : 
       	 Regardless of how many simultaneous SCSI commands we allow,
	 the probe code only executes a _single_ instruction at a time,
	 so we only need one here, and don't need to allocate NCR53c7x0_cmd
	 structures for each target until we are no longer in scan_scsis
	 and kmalloc() has become functional (memory_init() happens 
	 after all device driver initialization).
    */

    size = sizeof(struct NCR53c7x0_hostdata) + script_len + 
    /* Note that alignment will be guaranteed, since we put the command
       allocated at probe time after the fixed-up SCSI script, which 
       consists of 32 bit words, aligned on a 32 bit boundary.  But
       on a 64bit machine we need 8 byte alignment for hostdata->free, so
       we add in another 4 bytes to take care of potential misalignment
       */
	(sizeof(void *) - sizeof(u32)) + max_cmd_size + schedule_size;

    instance = scsi_register (tpnt, size);
    if (!instance)
	return -1;

    /* FIXME : if we ever support an ISA NCR53c7xx based board, we
       need to check if the chip is running in a 16 bit mode, and if so 
       unregister it if it is past the 16M (0x1000000) mark */
   	
    hostdata = (struct NCR53c7x0_hostdata *) 
    	instance->hostdata;
    hostdata->size = size;
    hostdata->script_count = script_len / sizeof(u32);
    hostdata = (struct NCR53c7x0_hostdata *) instance->hostdata;
    hostdata->board = board;
    hostdata->chip = chip;
    if ((hostdata->pci_valid = pci_valid)) {
	hostdata->pci_bus = pci_bus;
	hostdata->pci_device_fn = pci_device_fn;
    }

    /*
     * Being memory mapped is more desirable, since 
     *
     * - Memory accesses may be faster.
     *
     * - The destination and source address spaces are the same for 
     *	 all instructions, meaning we don't have to twiddle dmode or 
     *	 any other registers.
     *
     * So, we try for memory mapped, and if we don't get it,
     * we go for port mapped, and that failing we tell the user
     * it can't work.
     */

    if (base) {
	instance->base = (unsigned long) base;
	/* Check for forced I/O mapping */
    	if (!(options & OPTION_IO_MAPPED)) {
	    options |= OPTION_MEMORY_MAPPED;
	    ok = 1;
	}
    } else {
	options &= ~OPTION_MEMORY_MAPPED;
    }

    if (io_port) {
	instance->io_port = io_port;
	options |= OPTION_IO_MAPPED;
	ok = 1;
    } else {
	options &= ~OPTION_IO_MAPPED;
    }

    if (!ok) {
	printk ("scsi%d : not initializing, no I/O or memory mapping known \n",
	    instance->host_no);
	scsi_unregister (instance);
	return -1;
    }
    instance->irq = irq;
    instance->dma_channel = dma;
    scsi_set_pci_device(instance, pci_dev);

    hostdata->options = options;
    hostdata->dsa_len = dsa_len;
    hostdata->max_cmd_size = max_cmd_size;
    hostdata->num_cmds = 1;
    /* Initialize single command */
    tmp = (hostdata->script + hostdata->script_count);
    hostdata->free = ROUNDUP(tmp, void *);
    hostdata->free->real = tmp;
    hostdata->free->size = max_cmd_size;
    hostdata->free->free = NULL;
    hostdata->free->next = NULL;
    hostdata->extra_allocate = 0;

    /* Allocate command start code space */
    hostdata->schedule = (chip == 700 || chip == 70066) ?
	NULL : (u32 *) ((char *)hostdata->free + max_cmd_size);

/* 
 * For diagnostic purposes, we don't really care how fast things blaze.
 * For profiling, we want to access the 800ns resolution system clock,
 * using a 'C' call on the host processor.
 *
 * Therefore, there's no need for the NCR chip to directly manipulate
 * this data, and we should put it wherever is most convenient for 
 * Linux.
 */
    if (track_events) 
	hostdata->events = (struct NCR53c7x0_event *) (track_events ? 
	    vmalloc (sizeof (struct NCR53c7x0_event) * track_events) : NULL);
    else
	hostdata->events = NULL;

    if (hostdata->events) {
	memset ((void *) hostdata->events, 0, sizeof(struct NCR53c7x0_event) *
	    track_events);	
	hostdata->event_size = track_events;
	hostdata->event_index = 0;
    } else 
	hostdata->event_size = 0;

    return NCR53c7x0_init(instance);
}


/* 
 * Function : static int ncr_pci_init(Scsi_Host_Template *tpnt, int board, 
 *	int chip, int bus, int device_fn, long long options)
 *
 * Purpose : initializes a NCR53c800 family based on the PCI
 *	bus, device, and function location of it.  Allows 
 * 	reprogramming of latency timer and determining addresses
 *	and whether bus mastering, etc. are OK.
 *	
 *	Useful where a new NCR chip is backwards compatible with
 *	a supported chip, but the DEVICE ID has changed so it 
 *	doesn't show up when the autoprobe does a pcibios_find_device.
 *
 * Inputs : tpnt - Template for this SCSI adapter, board - board level
 *	product, chip - 810, 820, or 825, bus - PCI bus, device_fn -
 *	device and function encoding as used by PCI BIOS calls.
 * 
 * Returns : 0 on success, -1 on failure.
 *
 */

static int  __init 
ncr_pci_init (Scsi_Host_Template *tpnt, int board, int chip, 
    unsigned char bus, unsigned char device_fn, long long options){
    unsigned short command;
    unsigned int base, io_port; 
    unsigned char revision;
    int error, expected_chip;
    int expected_id = -1, max_revision = -1, min_revision = -1;
    int i, irq;
    struct pci_dev *pdev = pci_find_slot(bus, device_fn);

    printk("scsi-ncr53c7,8xx : at PCI bus %d, device %d, function %d\n",
	bus, (int) (device_fn & 0xf8) >> 3, 
    	(int) device_fn & 7);

    if (!pdev) {
	printk("scsi-ncr53c7,8xx : not initializing -- PCI device not found,\n"
	       "        try using memory, port, irq override instead.\n");
	return -1;
    }

    if ((error = pci_read_config_word (pdev, PCI_COMMAND, &command)) ||
	(error = pci_read_config_byte (pdev, PCI_CLASS_REVISION, &revision))) {
	printk ("scsi-ncr53c7,8xx : error %d not initializing due to error reading configuration space\n"
		"	 perhaps you specified an incorrect PCI bus, device, or function.\n", error);
	return -1;
    }
    if (pci_enable_device(pdev))
	return -1;
    io_port = pci_resource_start(pdev, 0);
    base = pci_resource_start(pdev, 1);
    irq = pdev->irq;

    /* If any one ever clones the NCR chips, this will have to change */

    if (pdev->vendor != PCI_VENDOR_ID_NCR) {
	printk ("scsi-ncr53c7,8xx : not initializing, 0x%04x is not NCR vendor ID\n",
	    (int) pdev->vendor);
	return -1;
    }

#ifdef __powerpc__
    if ( ! (command & PCI_COMMAND_MASTER)) {
      printk("SCSI: PCI Master Bit has not been set. Setting...\n");
      command |= PCI_COMMAND_MASTER|PCI_COMMAND_IO;
      pci_write_config_word(pdev, PCI_COMMAND, command);

      if (io_port >= 0x10000000 && is_prep ) {
	      /* Mapping on PowerPC can't handle this! */
	      unsigned long new_io_port;
	      new_io_port = (io_port & 0x00FFFFFF) | 0x01000000;
	      printk("SCSI: I/O moved from %08X to %08x\n", io_port, new_io_port);
	      io_port = new_io_port;
	      pci_write_config_dword(pdev, PCI_BASE_ADDRESS_0, io_port);
	      pdev->base_address[0] = io_port;
      }
    }
#endif

    /* 
     * Bit 0 is the address space indicator and must be one for I/O
     * space mappings, bit 1 is reserved, discard them after checking
     * that they have the correct value of 1.
     */

    if (command & PCI_COMMAND_IO) { 
	if (!(pdev->resource[0].flags & IORESOURCE_IO)) {
	    printk ("scsi-ncr53c7,8xx : disabling I/O mapping since base "
		    "address 0\n        contains a non-IO mapping\n");
	    io_port = 0;
	}
    } else {
    	io_port = 0;
    }

    if (command & PCI_COMMAND_MEMORY) {
	if (!(pdev->resource[1].flags & IORESOURCE_MEM)) {
	    printk("scsi-ncr53c7,8xx : disabling memory mapping since base "
		   "address 1\n        contains a non-memory mapping\n");
	    base = 0;
	}
    } else {
	base = 0;
    }
	
    if (!io_port && !base) {
	printk ("scsi-ncr53c7,8xx : not initializing, both I/O and memory mappings disabled\n");
	return -1;
    }
	
    if (!(command & PCI_COMMAND_MASTER)) {
	printk ("scsi-ncr53c7,8xx : not initializing, BUS MASTERING was disabled\n");
	return -1;
    }

    for (i = 0; i < NPCI_CHIP_IDS; ++i) {
	if (pdev->device == pci_chip_ids[i].pci_device_id) {
	    max_revision = pci_chip_ids[i].max_revision;
	    min_revision = pci_chip_ids[i].min_revision;
	    expected_chip = pci_chip_ids[i].chip;
	}
	if (chip == pci_chip_ids[i].chip)
	    expected_id = pci_chip_ids[i].pci_device_id;
    }

    if (chip && pdev->device != expected_id) 
	printk ("scsi-ncr53c7,8xx : warning : device id of 0x%04x doesn't\n"
                "                   match expected 0x%04x\n",
	    (unsigned int) pdev->device, (unsigned int) expected_id );
    
    if (max_revision != -1 && revision > max_revision) 
	printk ("scsi-ncr53c7,8xx : warning : revision of %d is greater than %d.\n",
	    (int) revision, max_revision);
    else if (min_revision != -1 && revision < min_revision)
	printk ("scsi-ncr53c7,8xx : warning : revision of %d is less than %d.\n",
	    (int) revision, min_revision);

    if (io_port && check_region (io_port, 128)) {
	printk ("scsi-ncr53c7,8xx : IO region 0x%x to 0x%x is in use\n",
	    (unsigned) io_port, (unsigned) io_port + 127);
	return -1;
    }

    return normal_init (tpnt, board, chip, (int) base, io_port, 
	(int) irq, DMA_NONE, 1, bus, device_fn, pdev, options);
}


/* 
 * Function : int NCR53c7xx_detect(Scsi_Host_Template *tpnt)
 *
 * Purpose : detects and initializes NCR53c7,8x0 SCSI chips
 *	that were autoprobed, overridden on the LILO command line, 
 *	or specified at compile time.
 *
 * Inputs : tpnt - template for this SCSI adapter
 * 
 * Returns : number of host adapters detected
 *
 */

int __init 
NCR53c7xx_detect(Scsi_Host_Template *tpnt){
    int i;
    int current_override;
    int count;			/* Number of boards detected */
    unsigned char pci_bus, pci_device_fn;
    static short pci_index=0;	/* Device index to PCI BIOS calls */

    tpnt->proc_name = "ncr53c7xx";

    for (current_override = count = 0; current_override < OVERRIDE_LIMIT; 
	 ++current_override) {
	 if (overrides[current_override].pci ? 
	    !ncr_pci_init (tpnt, overrides[current_override].board,
		overrides[current_override].chip,
		(unsigned char) overrides[current_override].data.pci.bus,
		(((overrides[current_override].data.pci.device
		<< 3) & 0xf8)|(overrides[current_override].data.pci.function & 
		7)), overrides[current_override].options):
	    !normal_init (tpnt, overrides[current_override].board, 
		overrides[current_override].chip, 
		overrides[current_override].data.normal.base, 
		overrides[current_override].data.normal.io_port,
		overrides[current_override].data.normal.irq,
		overrides[current_override].data.normal.dma,
		0 /* PCI data invalid */, 0 /* PCI bus place holder */,  
		0 /* PCI device_function place holder */,
                NULL /* PCI pci_dev place holder */,
    	    	overrides[current_override].options)) {
    	    ++count;
	} 
    }

    if (pci_present()) {
	for (i = 0; i < NPCI_CHIP_IDS; ++i) 
	    for (pci_index = 0;
		!pcibios_find_device (PCI_VENDOR_ID_NCR, 
		    pci_chip_ids[i].pci_device_id, pci_index, &pci_bus, 
		    &pci_device_fn); 
    		++pci_index)
		if (!ncr_pci_init (tpnt, BOARD_GENERIC, pci_chip_ids[i].chip, 
		    pci_bus, pci_device_fn, /* no options */ 0))
		    ++count;
    }
    return count;
}

/* NCR53c810 and NCR53c820 script handling code */

#include "53c8xx_d.h"
#ifdef A_int_debug_sync
#define DEBUG_SYNC_INTR A_int_debug_sync
#endif
static int NCR53c8xx_script_len = sizeof (SCRIPT);
static int NCR53c8xx_dsa_len = A_dsa_end + Ent_dsa_zero - Ent_dsa_code_template;

/* 
 * Function : static void NCR53c8x0_init_fixup (struct Scsi_Host *host)
 *
 * Purpose :  copy and fixup the SCSI SCRIPTS(tm) code for this device.
 *
 * Inputs : host - pointer to this host adapter's structure
 *
 */

static void 
NCR53c8x0_init_fixup (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    unsigned char tmp;
    int i, ncr_to_memory, memory_to_ncr;
    u32 base;
#ifdef __powerpc__
    unsigned long *script_ptr;
#endif    
    NCR53c7x0_local_setup(host);


    /* XXX - NOTE : this code MUST be made endian aware */
    /*  Copy code into buffer that was allocated at detection time.  */
    memcpy ((void *) hostdata->script, (void *) SCRIPT, 
	sizeof(SCRIPT));
    /* Fixup labels */
    for (i = 0; i < PATCHES; ++i) 
	hostdata->script[LABELPATCHES[i]] += 
    	    virt_to_bus(hostdata->script);
    /* Fixup addresses of constants that used to be EXTERNAL */

    patch_abs_32 (hostdata->script, 0, NCR53c7xx_msg_abort, 
    	virt_to_bus(&(hostdata->NCR53c7xx_msg_abort)));
    patch_abs_32 (hostdata->script, 0, NCR53c7xx_msg_reject, 
    	virt_to_bus(&(hostdata->NCR53c7xx_msg_reject)));
    patch_abs_32 (hostdata->script, 0, NCR53c7xx_zero, 
    	virt_to_bus(&(hostdata->NCR53c7xx_zero)));
    patch_abs_32 (hostdata->script, 0, NCR53c7xx_sink, 
    	virt_to_bus(&(hostdata->NCR53c7xx_sink)));
    patch_abs_32 (hostdata->script, 0, NOP_insn,
	virt_to_bus(&(hostdata->NOP_insn)));
    patch_abs_32 (hostdata->script, 0, schedule,
	virt_to_bus((void *) hostdata->schedule));

    /* Fixup references to external variables: */
    for (i = 0; i < EXTERNAL_PATCHES_LEN; ++i)
       hostdata->script[EXTERNAL_PATCHES[i].offset] +=
         virt_to_bus(EXTERNAL_PATCHES[i].address);

    /* 
     * Fixup absolutes set at boot-time.
     * 
     * All non-code absolute variables suffixed with "dsa_" and "int_"
     * are constants, and need no fixup provided the assembler has done 
     * it for us (I don't know what the "real" NCR assembler does in 
     * this case, my assembler does the right magic).
     */

    patch_abs_rwri_data (hostdata->script, 0, dsa_save_data_pointer, 
    	Ent_dsa_code_save_data_pointer - Ent_dsa_zero);
    patch_abs_rwri_data (hostdata->script, 0, dsa_restore_pointers,
    	Ent_dsa_code_restore_pointers - Ent_dsa_zero);
    patch_abs_rwri_data (hostdata->script, 0, dsa_check_reselect,
    	Ent_dsa_code_check_reselect - Ent_dsa_zero);

    /*
     * Just for the hell of it, preserve the settings of 
     * Burst Length and Enable Read Line bits from the DMODE 
     * register.  Make sure SCRIPTS start automagically.
     */

    tmp = NCR53c7x0_read8(DMODE_REG_10);
    tmp &= (DMODE_800_ERL | DMODE_BL_MASK);

    if (!(hostdata->options & OPTION_MEMORY_MAPPED)) {
    	base = (u32) host->io_port;
    	memory_to_ncr = tmp|DMODE_800_DIOM;
    	ncr_to_memory = tmp|DMODE_800_SIOM;
    } else {
    	base = virt_to_bus((void *)host->base);
	memory_to_ncr = ncr_to_memory = tmp;
    }

    patch_abs_32 (hostdata->script, 0, addr_scratch, base + SCRATCHA_REG_800);
    patch_abs_32 (hostdata->script, 0, addr_temp, base + TEMP_REG);

    /*
     * I needed some variables in the script to be accessible to 
     * both the NCR chip and the host processor. For these variables,
     * I made the arbitrary decision to store them directly in the 
     * hostdata structure rather than in the RELATIVE area of the 
     * SCRIPTS.
     */
    

    patch_abs_rwri_data (hostdata->script, 0, dmode_memory_to_memory, tmp);
    patch_abs_rwri_data (hostdata->script, 0, dmode_memory_to_ncr, memory_to_ncr);
    patch_abs_rwri_data (hostdata->script, 0, dmode_ncr_to_memory, ncr_to_memory);

    patch_abs_32 (hostdata->script, 0, msg_buf, 
	virt_to_bus((void *)&(hostdata->msg_buf)));
    patch_abs_32 (hostdata->script, 0, reconnect_dsa_head, 
    	virt_to_bus((void *)&(hostdata->reconnect_dsa_head)));
    patch_abs_32 (hostdata->script, 0, addr_reconnect_dsa_head, 
	virt_to_bus((void *)&(hostdata->addr_reconnect_dsa_head)));
    patch_abs_32 (hostdata->script, 0, reselected_identify, 
    	virt_to_bus((void *)&(hostdata->reselected_identify)));
/* reselected_tag is currently unused */
#if 0
    patch_abs_32 (hostdata->script, 0, reselected_tag, 
    	virt_to_bus((void *)&(hostdata->reselected_tag)));
#endif

    patch_abs_32 (hostdata->script, 0, test_dest, 
	virt_to_bus((void*)&hostdata->test_dest));
    patch_abs_32 (hostdata->script, 0, test_src, 
	virt_to_bus(&hostdata->test_source));

    patch_abs_rwri_data (hostdata->script, 0, dsa_check_reselect, 
	(unsigned char)(Ent_dsa_code_check_reselect - Ent_dsa_zero));

/* These are for event logging; the ncr_event enum contains the 
   actual interrupt numbers. */
#ifdef A_int_EVENT_SELECT
   patch_abs_32 (hostdata->script, 0, int_EVENT_SELECT, (u32) EVENT_SELECT);
#endif
#ifdef A_int_EVENT_DISCONNECT
   patch_abs_32 (hostdata->script, 0, int_EVENT_DISCONNECT, (u32) EVENT_DISCONNECT);
#endif
#ifdef A_int_EVENT_RESELECT
   patch_abs_32 (hostdata->script, 0, int_EVENT_RESELECT, (u32) EVENT_RESELECT);
#endif
#ifdef A_int_EVENT_COMPLETE
   patch_abs_32 (hostdata->script, 0, int_EVENT_COMPLETE, (u32) EVENT_COMPLETE);
#endif
#ifdef A_int_EVENT_IDLE
   patch_abs_32 (hostdata->script, 0, int_EVENT_IDLE, (u32) EVENT_IDLE);
#endif
#ifdef A_int_EVENT_SELECT_FAILED
   patch_abs_32 (hostdata->script, 0, int_EVENT_SELECT_FAILED, 
	(u32) EVENT_SELECT_FAILED);
#endif
#ifdef A_int_EVENT_BEFORE_SELECT
   patch_abs_32 (hostdata->script, 0, int_EVENT_BEFORE_SELECT,
	(u32) EVENT_BEFORE_SELECT);
#endif
#ifdef A_int_EVENT_RESELECT_FAILED
   patch_abs_32 (hostdata->script, 0, int_EVENT_RESELECT_FAILED, 
	(u32) EVENT_RESELECT_FAILED);
#endif

    /*
     * Make sure the NCR and Linux code agree on the location of 
     * certain fields.
     */

    hostdata->E_accept_message = Ent_accept_message;
    hostdata->E_command_complete = Ent_command_complete;		
    hostdata->E_cmdout_cmdout = Ent_cmdout_cmdout;
    hostdata->E_data_transfer = Ent_data_transfer;
    hostdata->E_debug_break = Ent_debug_break;	
    hostdata->E_dsa_code_template = Ent_dsa_code_template;
    hostdata->E_dsa_code_template_end = Ent_dsa_code_template_end;
    hostdata->E_end_data_transfer = Ent_end_data_transfer;
    hostdata->E_initiator_abort = Ent_initiator_abort;
    hostdata->E_msg_in = Ent_msg_in;
    hostdata->E_other_transfer = Ent_other_transfer;
    hostdata->E_other_in = Ent_other_in;
    hostdata->E_other_out = Ent_other_out;
    hostdata->E_reject_message = Ent_reject_message;
    hostdata->E_respond_message = Ent_respond_message;
    hostdata->E_select = Ent_select;
    hostdata->E_select_msgout = Ent_select_msgout;
    hostdata->E_target_abort = Ent_target_abort;
#ifdef Ent_test_0
    hostdata->E_test_0 = Ent_test_0;
#endif
    hostdata->E_test_1 = Ent_test_1;
    hostdata->E_test_2 = Ent_test_2;
#ifdef Ent_test_3
    hostdata->E_test_3 = Ent_test_3;
#endif
    hostdata->E_wait_reselect = Ent_wait_reselect;
    hostdata->E_dsa_code_begin = Ent_dsa_code_begin;

    hostdata->dsa_cmdout = A_dsa_cmdout;
    hostdata->dsa_cmnd = A_dsa_cmnd;
    hostdata->dsa_datain = A_dsa_datain;
    hostdata->dsa_dataout = A_dsa_dataout;
    hostdata->dsa_end = A_dsa_end;			
    hostdata->dsa_msgin = A_dsa_msgin;
    hostdata->dsa_msgout = A_dsa_msgout;
    hostdata->dsa_msgout_other = A_dsa_msgout_other;
    hostdata->dsa_next = A_dsa_next;
    hostdata->dsa_select = A_dsa_select;
    hostdata->dsa_start = Ent_dsa_code_template - Ent_dsa_zero;
    hostdata->dsa_status = A_dsa_status;
    hostdata->dsa_jump_dest = Ent_dsa_code_fix_jump - Ent_dsa_zero + 
	8 /* destination operand */;

    /* sanity check */
    if (A_dsa_fields_start != Ent_dsa_code_template_end - 
    	Ent_dsa_zero) 
    	printk("scsi%d : NCR dsa_fields start is %d not %d\n",
    	    host->host_no, A_dsa_fields_start, Ent_dsa_code_template_end - 
    	    Ent_dsa_zero);
#ifdef __powerpc__
/* The PowerPC is Big Endian - adjust script appropriately */
    script_ptr = hostdata->script;
    for (i = 0;  i < sizeof(SCRIPT);  i += sizeof(long))
    {
        *script_ptr++ = le32_to_cpu(*script_ptr);
    }
#endif    	    

    printk("scsi%d : NCR code relocated to 0x%lx (virt 0x%p)\n", host->host_no,
	virt_to_bus(hostdata->script), hostdata->script);
}

/*
 * Function : static int NCR53c8xx_run_tests (struct Scsi_Host *host)
 *
 * Purpose : run various verification tests on the NCR chip, 
 *	including interrupt generation, and proper bus mastering
 * 	operation.
 * 
 * Inputs : host - a properly initialized Scsi_Host structure
 *
 * Preconditions : the NCR chip must be in a halted state.
 *
 * Returns : 0 if all tests were successful, -1 on error.
 * 
 */

static int 
NCR53c8xx_run_tests (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    unsigned long timeout;
    u32 start;
    int failed, i;
    unsigned long flags;
    NCR53c7x0_local_setup(host);

    /* The NCR chip _must_ be idle to run the test scripts */

    save_flags(flags);
    cli();
    if (!hostdata->idle) {
	printk ("scsi%d : chip not idle, aborting tests\n", host->host_no);
	restore_flags(flags);
	return -1;
    }

    /* 
     * Check for functional interrupts, this could work as an
     * autoprobe routine.
     */

    if ((hostdata->options & OPTION_DEBUG_TEST1) && 
	    hostdata->state != STATE_DISABLED) {
	hostdata->idle = 0;
	hostdata->test_running = 1;
	hostdata->test_completed = -1;
	hostdata->test_dest = 0;
	hostdata->test_source = 0xdeadbeef;
	start = virt_to_bus (hostdata->script) + hostdata->E_test_1;
    	hostdata->state = STATE_RUNNING;
	printk ("scsi%d : test 1", host->host_no);
	NCR53c7x0_write32 (DSP_REG, start);
	printk (" started\n");
	restore_flags(flags);

	/* 
	 * This is currently a .5 second timeout, since (in theory) no slow 
	 * board will take that long.  In practice, we've seen one 
	 * pentium which occasionally fails with this, but works with 
	 * 10 times as much?
	 */

	timeout = jiffies + 5 * HZ / 10;
	while ((hostdata->test_completed == -1) && time_before(jiffies, timeout)) {
		barrier();
		cpu_relax();
	}

	failed = 1;
	if (hostdata->test_completed == -1)
	    printk ("scsi%d : driver test 1 timed out%s\n",host->host_no ,
		(hostdata->test_dest == 0xdeadbeef) ? 
		    " due to lost interrupt.\n"
		    "         Please verify that the correct IRQ is being used for your board,\n"
		    "	      and that the motherboard IRQ jumpering matches the PCI setup on\n"
		    "         PCI systems.\n"
		    "         If you are using a NCR53c810 board in a PCI system, you should\n" 
		    "         also verify that the board is jumpered to use PCI INTA, since\n"
		    "         most PCI motherboards lack support for INTB, INTC, and INTD.\n"
		    : "");
      else if (hostdata->test_completed != 1) 
	    printk ("scsi%d : test 1 bad interrupt value (%d)\n", 
		host->host_no, hostdata->test_completed);
	else 
	    failed = (hostdata->test_dest != 0xdeadbeef);

	if (hostdata->test_dest != 0xdeadbeef) {
	    printk ("scsi%d : driver test 1 read 0x%x instead of 0xdeadbeef indicating a\n"
                    "         probable cache invalidation problem.  Please configure caching\n"
		    "         as write-through or disabled\n",
		host->host_no, hostdata->test_dest);
	}

	if (failed) {
	    printk ("scsi%d : DSP = 0x%p (script at 0x%p, start at 0x%x)\n",
		host->host_no, bus_to_virt(NCR53c7x0_read32(DSP_REG)),
		hostdata->script, start);
	    printk ("scsi%d : DSPS = 0x%x\n", host->host_no,
		NCR53c7x0_read32(DSPS_REG));
	    return -1;
	}
    	hostdata->test_running = 0;
    }

    if ((hostdata->options & OPTION_DEBUG_TEST2) && 
	hostdata->state != STATE_DISABLED) {
	u32 dsa[48];
    	unsigned char identify = IDENTIFY(0, 0);
	unsigned char cmd[6];
	unsigned char data[36];
    	unsigned char status = 0xff;
    	unsigned char msg = 0xff;

    	cmd[0] = INQUIRY;
    	cmd[1] = cmd[2] = cmd[3] = cmd[5] = 0;
    	cmd[4] = sizeof(data); 

/* Need to adjust for endian-ness */    	
    	dsa[2] = le32_to_cpu(1);
    	dsa[3] = le32_to_cpu(virt_to_bus(&identify));
    	dsa[4] = le32_to_cpu(6);
    	dsa[5] = le32_to_cpu(virt_to_bus(&cmd));
    	dsa[6] = le32_to_cpu(sizeof(data));
    	dsa[7] = le32_to_cpu(virt_to_bus(&data));
    	dsa[8] = le32_to_cpu(1);
    	dsa[9] = le32_to_cpu(virt_to_bus(&status));
    	dsa[10] = le32_to_cpu(1);
    	dsa[11] = le32_to_cpu(virt_to_bus(&msg));

	for (i = 0; i < 3; ++i) {
	    cli();
	    if (!hostdata->idle) {
		printk ("scsi%d : chip not idle, aborting tests\n", host->host_no);
		restore_flags(flags);
		return -1;
	    }

	    /*	     SCNTL3         SDID	*/
	    dsa[0] = le32_to_cpu((0x33 << 24) | (i << 16))  ;
	    hostdata->idle = 0;
	    hostdata->test_running = 2;
	    hostdata->test_completed = -1;
	    start = virt_to_bus(hostdata->script) + hostdata->E_test_2;
	    hostdata->state = STATE_RUNNING;
	    NCR53c7x0_write32 (DSA_REG, virt_to_bus(dsa));
	    NCR53c7x0_write32 (DSP_REG, start);
	    restore_flags(flags);

	    timeout = jiffies + 5 * HZ;	/* arbitrary */
	    while ((hostdata->test_completed == -1) && time_before(jiffies, timeout)) {
	    	barrier();
		cpu_relax();
	    }
	    NCR53c7x0_write32 (DSA_REG, 0);

	    if (hostdata->test_completed == 2) {
		data[35] = 0;
		printk ("scsi%d : test 2 INQUIRY to target %d, lun 0 : %s\n",
		    host->host_no, i, data + 8);
		printk ("scsi%d : status ", host->host_no);
		print_status (status);
		printk ("\nscsi%d : message ", host->host_no);
		print_msg (&msg);
		printk ("\n");
	    } else if (hostdata->test_completed == 3) {
		printk("scsi%d : test 2 no connection with target %d\n",
		    host->host_no, i);
		if (!hostdata->idle) {
		    printk("scsi%d : not idle\n", host->host_no);
		    restore_flags(flags);
		    return -1;
		}
	    } else if (hostdata->test_completed == -1) {
		printk ("scsi%d : test 2 timed out\n", host->host_no);
		restore_flags(flags);
		return -1;
	    } 
	    hostdata->test_running = 0;
	}
    }

    restore_flags(flags);
    return 0;
}

/*
 * Function : static void NCR53c8xx_dsa_fixup (struct NCR53c7x0_cmd *cmd)
 *
 * Purpose : copy the NCR53c8xx dsa structure into cmd's dsa buffer,
 * 	performing all necessary relocation.
 *
 * Inputs : cmd, a NCR53c7x0_cmd structure with a dsa area large
 *	enough to hold the NCR53c8xx dsa.
 */

static void 
NCR53c8xx_dsa_fixup (struct NCR53c7x0_cmd *cmd) {
    Scsi_Cmnd *c = cmd->cmd;
    struct Scsi_Host *host = c->host;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
    	host->hostdata;
    int i;
#ifdef __powerpc__
    int len;
    unsigned long *dsa_ptr;
#endif    

    memcpy (cmd->dsa, hostdata->script + (hostdata->E_dsa_code_template / 4),
    	hostdata->E_dsa_code_template_end - hostdata->E_dsa_code_template);
#ifdef __powerpc__
    /* Note: the script has already been 'endianized' */
    dsa_ptr = cmd->dsa;
    len = hostdata->E_dsa_code_template_end - hostdata->E_dsa_code_template;
    for (i = 0;  i < len;  i += sizeof(long))
    {
       *dsa_ptr++ = le32_to_cpu(*dsa_ptr);
    }
#endif    	

    /* 
     * Note : within the NCR 'C' code, dsa points to the _start_
     * of the DSA structure, and _not_ the offset of dsa_zero within
     * that structure used to facilitate shorter signed offsets
     * for the 8 bit ALU.
     * 
     * The implications of this are that 
     * 
     * - 32 bit A_dsa_* absolute values require an additional 
     * 	 dsa_zero added to their value to be correct, since they are 
     *   relative to dsa_zero which is in essentially a separate
     *   space from the code symbols.
     *
     * - All other symbols require no special treatment.
     */

    patch_abs_tci_data (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
    	dsa_temp_lun, c->lun);
    patch_abs_32 (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
	dsa_temp_addr_next, virt_to_bus(&cmd->dsa_next_addr));
    patch_abs_32 (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
    	dsa_temp_next, virt_to_bus(cmd->dsa) + Ent_dsa_zero -
	Ent_dsa_code_template + A_dsa_next);
    patch_abs_32 (cmd->dsa, Ent_dsa_code_template / sizeof(u32), 
    	dsa_temp_sync, virt_to_bus((void *)hostdata->sync[c->target].script));
    patch_abs_tci_data (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
    	dsa_temp_target, c->target);
    /* XXX - new pointer stuff */
    patch_abs_32 (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
    	dsa_temp_addr_saved_pointer, virt_to_bus(&cmd->saved_data_pointer));
    patch_abs_32 (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
    	dsa_temp_addr_saved_residual, virt_to_bus(&cmd->saved_residual));
    patch_abs_32 (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
    	dsa_temp_addr_residual, virt_to_bus(&cmd->residual));

    /*  XXX - new start stuff */
    patch_abs_32 (cmd->dsa, Ent_dsa_code_template / sizeof(u32),
	dsa_temp_addr_dsa_value, virt_to_bus(&cmd->dsa_addr));
#ifdef __powerpc__
    dsa_ptr = cmd->dsa;    
    len = hostdata->E_dsa_code_template_end - hostdata->E_dsa_code_template;
    for (i = 0;  i < len;  i += sizeof(long))
    {
       *dsa_ptr++ = le32_to_cpu(*dsa_ptr);
    }
#endif    	

}

/* 
 * Function : run_process_issue_queue (void)
 * 
 * Purpose : insure that the coroutine is running and will process our 
 * 	request.  process_issue_queue_running is checked/set here (in an 
 *	inline function) rather than in process_issue_queue itself to reduce 
 * 	the chances of stack overflow.
 *
 */

static volatile int process_issue_queue_running = 0;

static __inline__ void 
run_process_issue_queue(void) {
    unsigned long flags;
    save_flags (flags);
    cli();
    if (!process_issue_queue_running) {
	process_issue_queue_running = 1;
        process_issue_queue(flags);
	/* 
         * process_issue_queue_running is cleared in process_issue_queue 
	 * once it can't do more work, and process_issue_queue exits with 
	 * interrupts disabled.
	 */
    }
    restore_flags (flags);
}

/*
 * Function : static void abnormal_finished (struct NCR53c7x0_cmd *cmd, int
 *	result)
 *
 * Purpose : mark SCSI command as finished, OR'ing the host portion 
 *	of the result word into the result field of the corresponding
 *	Scsi_Cmnd structure, and removing it from the internal queues.
 *
 * Inputs : cmd - command, result - entire result field
 *
 * Preconditions : the 	NCR chip should be in a halted state when 
 *	abnormal_finished is run, since it modifies structures which
 *	the NCR expects to have exclusive access to.
 */

static void 
abnormal_finished (struct NCR53c7x0_cmd *cmd, int result) {
    Scsi_Cmnd *c = cmd->cmd;
    struct Scsi_Host *host = c->host;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
    	host->hostdata;
    unsigned long flags;
    int left, found;
    volatile struct NCR53c7x0_cmd * linux_search;
    volatile struct NCR53c7x0_cmd * volatile *linux_prev;
    volatile u32 *ncr_prev, *curr, ncr_search;

#if 0
    printk ("scsi%d: abnormal finished\n", host->host_no);
#endif

    save_flags(flags);
    cli();
    found = 0;
    /* 
     * Traverse the NCR issue array until we find a match or run out 
     * of instructions.  Instructions in the NCR issue array are 
     * either JUMP or NOP instructions, which are 2 words in length.
     */


    for (found = 0, left = host->can_queue, curr = hostdata->schedule; 
	left > 0; --left, curr += 2)
    {
	if (issue_to_cmd (host, hostdata, (u32 *) curr) == cmd) 
	{
	    curr[0] = hostdata->NOP_insn;
	    curr[1] = le32_to_cpu(0xdeadbeef);
	    ++found;
	    break;
	}
    }
	
    /* 
     * Traverse the NCR reconnect list of DSA structures until we find 
     * a pointer to this dsa or have found too many command structures.  
     * We let prev point at the next field of the previous element or 
     * head of the list, so we don't do anything different for removing 
     * the head element.  
     */

    for (left = host->can_queue,
	    ncr_search = le32_to_cpu(hostdata->reconnect_dsa_head), 
	    ncr_prev = &hostdata->reconnect_dsa_head;
	left >= 0 && ncr_search && 
	    ((char*)bus_to_virt(ncr_search) + hostdata->dsa_start) 
		!= (char *) cmd->dsa;
	ncr_prev = (u32*) ((char*)bus_to_virt(ncr_search) + 
	    hostdata->dsa_next), ncr_search = le32_to_cpu(*ncr_prev), --left);

    if (left < 0) {
	printk("scsi%d: loop detected in ncr reconnect list\n",
	    host->host_no);
    } else if (ncr_search) {
	if (found)
	    printk("scsi%d: scsi %ld in ncr issue array and reconnect lists\n",
		host->host_no, c->pid);
	else {
	    volatile u32 * next = (u32 *) 
	    	((char *)bus_to_virt(ncr_search) + hostdata->dsa_next);
	    *ncr_prev = *next;
/* If we're at the tail end of the issue queue, update that pointer too. */
	    found = 1;
	}
    }

    /*
     * Traverse the host running list until we find this command or discover
     * we have too many elements, pointing linux_prev at the next field of the 
     * linux_previous element or head of the list, search at this element.
     */

    for (left = host->can_queue, linux_search = hostdata->running_list, 
	    linux_prev = &hostdata->running_list;
	left >= 0 && linux_search && linux_search != cmd;
	linux_prev = &(linux_search->next), 
	    linux_search = linux_search->next, --left);
    
    if (left < 0) 
	printk ("scsi%d: loop detected in host running list for scsi pid %ld\n",
	    host->host_no, c->pid);
    else if (linux_search) {
	*linux_prev = linux_search->next;
	--hostdata->busy[c->target][c->lun];
    }

    /* Return the NCR command structure to the free list */
    cmd->next = hostdata->free;
    hostdata->free = cmd;
    c->host_scribble = NULL;

    /* And return */
    c->result = result;
    c->scsi_done(c);

    restore_flags(flags);
    run_process_issue_queue();
}

/* 
 * Function : static void intr_break (struct Scsi_Host *host,
 * 	struct NCR53c7x0_cmd *cmd)
 *
 * Purpose :  Handler for breakpoint interrupts from a SCSI script
 *
 * Inputs : host - pointer to this host adapter's structure,
 * 	cmd - pointer to the command (if any) dsa was pointing 
 * 	to.
 *
 */

static void 
intr_break (struct Scsi_Host *host, struct 
    NCR53c7x0_cmd *cmd) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_break *bp;
#if 0
    Scsi_Cmnd *c = cmd ? cmd->cmd : NULL;
#endif
    u32 *dsp;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;		
    unsigned long flags;
    NCR53c7x0_local_setup(host);

    /*
     * Find the break point corresponding to this address, and 
     * dump the appropriate debugging information to standard 
     * output.  
     */
    save_flags(flags);
    cli();
    dsp = (u32 *) bus_to_virt(NCR53c7x0_read32(DSP_REG));
    for (bp = hostdata->breakpoints; bp && bp->address != dsp; 
    	bp = bp->next);
    if (!bp) 
    	panic("scsi%d : break point interrupt from %p with no breakpoint!",
    	    host->host_no, dsp);

    /*
     * Configure the NCR chip for manual start mode, so that we can 
     * point the DSP register at the instruction that follows the 
     * INT int_debug_break instruction.
     */

    NCR53c7x0_write8 (hostdata->dmode, 
	NCR53c7x0_read8(hostdata->dmode)|DMODE_MAN);

    /*
     * And update the DSP register, using the size of the old 
     * instruction in bytes.
     */

    restore_flags(flags);
}
/*
 * Function : static void print_synchronous (const char *prefix, 
 *	const unsigned char *msg)
 * 
 * Purpose : print a pretty, user and machine parsable representation
 *	of a SDTR message, including the "real" parameters, data
 *	clock so we can tell transfer rate at a glance.
 *
 * Inputs ; prefix - text to prepend, msg - SDTR message (5 bytes)
 */

static void
print_synchronous (const char *prefix, const unsigned char *msg) {
    if (msg[4]) {
	int Hz = 1000000000 / (msg[3] * 4);
	int integer = Hz / 1000000;
	int fraction = (Hz - (integer * 1000000)) / 10000;
	printk ("%speriod %dns offset %d %d.%02dMHz %s SCSI%s\n",
	    prefix, (int) msg[3] * 4, (int) msg[4], integer, fraction,
	    (((msg[3] * 4) < 200) ? "FAST" : "synchronous"),
	    (((msg[3] * 4) < 200) ? "-II" : ""));
    } else 
	printk ("%sasynchronous SCSI\n", prefix);
}

/*
 * Function : static void set_synchronous (struct Scsi_Host *host, 
 *	 	int target, int sxfer, int scntl3, int now_connected)
 *
 * Purpose : reprogram transfers between the selected SCSI initiator and 
 *	target with the given register values; in the indirect
 *	select operand, reselection script, and chip registers.
 *
 * Inputs : host - NCR53c7,8xx SCSI host, target - number SCSI target id,
 *	sxfer and scntl3 - NCR registers. now_connected - if non-zero, 
 *	we should reprogram the registers now too.
 */

static void
set_synchronous (struct Scsi_Host *host, int target, int sxfer, int scntl3,
    int now_connected) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *) 
	host->hostdata;
    u32 *script;
    NCR53c7x0_local_setup(host);

    /* These are eight bit registers */
    sxfer &= 0xff;
    scntl3 &= 0xff;

    hostdata->sync[target].sxfer_sanity = sxfer;
    hostdata->sync[target].scntl3_sanity = scntl3;

/* 
 * HARD CODED : synchronous script is EIGHT words long.  This 
 * must agree with 53c7.8xx.h
 */

    if ((hostdata->chip != 700) && (hostdata->chip != 70066)) {
	hostdata->sync[target].select_indirect = (scntl3 << 24) | 
	    (target << 16) | (sxfer << 8);

	script = (u32 *) hostdata->sync[target].script;

	/* XXX - add NCR53c7x0 code to reprogram SCF bits if we want to */
	if ((hostdata->chip / 100) == 8) {
	    script[0] = ((DCMD_TYPE_RWRI | DCMD_RWRI_OPC_MODIFY |
		DCMD_RWRI_OP_MOVE) << 24) |
		(SCNTL3_REG_800 << 16) | (scntl3 << 8);
	    script[1] = 0;
	    script += 2;
	}

	script[0] = ((DCMD_TYPE_RWRI | DCMD_RWRI_OPC_MODIFY |
	    DCMD_RWRI_OP_MOVE) << 24) |
		(SXFER_REG << 16) | (sxfer << 8);
	script[1] = 0;
	script += 2;

#ifdef DEBUG_SYNC_INTR
	if (hostdata->options & OPTION_DEBUG_DISCONNECT) {
	    script[0] = ((DCMD_TYPE_TCI|DCMD_TCI_OP_INT) << 24) | DBC_TCI_TRUE;
	    script[1] = DEBUG_SYNC_INTR;
	    script += 2;
	}
#endif

	script[0] = ((DCMD_TYPE_TCI|DCMD_TCI_OP_RETURN) << 24) | DBC_TCI_TRUE;
	script[1] = 0;
	script += 2;
    }

    if (hostdata->options & OPTION_DEBUG_SYNCHRONOUS) 
	printk ("scsi%d : target %d sync parameters are sxfer=0x%x, scntl3=0x%x\n",
	host->host_no, target, sxfer, scntl3);

    if (now_connected) {
	if ((hostdata->chip / 100) == 8) 
	    NCR53c7x0_write8(SCNTL3_REG_800, scntl3);
	NCR53c7x0_write8(SXFER_REG, sxfer);
    }
}


/*
 * Function : static int asynchronous (struct Scsi_Host *host, int target)
 *
 * Purpose : reprogram between the selected SCSI Host adapter and target 
 *      (assumed to be currently connected) for asynchronous transfers.
 *
 * Inputs : host - SCSI host structure, target - numeric target ID.
 *
 * Preconditions : the NCR chip should be in one of the halted states
 */
    
static void
asynchronous (struct Scsi_Host *host, int target) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    NCR53c7x0_local_setup(host);
    set_synchronous (host, target, /* no offset */ 0, hostdata->saved_scntl3,
	1);
    printk ("scsi%d : setting target %d to asynchronous SCSI\n",
	host->host_no, target);
}

/* 
 * XXX - do we want to go out of our way (ie, add extra code to selection
 * 	in the NCR53c710/NCR53c720 script) to reprogram the synchronous
 * 	conversion bits, or can we be content in just setting the 
 * 	sxfer bits?
 */

/* Table for NCR53c8xx synchronous values */
static const struct {
    int div;		/* Total clock divisor * 10 */
    unsigned char scf;	/* */
    unsigned char tp;	/* 4 + tp = xferp divisor */
} syncs[] = {
/*	div	scf	tp	div	scf	tp	div	scf	tp */
    {	40,	1,	0}, {	50,	1,	1}, {	60,	1,	2}, 
    {	70,	1,	3}, {	75,	2,	1}, {	80,	1,	4},
    {	90,	1,	5}, {	100,	1,	6}, {	105,	2,	3},
    {	110,	1,	7}, {	120,	2,	4}, {	135,	2,	5},
    {	140,	3,	3}, {	150,	2,	6}, {	160,	3,	4},
    {	165,	2,	7}, {	180,	3,	5}, {	200,	3,	6},
    {	210,	4,	3}, {	220,	3,	7}, {	240,	4,	4},
    {	270,	4,	5}, {	300,	4,	6}, {	330,	4,	7}
};

/*
 * Function : static void synchronous (struct Scsi_Host *host, int target, 
 *	char *msg)
 *
 * Purpose : reprogram transfers between the selected SCSI initiator and 
 *	target for synchronous SCSI transfers such that the synchronous 
 *	offset is less than that requested and period at least as long 
 *	as that requested.  Also modify *msg such that it contains 
 *	an appropriate response. 
 *
 * Inputs : host - NCR53c7,8xx SCSI host, target - number SCSI target id,
 *	msg - synchronous transfer request.
 */


static void 
synchronous (struct Scsi_Host *host, int target, char *msg) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    int desire, divisor, i, limit;
    unsigned char scntl3, sxfer;
/* The diagnostic message fits on one line, even with max. width integers */
    char buf[80];	
   
/* Desired transfer clock in Hz */
    desire = 1000000000L / (msg[3] * 4);
/* Scale the available SCSI clock by 10 so we get tenths */
    divisor = (hostdata->scsi_clock * 10) / desire;

/* NCR chips can handle at most an offset of 8 */
    if (msg[4] > 8)
	msg[4] = 8;

    if (hostdata->options & OPTION_DEBUG_SDTR)
    	printk("scsi%d : optimal synchronous divisor of %d.%01d\n", 
	    host->host_no, divisor / 10, divisor % 10);

    limit = (sizeof(syncs) / sizeof(syncs[0]) -1);
    for (i = 0; (i < limit) && (divisor > syncs[i].div); ++i);

    if (hostdata->options & OPTION_DEBUG_SDTR)
    	printk("scsi%d : selected synchronous divisor of %d.%01d\n", 
	    host->host_no, syncs[i].div / 10, syncs[i].div % 10);

    msg[3] = ((1000000000L / hostdata->scsi_clock) * syncs[i].div / 10 / 4);

    if (hostdata->options & OPTION_DEBUG_SDTR)
    	printk("scsi%d : selected synchronous period of %dns\n", host->host_no,
	    msg[3] * 4);

    scntl3 = (hostdata->chip / 100 == 8) ? ((hostdata->saved_scntl3 & 
	~SCNTL3_800_SCF_MASK) | (syncs[i].scf << SCNTL3_800_SCF_SHIFT)) : 0;
    sxfer = (msg[4] << SXFER_MO_SHIFT) | ((syncs[i].tp) << SXFER_TP_SHIFT);
    if (hostdata->options & OPTION_DEBUG_SDTR)
    	printk ("scsi%d : sxfer=0x%x scntl3=0x%x\n", 
	    host->host_no, (int) sxfer, (int) scntl3);
    set_synchronous (host, target, sxfer, scntl3, 1);
    sprintf (buf, "scsi%d : setting target %d to ", host->host_no, target);
    print_synchronous (buf, msg);
}

/* 
 * Function : static int NCR53c8x0_dstat_sir_intr (struct Scsi_Host *host,
 * 	struct NCR53c7x0_cmd *cmd)
 *
 * Purpose :  Handler for INT generated instructions for the 
 * 	NCR53c810/820 SCSI SCRIPT
 *
 * Inputs : host - pointer to this host adapter's structure,
 * 	cmd - pointer to the command (if any) dsa was pointing 
 * 	to.
 *
 */

static int 
NCR53c8x0_dstat_sir_intr (struct Scsi_Host *host, struct 
    NCR53c7x0_cmd *cmd) {
    NCR53c7x0_local_declare();
    int print;
    Scsi_Cmnd *c = cmd ? cmd->cmd : NULL;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;		
    u32 dsps,*dsp;	/* Argument of the INT instruction */
    NCR53c7x0_local_setup(host);
    dsps = NCR53c7x0_read32(DSPS_REG);
    dsp = (u32 *) bus_to_virt(NCR53c7x0_read32(DSP_REG));

    if (hostdata->options & OPTION_DEBUG_INTR)
	printk ("scsi%d : DSPS = 0x%x\n", host->host_no, dsps);

    switch (dsps) {
    case A_int_msg_1:
	print = 1;
	switch (hostdata->msg_buf[0]) {
	/* 
	 * Unless we've initiated synchronous negotiation, I don't
	 * think that this should happen.
	 */
	case MESSAGE_REJECT:
	    hostdata->dsp = hostdata->script + hostdata->E_accept_message /
		sizeof(u32);
	    hostdata->dsp_changed = 1;
	    if (cmd && (cmd->flags & CMD_FLAG_SDTR)) {
		printk ("scsi%d : target %d rejected SDTR\n", host->host_no, 
		    c->target);
		cmd->flags &= ~CMD_FLAG_SDTR;
		asynchronous (host, c->target);
		print = 0;
	    } 
	    break;
	case INITIATE_RECOVERY:
	    printk ("scsi%d : extended contingent allegiance not supported yet, rejecting\n",
		host->host_no);
	    /* Fall through to default */
	    hostdata->dsp = hostdata->script + hostdata->E_reject_message /
		sizeof(u32);
	    hostdata->dsp_changed = 1;
	    break;
	default:
	    printk ("scsi%d : unsupported message, rejecting\n",
		host->host_no);
	    hostdata->dsp = hostdata->script + hostdata->E_reject_message /
		sizeof(u32);
	    hostdata->dsp_changed = 1;
	}
	if (print) {
	    printk ("scsi%d : received message", host->host_no);
	    if (c) 
	    	printk (" from target %d lun %d ", c->target, c->lun);
	    print_msg ((unsigned char *) hostdata->msg_buf);
	    printk("\n");
	}
	
	return SPECIFIC_INT_NOTHING;


    case A_int_msg_sdtr:
/*
 * At this point, hostdata->msg_buf contains
 * 0 EXTENDED MESSAGE
 * 1 length 
 * 2 SDTR
 * 3 period * 4ns
 * 4 offset
 */

	if (cmd) {
	    char buf[80];
	    sprintf (buf, "scsi%d : target %d %s ", host->host_no, c->target,
		(cmd->flags & CMD_FLAG_SDTR) ? "accepting" : "requesting");
	    print_synchronous (buf, (unsigned char *) hostdata->msg_buf);

	/* 
	 * Initiator initiated, won't happen unless synchronous 
	 * 	transfers are enabled.  If we get a SDTR message in
	 * 	response to our SDTR, we should program our parameters
	 * 	such that 
	 *		offset <= requested offset
	 *		period >= requested period		 	
   	 */
	    if (cmd->flags & CMD_FLAG_SDTR) {
		cmd->flags &= ~CMD_FLAG_SDTR; 
		if (hostdata->msg_buf[4]) 
		    synchronous (host, c->target, (unsigned char *) 
		    	hostdata->msg_buf);
		else 
		    asynchronous (host, c->target);
		hostdata->dsp = hostdata->script + hostdata->E_accept_message /
		    sizeof(u32);
		hostdata->dsp_changed = 1;
		return SPECIFIC_INT_NOTHING;
	    } else {
		if (hostdata->options & OPTION_SYNCHRONOUS)  {
		    cmd->flags |= CMD_FLAG_DID_SDTR;
		    synchronous (host, c->target, (unsigned char *) 
			hostdata->msg_buf);
		} else {
		    hostdata->msg_buf[4] = 0;		/* 0 offset = async */
		    asynchronous (host, c->target);
		}
		patch_dsa_32 (cmd->dsa, dsa_msgout_other, 0, le32_to_cpu(5));
		patch_dsa_32 (cmd->dsa, dsa_msgout_other, 1, (u32) 
		    le32_to_cpu(virt_to_bus ((void *)&hostdata->msg_buf)));
		hostdata->dsp = hostdata->script + 
		    hostdata->E_respond_message / sizeof(u32);
		hostdata->dsp_changed = 1;
	    }
	    return SPECIFIC_INT_NOTHING;
	}
	/* Fall through to abort if we couldn't find a cmd, and 
	   therefore a dsa structure to twiddle */
    case A_int_msg_wdtr:
	hostdata->dsp = hostdata->script + hostdata->E_reject_message /
	    sizeof(u32);
	hostdata->dsp_changed = 1;
	return SPECIFIC_INT_NOTHING;
    case A_int_err_unexpected_phase:
	if (hostdata->options & OPTION_DEBUG_INTR) 
	    printk ("scsi%d : unexpected phase\n", host->host_no);
	return SPECIFIC_INT_ABORT;
    case A_int_err_selected:
	printk ("scsi%d : selected by target %d\n", host->host_no,
	    (int) NCR53c7x0_read8(SDID_REG_800) &7);
	hostdata->dsp = hostdata->script + hostdata->E_target_abort / 
    	    sizeof(u32);
	hostdata->dsp_changed = 1;
	return SPECIFIC_INT_NOTHING;
    case A_int_err_unexpected_reselect:
	printk ("scsi%d : unexpected reselect by target %d lun %d\n", 
	    host->host_no, (int) NCR53c7x0_read8(SDID_REG_800) & 7,
	    hostdata->reselected_identify & 7);
	hostdata->dsp = hostdata->script + hostdata->E_initiator_abort /
    	    sizeof(u32);
	hostdata->dsp_changed = 1;
	return SPECIFIC_INT_NOTHING;
/*
 * Since contingent allegiance conditions are cleared by the next 
 * command issued to a target, we must issue a REQUEST SENSE 
 * command after receiving a CHECK CONDITION status, before
 * another command is issued.
 * 
 * Since this NCR53c7x0_cmd will be freed after use, we don't 
 * care if we step on the various fields, so modify a few things.
 */
    case A_int_err_check_condition: 
#if 0
	if (hostdata->options & OPTION_DEBUG_INTR) 
#endif
	    printk ("scsi%d : CHECK CONDITION\n", host->host_no);
	if (!c) {
	    printk("scsi%d : CHECK CONDITION with no SCSI command\n",
		host->host_no);
	    return SPECIFIC_INT_PANIC;
	}

	/* 
	 * FIXME : this uses the normal one-byte selection message.
	 * 	We may want to renegotiate for synchronous & WIDE transfers
	 * 	since these could be the crux of our problem.
	 *
	 hostdata->NOP_insn* FIXME : once SCSI-II tagged queuing is implemented, we'll
	 * 	have to set this up so that the rest of the DSA
	 *	agrees with this being an untagged queue'd command.
	 */

    	patch_dsa_32 (cmd->dsa, dsa_msgout, 0, le32_to_cpu(1));

    	/* 
    	 * Modify the table indirect for COMMAND OUT phase, since 
    	 * Request Sense is a six byte command.
    	 */

    	patch_dsa_32 (cmd->dsa, dsa_cmdout, 0, le32_to_cpu(6));

	c->cmnd[0] = REQUEST_SENSE;
	c->cmnd[1] &= 0xe0;	/* Zero all but LUN */
	c->cmnd[2] = 0;
	c->cmnd[3] = 0;
	c->cmnd[4] = sizeof(c->sense_buffer);
	c->cmnd[5] = 0; 

	/*
	 * Disable dataout phase, and program datain to transfer to the 
	 * sense buffer, and add a jump to other_transfer after the 
    	 * command so overflow/underrun conditions are detected.
	 */

    	patch_dsa_32 (cmd->dsa, dsa_dataout, 0, 
	    le32_to_cpu(virt_to_bus(hostdata->script) + hostdata->E_other_transfer));
    	patch_dsa_32 (cmd->dsa, dsa_datain, 0, 
	    le32_to_cpu(virt_to_bus(cmd->data_transfer_start)));
    	cmd->data_transfer_start[0] = le32_to_cpu((((DCMD_TYPE_BMI | DCMD_BMI_OP_MOVE_I | 
    	    DCMD_BMI_IO)) << 24) | sizeof(c->sense_buffer));
    	cmd->data_transfer_start[1] = (u32) le32_to_cpu(virt_to_bus(c->sense_buffer));

	cmd->data_transfer_start[2] = le32_to_cpu(((DCMD_TYPE_TCI | DCMD_TCI_OP_JUMP) 
    	    << 24) | DBC_TCI_TRUE);
	cmd->data_transfer_start[3] = (u32) le32_to_cpu(virt_to_bus(hostdata->script) + 
	    hostdata->E_other_transfer);

    	/*
    	 * Currently, this command is flagged as completed, ie 
    	 * it has valid status and message data.  Reflag it as
    	 * incomplete.  Q - need to do something so that original
	 * status, etc are used.
    	 */

	cmd->cmd->result = le32_to_cpu(0xffff);		

	/* 
	 * Restart command as a REQUEST SENSE.
	 */
	hostdata->dsp = (u32 *) hostdata->script + hostdata->E_select /
	    sizeof(u32);
	hostdata->dsp_changed = 1;
	return SPECIFIC_INT_NOTHING;
    case A_int_debug_break:
	return SPECIFIC_INT_BREAK;
    case A_int_norm_aborted:
	hostdata->dsp = (u32 *) hostdata->schedule;
	hostdata->dsp_changed = 1;
	if (cmd)
	    abnormal_finished (cmd, DID_ERROR << 16);
	return SPECIFIC_INT_NOTHING;
    case A_int_test_1:
    case A_int_test_2:
	hostdata->idle = 1;
	hostdata->test_completed = (dsps - A_int_test_1) / 0x00010000 + 1;
	if (hostdata->options & OPTION_DEBUG_INTR)
	    printk("scsi%d : test%d complete\n", host->host_no,
		hostdata->test_completed);
	return SPECIFIC_INT_NOTHING;
#ifdef A_int_debug_reselected_ok
    case A_int_debug_reselected_ok:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR|
    	    	OPTION_DEBUG_DISCONNECT)) {
	    /* 
	     * Note - this dsa is not based on location relative to 
	     * the command structure, but to location relative to the 
	     * DSA register 
	     */	
	    u32 *dsa;
	    dsa = (u32 *) bus_to_virt (NCR53c7x0_read32(DSA_REG));

	    printk("scsi%d : reselected_ok (DSA = 0x%x (virt 0x%p)\n", 
		host->host_no, NCR53c7x0_read32(DSA_REG), dsa);
	    printk("scsi%d : resume address is 0x%x (virt 0x%p)\n",
		    host->host_no, cmd->saved_data_pointer,
		    bus_to_virt(le32_to_cpu(cmd->saved_data_pointer)));
	    print_insn (host, hostdata->script + Ent_reselected_ok / 
    	    	    sizeof(u32), "", 1);
    	    printk ("scsi%d : sxfer=0x%x, scntl3=0x%x\n",
		host->host_no, NCR53c7x0_read8(SXFER_REG),
		NCR53c7x0_read8(SCNTL3_REG_800));
	    if (c) {
		print_insn (host, (u32 *) 
		    hostdata->sync[c->target].script, "", 1);
		print_insn (host, (u32 *) 
		    hostdata->sync[c->target].script + 2, "", 1);
	    }
	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_reselect_check
    case A_int_debug_reselect_check:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR)) {
	    u32 *dsa;
#if 0
	    u32 *code;
#endif
	    /* 
	     * Note - this dsa is not based on location relative to 
	     * the command structure, but to location relative to the 
	     * DSA register 
	     */	
	    dsa = bus_to_virt (NCR53c7x0_read32(DSA_REG));
	    printk("scsi%d : reselected_check_next (DSA = 0x%lx (virt 0x%p))\n",
		host->host_no, virt_to_bus(dsa), dsa);
	    if (dsa) {
		printk("scsi%d : resume address is 0x%x (virt 0x%p)\n",
		    host->host_no, cmd->saved_data_pointer,
		    bus_to_virt (le32_to_cpu(cmd->saved_data_pointer)));
#if 0
		printk("scsi%d : template code :\n", host->host_no);
		for (code = dsa + (Ent_dsa_code_check_reselect - Ent_dsa_zero) 
		    / sizeof(u32); code < (dsa + Ent_dsa_zero / sizeof(u32)); 
		    code += print_insn (host, code, "", 1));
#endif
	    }
	    print_insn (host, hostdata->script + Ent_reselected_ok / 
    	    	    sizeof(u32), "", 1);
	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_dsa_schedule
    case A_int_debug_dsa_schedule:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR)) {
	    u32 *dsa;
	    /* 
	     * Note - this dsa is not based on location relative to 
	     * the command structure, but to location relative to the 
	     * DSA register 
	     */	
	    dsa = (u32 *) bus_to_virt (NCR53c7x0_read32(DSA_REG));
	    printk("scsi%d : dsa_schedule (old DSA = 0x%lx (virt 0x%p))\n", 
		host->host_no, virt_to_bus(dsa), dsa);
	    if (dsa) 
		printk("scsi%d : resume address is 0x%x (virt 0x%p)\n"
		       "         (temp was 0x%x (virt 0x%p))\n",
		    host->host_no, cmd->saved_data_pointer,
		    bus_to_virt (le32_to_cpu(cmd->saved_data_pointer)),
		    NCR53c7x0_read32 (TEMP_REG),
		    bus_to_virt (NCR53c7x0_read32(TEMP_REG)));
	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_scheduled
    case A_int_debug_scheduled:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR)) {
	    printk("scsi%d : new I/O 0x%x (virt 0x%p) scheduled\n", 
		host->host_no, NCR53c7x0_read32(DSA_REG),
	    	bus_to_virt(NCR53c7x0_read32(DSA_REG)));
	}
	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_idle
    case A_int_debug_idle:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR)) {
	    printk("scsi%d : idle\n", host->host_no);
	}
	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_cmd
    case A_int_debug_cmd:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR)) {
	    printk("scsi%d : command sent\n");
	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_dsa_loaded
    case A_int_debug_dsa_loaded:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR)) {
	    printk("scsi%d : DSA loaded with 0x%x (virt 0x%p)\n", host->host_no,
		NCR53c7x0_read32(DSA_REG), 
		bus_to_virt(NCR53c7x0_read32(DSA_REG)));
	}
	return SPECIFIC_INT_RESTART; 
#endif
#ifdef A_int_debug_reselected
    case A_int_debug_reselected:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR|
	    OPTION_DEBUG_DISCONNECT)) {
	    printk("scsi%d : reselected by target %d lun %d\n",
		host->host_no, (int) NCR53c7x0_read8(SDID_REG_800) & ~0x80, 
		(int) hostdata->reselected_identify & 7);
	    print_queues(host);
	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_disconnect_msg
    case A_int_debug_disconnect_msg:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR)) {
	    if (c)
		printk("scsi%d : target %d lun %d disconnecting\n", 
		    host->host_no, c->target, c->lun);
	    else
		printk("scsi%d : unknown target disconnecting\n",
		    host->host_no);
	}
	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_disconnected
    case A_int_debug_disconnected:
	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR|
		OPTION_DEBUG_DISCONNECT)) {
	    printk ("scsi%d : disconnected, new queues are\n", 
		host->host_no);
	    print_queues(host);
#if 0
    	    printk ("scsi%d : sxfer=0x%x, scntl3=0x%x\n",
		host->host_no, NCR53c7x0_read8(SXFER_REG),
		NCR53c7x0_read8(SCNTL3_REG_800));
#endif
	    if (c) {
		print_insn (host, (u32 *) 
		    hostdata->sync[c->target].script, "", 1);
		print_insn (host, (u32 *) 
		    hostdata->sync[c->target].script + 2, "", 1);
	    }
	}
	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_panic
    case A_int_debug_panic:
	printk("scsi%d : int_debug_panic received\n", host->host_no);
	print_lots (host);
	return SPECIFIC_INT_PANIC;
#endif
#ifdef A_int_debug_saved
    case A_int_debug_saved:
    	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR|
    	    OPTION_DEBUG_DISCONNECT)) {
    	    printk ("scsi%d : saved data pointer 0x%x (virt 0x%p)\n",
    	    	host->host_no, cmd->saved_data_pointer,
		bus_to_virt (le32_to_cpu(cmd->saved_data_pointer)));
    	    print_progress (c);
    	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_restored
    case A_int_debug_restored:
    	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR|
    	    OPTION_DEBUG_DISCONNECT)) {
    	    if (cmd) {
		int size;
    	    	printk ("scsi%d : restored data pointer 0x%x (virt 0x%p)\n",
    	    	    host->host_no, cmd->saved_data_pointer, bus_to_virt (
		    le32_to_cpu(cmd->saved_data_pointer)));
		size = print_insn (host, (u32 *) 
		    bus_to_virt(le32_to_cpu(cmd->saved_data_pointer)), "", 1);
		size = print_insn (host, (u32 *) 
		    bus_to_virt(le32_to_cpu(cmd->saved_data_pointer)) + size, "", 1);
    	    	print_progress (c);
	    }
#if 0
	    printk ("scsi%d : datapath residual %d\n",
		host->host_no, datapath_residual (host)) ;
#endif
    	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_sync
    case A_int_debug_sync:
    	if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR|
    	    OPTION_DEBUG_DISCONNECT|OPTION_DEBUG_SDTR)) {
	    unsigned char sxfer = NCR53c7x0_read8 (SXFER_REG),
		scntl3 = NCR53c7x0_read8 (SCNTL3_REG_800);
	    if (c) {
		if (sxfer != hostdata->sync[c->target].sxfer_sanity ||
		    scntl3 != hostdata->sync[c->target].scntl3_sanity) {
		   	printk ("scsi%d :  sync sanity check failed sxfer=0x%x, scntl3=0x%x",
			    host->host_no, sxfer, scntl3);
			NCR53c7x0_write8 (SXFER_REG, sxfer);
			NCR53c7x0_write8 (SCNTL3_REG_800, scntl3);
		    }
	    } else 
    	    	printk ("scsi%d : unknown command sxfer=0x%x, scntl3=0x%x\n",
		    host->host_no, (int) sxfer, (int) scntl3);
	}
    	return SPECIFIC_INT_RESTART;
#endif
#ifdef A_int_debug_datain
	case A_int_debug_datain:
	    if (hostdata->options & (OPTION_DEBUG_SCRIPT|OPTION_DEBUG_INTR|
		OPTION_DEBUG_DISCONNECT|OPTION_DEBUG_SDTR)) {
		int size;
		printk ("scsi%d : In do_datain (%s) sxfer=0x%x, scntl3=0x%x\n"
			"         datapath residual=%d\n",
		    host->host_no, sbcl_to_phase (NCR53c7x0_read8 (SBCL_REG)),
		    (int) NCR53c7x0_read8(SXFER_REG), 
		    (int) NCR53c7x0_read8(SCNTL3_REG_800),
		    datapath_residual (host)) ;
		print_insn (host, dsp, "", 1);
		size = print_insn (host, (u32 *) bus_to_virt(le32_to_cpu(dsp[1])), "", 1);
		print_insn (host, (u32 *) bus_to_virt(le32_to_cpu(dsp[1])) + size, "", 1);
	   } 
	return SPECIFIC_INT_RESTART;
#endif
/* 
 * FIXME : for 7xx support, we need to read SDID_REG_700 and handle 
 *	the comparison as bitfielded,  not binary.
 */
#ifdef A_int_debug_check_dsa
	case A_int_debug_check_dsa:
	    if (NCR53c7x0_read8 (SCNTL1_REG) & SCNTL1_CON) {
		int sdid = NCR53c7x0_read8 (SDID_REG_800) & 15;
		char *where = dsp - NCR53c7x0_insn_size(NCR53c7x0_read8 
			(DCMD_REG)) == hostdata->script + 
		    	Ent_select_check_dsa / sizeof(u32) ?
		    "selection" : "reselection";
		if (c && sdid != c->target) {
		    printk ("scsi%d : SDID target %d != DSA target %d at %s\n",
			host->host_no, sdid, c->target, where);
		    print_lots(host);
		    dump_events (host, 20);
		    return SPECIFIC_INT_PANIC;
		}
	    }
	    return SPECIFIC_INT_RESTART;
#endif
    default:
	if ((dsps & 0xff000000) == 0x03000000) {
	     printk ("scsi%d : misc debug interrupt 0x%x\n",
		host->host_no, dsps);
	    return SPECIFIC_INT_RESTART;
	} else if ((dsps & 0xff000000) == 0x05000000) {
	    if (hostdata->events) {
		struct NCR53c7x0_event *event;
		++hostdata->event_index;
		if (hostdata->event_index >= hostdata->event_size)
		    hostdata->event_index = 0;
		event = (struct NCR53c7x0_event *) hostdata->events + 
		    hostdata->event_index;
		event->event = (enum ncr_event) dsps;
		event->dsa = bus_to_virt(NCR53c7x0_read32(DSA_REG));
	    /* FIXME : this needs to change for the '7xx family */
		if (NCR53c7x0_read8 (SCNTL1_REG) & SCNTL1_CON) 
			event->target = NCR53c7x0_read8(SSID_REG_800);
		else 
			event->target = 255;

		if (event->event == EVENT_RESELECT)
		    event->lun = hostdata->reselected_identify & 0xf;
		else if (c)
		    event->lun = c->lun;
		else
		    event->lun = 255;
		do_gettimeofday(&(event->time));
		if (c) {
		    event->pid = c->pid;
		    memcpy ((void *) event->cmnd, (void *) c->cmnd, 
			sizeof (event->cmnd));
		} else {
		    event->pid = -1;
		}
	    }
	    return SPECIFIC_INT_RESTART;
	}

	printk ("scsi%d : unknown user interrupt 0x%x\n", 
	    host->host_no, (unsigned) dsps);
	return SPECIFIC_INT_PANIC;
    }
}

/* 
 * XXX - the stock NCR assembler won't output the scriptu.h file,
 * which undefine's all #define'd CPP symbols from the script.h
 * file, which will create problems if you use multiple scripts
 * with the same  symbol names.
 *
 * If you insist on using NCR's assembler, you could generate
 * scriptu.h from script.h using something like 
 *
 * grep #define script.h | \
 * sed 's/#define[ 	][ 	]*\([_a-zA-Z][_a-zA-Z0-9]*\).*$/#undefine \1/' \
 * > scriptu.h
 */

#include "53c8xx_u.h"

/* XXX - add alternate script handling code here */


#ifdef NCR_DEBUG
/*
 * Debugging without a debugger is no fun. So, I've provided 
 * a debugging interface in the NCR53c7x0 driver.  To avoid
 * kernel cruft, there's just enough here to act as an interface
 * to a user level debugger (aka, GDB).
 *
 *
 * The following restrictions apply to debugger commands : 
 * 1.  The command must be terminated by a newline.
 * 2.  Command length must be less than 80 bytes including the 
 * 	newline.
 * 3.  The entire command must be written with one system call.
 */

static const char debugger_help = 
"bc <addr> 			- clear breakpoint\n"
"bl				- list breakpoints\n"
"bs <addr>			- set breakpoint\n"
"g				- start\n" 				
"h				- halt\n"
"?				- this message\n"
"i				- info\n"
"mp <addr> <size> 		- print memory\n"
"ms <addr> <size> <value>	- store memory\n"
"rp <num> <size>		- print register\n"
"rs <num> <size> <value> 	- store register\n"
"s                              - single step\n"	
"tb				- begin trace \n"
"te				- end trace\n";

/*
 * Whenever we change a break point, we should probably 
 * set the NCR up so that it is in a single step mode.
 */

static int debugger_fn_bc (struct Scsi_Host *host, struct debugger_token *token,
    u32 args[]) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	instance->hostdata;
    struct NCR53c7x0_break *bp, **prev;
    unsigned long flags;
    save_flags(flags);
    cli();
    for (bp = (struct NCR53c7x0_break *) instance->breakpoints,
	    prev = (struct NCR53c7x0_break **) &instance->breakpoints;
	    bp; prev = (struct NCR53c7x0_break **) &(bp->next),
	    bp = (struct NCR53c7x0_break *) bp->next);

    if (!bp) {
	restore_flags(flags);
	return -EIO;
    }

    /* 
     * XXX - we need to insure that the processor is halted 
     * here in order to prevent a race condition.
     */
    
    memcpy ((void *) bp->addr, (void *) bp->old, sizeof(bp->old));
    if (prev)
	*prev = bp->next;

    restore_flags(flags);
    return 0;
}


static int 
debugger_fn_bl (struct Scsi_Host *host, struct debugger_token *token,
    u32 args[]) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    struct NCR53c7x0_break *bp;
    char buf[80];
    size_t len;
    unsigned long flags;
    /* 
     * XXX - we need to insure that the processor is halted 
     * here in order to prevent a race condition.  So, if the 
     * processor isn't halted, print an error message and continue.
     */

    sprintf (buf, "scsi%d : bp : warning : processor not halted\b",
	host->host_no);
    debugger_kernel_write (host, buf, strlen(buf));

    save_flags(flags);
    cli();
    for (bp = (struct NCR53c7x0_break *) host->breakpoints;
	    bp; bp = (struct NCR53c7x0_break *) bp->next) {
	    sprintf (buf, "scsi%d : bp : success : at %08x, replaces %08x %08x",
		bp->addr, bp->old[0], bp->old[1]);
	    len = strlen(buf);
	    if ((bp->old[0] & (DCMD_TYPE_MASK << 24)) ==
		(DCMD_TYPE_MMI << 24)) {
		sprintf(buf + len, "%08x\n", * (u32 *) bp->addr);
	    } else {
		sprintf(buf + len, "\n");
	    }
	    len = strlen(buf);
	    debugger_kernel_write (host, buf, len);
    }
    restore_flags(flags);
    return 0;
}

static int 
debugger_fn_bs (struct Scsi_Host *host, struct debugger_token *token,
    u32 args[]) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    struct NCR53c7x0_break *bp;
    char buf[80];
    size_t len;
    unsigned long flags;
    save_flags(flags);
    cli();

    if (hostdata->state != STATE_HALTED) {
	sprintf (buf, "scsi%d : bs : failure : NCR not halted\n", host->host_no);
	debugger_kernel_write (host, buf, strlen(buf));
	restore_flags(flags);
	return -1;
    }

    if (!(bp = kmalloc (sizeof (struct NCR53c7x0_break)))) {
	printk ("scsi%d : kmalloc(%d) of breakpoint structure failed, try again\n",
	    host->host_no, sizeof(struct NCR53c7x0_break));
	restore_flags(flags);
	return -1;
    }

    bp->address = (u32 *) args[0];
    memcpy ((void *) bp->old_instruction, (void *) bp->address, 8);
    bp->old_size = (((bp->old_instruction[0] >> 24) & DCMD_TYPE_MASK) ==
	DCMD_TYPE_MMI ? 3 : 2;
    bp->next = hostdata->breakpoints;
    hostdata->breakpoints = bp->next;
    memcpy ((void *) bp->address, (void *) hostdata->E_debug_break, 8);
    
    restore_flags(flags);
    return 0;
}

#define TOKEN(name,nargs) {#name, nargs, debugger_fn_##name}
static const struct debugger_token {
    char *name;
    int numargs;
    int (*fn)(struct debugger_token *token, u32 args[]);
} debugger_tokens[] = {
    TOKEN(bc,1), TOKEN(bl,0), TOKEN(bs,1), TOKEN(g,0), TOKEN(halt,0),
    {DT_help, "?", 0} , TOKEN(h,0), TOKEN(i,0), TOKEN(mp,2), 
    TOKEN(ms,3), TOKEN(rp,2), TOKEN(rs,2), TOKEN(s,0), TOKEN(tb,0), TOKEN(te,0)
};

#define NDT sizeof(debugger_tokens / sizeof(struct debugger_token))

static struct Scsi_Host * inode_to_host (struct inode *inode) {
    int dev;
    struct Scsi_Host *tmp;
    for (dev = MINOR(inode->rdev), host = first_host;
	(host->hostt == the_template); --dev, host = host->next)
	if (!dev) return host;
    return NULL;
}


static int
debugger_user_write (struct inode *inode,struct file *filp,
    char *buf,int count) {
    struct Scsi_Host *host;			/* This SCSI host */
    struct NCR53c7x0_hostadata *hostdata;	
    char input_buf[80], 			/* Kernel space copy of buf */
	*ptr;					/* Pointer to argument list */
    u32 args[3];				/* Arguments */
    int i, j, error, len;

    if (!(host = inode_to_host(inode)))
	return -ENXIO;

    hostdata = (struct NCR53c7x0_hostdata *) host->hostdata;

    if (error = verify_area(VERIFY_READ,buf,count))
	return error;

    if (count > 80) 
	return -EIO;

    memcpy_from_fs(input_buf, buf, count);

    if (input_buf[count - 1] != '\n')
	return -EIO;

    input_buf[count - 1]=0;

    for (i = 0; i < NDT; ++i) {
	len = strlen (debugger_tokens[i].name);
	if (!strncmp(input_buf, debugger_tokens[i].name, len)) 
	    break;
    };

    if (i == NDT) 
	return -EIO;

    for (ptr = input_buf + len, j = 0; j < debugger_tokens[i].nargs && *ptr;) {
	if (*ptr == ' ' || *ptr == '\t') {
	    ++ptr; 
	} else if (isdigit(*ptr)) {
	    args[j++] = simple_strtoul (ptr, &ptr, 0);
	} else {
	    return -EIO;
	} 
    }

    if (j != debugger_tokens[i].nargs)
	return -EIO;

    return count;
} 

static int 
debugger_user_read (struct inode *inode,struct file *filp,
    char *buf,int count) {
    struct Scsi_Host *instance;
    
}

static int 
debugger_kernel_write (struct Scsi_Host *host, char *buf, size_t
    buflen) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    int copy, left;
    unsigned long flags;
    save_flags(flags);
    cli();
    while (buflen) {
	left = (hostdata->debug_buf + hostdata->debug_size - 1) -
	    hostdata->debug_write;
	copy = (buflen <= left) ? buflen : left;
	memcpy (hostdata->debug_write, buf, copy);
	buf += copy;
	buflen -= copy;
	hostdata->debug_count += copy;
	if ((hostdata->debug_write += copy) == 
	    (hostdata->debug_buf + hostdata->debug_size))
	    hosdata->debug_write = hostdata->debug_buf;
    }
    restore_flags(flags);
}

#endif /* def NCRDEBUG */

/* 
 * Function : static void NCR538xx_soft_reset (struct Scsi_Host *host)
 *
 * Purpose :  perform a soft reset of the NCR53c8xx chip
 *
 * Inputs : host - pointer to this host adapter's structure
 *
 * Preconditions : NCR53c7x0_init must have been called for this 
 *      host.
 * 
 */

static void 
NCR53c8x0_soft_reset (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    NCR53c7x0_local_setup(host);


    /*
     * Do a soft reset of the chip so that everything is 
     * reinitialized to the power-on state.
     *
     * Basically follow the procedure outlined in the NCR53c700
     * data manual under Chapter Six, How to Use, Steps Necessary to
     * Start SCRIPTS, with the exception of actually starting the 
     * script and setting up the synchronous transfer gunk.
     */

    NCR53c7x0_write8(ISTAT_REG_800, ISTAT_10_SRST);
    NCR53c7x0_write8(ISTAT_REG_800, 0);
    NCR53c7x0_write8(hostdata->dmode, hostdata->saved_dmode & ~DMODE_MAN);


    /* 
     * Respond to reselection by targets and use our _initiator_ SCSI ID  
     * for arbitration. If notyet, also respond to SCSI selection.
     *
     * XXX - Note : we must reprogram this when reselecting as 
     *	a target.
     */

#ifdef notyet
    NCR53c7x0_write8(SCID_REG, (host->this_id & 7)|SCID_800_RRE|SCID_800_SRE);
#else
    NCR53c7x0_write8(SCID_REG, (host->this_id & 7)|SCID_800_RRE);
#endif
    NCR53c7x0_write8(RESPID_REG_800, hostdata->this_id_mask);

    /*
     * Use a maximum (1.6) second handshake to handshake timeout,
     * and SCSI recommended .5s selection timeout.
     */

    /*
     * The new gcc won't recognize preprocessing directives
     * within macro args.
     */
#if 0
    NCR53c7x0_write8(STIME0_REG_800, 
    	((selection_timeout << STIME0_800_SEL_SHIFT) & STIME0_800_SEL_MASK) 
	| ((15 << STIME0_800_HTH_SHIFT) & STIME0_800_HTH_MASK));
#else
/* Disable HTH interrupt */
    NCR53c7x0_write8(STIME0_REG_800, 
    	((selection_timeout << STIME0_800_SEL_SHIFT) & STIME0_800_SEL_MASK));
#endif


    /*
     * Enable active negation for happy synchronous transfers.
     */

    NCR53c7x0_write8(STEST3_REG_800, STEST3_800_TE);

    /*
     * Enable all interrupts, except parity which we only want when
     * the user requests it.
     */

    NCR53c7x0_write8(DIEN_REG, DIEN_800_MDPE | DIEN_800_BF |
		DIEN_ABRT | DIEN_SSI | DIEN_SIR | DIEN_800_IID);

    
    NCR53c7x0_write8(SIEN0_REG_800, ((hostdata->options & OPTION_PARITY) ?
	    SIEN_PAR : 0) | SIEN_RST | SIEN_UDC | SIEN_SGE | SIEN_MA);
    NCR53c7x0_write8(SIEN1_REG_800, SIEN1_800_STO | SIEN1_800_HTH);

    /* 
     * Use saved clock frequency divisor and scripts loaded in 16 bit
     * mode flags from the saved dcntl.
     */

    NCR53c7x0_write8(DCNTL_REG, hostdata->saved_dcntl);
    NCR53c7x0_write8(CTEST4_REG_800, hostdata->saved_ctest4);

    /* Enable active negation */
    NCR53c7x0_write8(STEST3_REG_800, STEST3_800_TE);
}

/*
 * Function static struct NCR53c7x0_cmd *allocate_cmd (Scsi_Cmnd *cmd)
 * 
 * Purpose : Return the first free NCR53c7x0_cmd structure (which are 
 * 	reused in a LIFO manner to minimize cache thrashing).
 *
 * Side effects : If we haven't yet scheduled allocation of NCR53c7x0_cmd
 *	structures for this device, do so.  Attempt to complete all scheduled
 *	allocations using kmalloc(), putting NCR53c7x0_cmd structures on 
 *	the free list.  Teach programmers not to drink and hack.
 *
 * Inputs : cmd - SCSI command
 *
 * Returns : NCR53c7x0_cmd structure allocated on behalf of cmd;
 *	NULL on failure.
 */

static struct NCR53c7x0_cmd *
allocate_cmd (Scsi_Cmnd *cmd) {
    struct Scsi_Host *host = cmd->host;
    struct NCR53c7x0_hostdata *hostdata = 
	(struct NCR53c7x0_hostdata *) host->hostdata;
    void *real;			/* Real address */
    int size;			/* Size of *tmp */
    struct NCR53c7x0_cmd *tmp;
    unsigned long flags;

    if (hostdata->options & OPTION_DEBUG_ALLOCATION)
	printk ("scsi%d : num_cmds = %d, can_queue = %d\n"
		"         target = %d, lun = %d, %s\n",
	    host->host_no, hostdata->num_cmds, host->can_queue,
	    cmd->target, cmd->lun, (hostdata->cmd_allocated[cmd->target] &
		(1 << cmd->lun)) ? "already allocated" : "not allocated");

/*
 * If we have not yet reserved commands for this I_T_L nexus, and
 * the device exists (as indicated by permanent Scsi_Cmnd structures
 * being allocated under 1.3.x, or being outside of scan_scsis in 
 * 1.2.x), do so now.
 */
    if (!(hostdata->cmd_allocated[cmd->target] & (1 << cmd->lun)) &&
				cmd->device && cmd->device->has_cmdblocks
	) {
	if ((hostdata->extra_allocate + hostdata->num_cmds) < host->can_queue) 
	    hostdata->extra_allocate += host->cmd_per_lun;
	hostdata->cmd_allocated[cmd->target] |= (1 << cmd->lun);
    }

    for (; hostdata->extra_allocate > 0 ; --hostdata->extra_allocate, 
    	++hostdata->num_cmds) {
    /* historically, kmalloc has returned unaligned addresses; pad so we 
       have enough room to ROUNDUP */
	size = hostdata->max_cmd_size + sizeof (void *);
/* FIXME: for ISA bus '7xx chips, we need to or GFP_DMA in here */
	real = kmalloc (size, GFP_ATOMIC);
	if (!real) {
	    if (hostdata->options & OPTION_DEBUG_ALLOCATION)
		printk ("scsi%d : kmalloc(%d) failed\n",
		    host->host_no, size);
	    break;
	}
	tmp = ROUNDUP(real, void *);
	tmp->real = real;
	tmp->size = size;			
	tmp->free = ((void (*)(void *, int)) kfree);
	save_flags (flags);
	cli();
	tmp->next = hostdata->free;
	hostdata->free = tmp;
	restore_flags (flags);
    }
    save_flags(flags);
    cli();
    tmp = (struct NCR53c7x0_cmd *) hostdata->free;
    if (tmp) {
	hostdata->free = tmp->next;
    }
    restore_flags(flags);
    if (!tmp)
	printk ("scsi%d : can't allocate command for target %d lun %d\n",
	    host->host_no, cmd->target, cmd->lun);
    return tmp;
}

/*
 * Function static struct NCR53c7x0_cmd *create_cmd (Scsi_Cmnd *cmd) 
 *
 *
 * Purpose : allocate a NCR53c7x0_cmd structure, initialize it based on the 
 * 	Scsi_Cmnd structure passed in cmd, including dsa and Linux field 
 * 	initialization, and dsa code relocation.
 *
 * Inputs : cmd - SCSI command
 *
 * Returns : NCR53c7x0_cmd structure corresponding to cmd,
 *	NULL on failure.
 */

static struct NCR53c7x0_cmd *
create_cmd (Scsi_Cmnd *cmd) {
    NCR53c7x0_local_declare();
    struct Scsi_Host *host = cmd->host;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
        host->hostdata;	
    struct NCR53c7x0_cmd *tmp; 	/* NCR53c7x0_cmd structure for this command */
    int datain,  		/* Number of instructions per phase */
	dataout;
    int data_transfer_instructions, /* Count of dynamic instructions */
    	i;			/* Counter */
    u32 *cmd_datain,		/* Address of datain/dataout code */
	*cmd_dataout;		/* Incremented as we assemble */
#ifdef notyet
    unsigned char *msgptr;	/* Current byte in select message */
    int msglen;			/* Length of whole select message */
#endif
    unsigned long flags;
    NCR53c7x0_local_setup(cmd->host);

    if (!(tmp = allocate_cmd (cmd)))
	return NULL;


    /*
     * Decide whether we need to generate commands for DATA IN,
     * DATA OUT, neither, or both based on the SCSI command 
     */

    switch (cmd->cmnd[0]) {
    /* These commands do DATA IN */
    case INQUIRY:
    case MODE_SENSE:
    case READ_6:
    case READ_10:
    case READ_CAPACITY:
    case REQUEST_SENSE:
	datain = 2 * (cmd->use_sg ? cmd->use_sg : 1) + 3;
    	dataout = 0;
	break;
    /* These commands do DATA OUT */
    case MODE_SELECT: 
    case WRITE_6:
    case WRITE_10:
#if 0
	printk("scsi%d : command is ", host->host_no);
	print_command(cmd->cmnd);
#endif
#if 0
	printk ("scsi%d : %d scatter/gather segments\n", host->host_no,
	    cmd->use_sg);
#endif
    	datain = 0;
	dataout = 2 * (cmd->use_sg ? cmd->use_sg : 1) + 3;
#if 0
	hostdata->options |= OPTION_DEBUG_INTR;
#endif
	break;
    /* 
     * These commands do no data transfer, we should force an
     * interrupt if a data phase is attempted on them.
     */
    case START_STOP: /* also SCAN, which may do DATA OUT */
    case TEST_UNIT_READY:
    	datain = dataout = 0;
	break;
    /*
     * We don't know about these commands, so generate code to handle
     * both DATA IN and DATA OUT phases.
     */
    default:
	datain = dataout = 2 * (cmd->use_sg ? cmd->use_sg : 1) + 3;
    }

    /*
     * New code : so that active pointers work correctly regardless
     * 	of where the saved data pointer is at, we want to immediately
     * 	enter the dynamic code after selection, and on a non-data
     * 	phase perform a CALL to the non-data phase handler, with
     * 	returns back to this address.
     *
     * 	If a phase mismatch is encountered in the middle of a 
     * 	Block MOVE instruction, we want to _leave_ that instruction
     *	unchanged as the current case is, modify a temporary buffer,
     *	and point the active pointer (TEMP) at that.
     *
     * 	Furthermore, we want to implement a saved data pointer, 
     * 	set by the SAVE_DATA_POINTERs message.
     *
     * 	So, the data transfer segments will change to 
     *		CALL data_transfer, WHEN NOT data phase
     *		MOVE x, x, WHEN data phase
     *		( repeat )
     *		JUMP other_transfer
     */

    data_transfer_instructions = datain + dataout;

    /*
     * When we perform a request sense, we overwrite various things,
     * including the data transfer code.  Make sure we have enough
     * space to do that.
     */

    if (data_transfer_instructions < 2)
    	data_transfer_instructions = 2;


    /*
     * The saved data pointer is set up so that a RESTORE POINTERS message 
     * will start the data transfer over at the beginning.
     */

    tmp->saved_data_pointer = le32_to_cpu(virt_to_bus (hostdata->script) + 
	hostdata->E_data_transfer);

    /*
     * Initialize Linux specific fields.
     */

    tmp->cmd = cmd;
    tmp->next = NULL;
    tmp->flags = 0;
    tmp->dsa_next_addr = le32_to_cpu(virt_to_bus(tmp->dsa) + hostdata->dsa_next - 
	hostdata->dsa_start);
    tmp->dsa_addr = le32_to_cpu(virt_to_bus(tmp->dsa) - hostdata->dsa_start);

    /* 
     * Calculate addresses of dynamic code to fill in DSA
     */

    tmp->data_transfer_start = tmp->dsa + (hostdata->dsa_end - 
    	hostdata->dsa_start) / sizeof(u32);
    tmp->data_transfer_end = tmp->data_transfer_start + 
    	2 * data_transfer_instructions;

    cmd_datain = datain ? tmp->data_transfer_start : NULL;
    cmd_dataout = dataout ? (datain ? cmd_datain + 2 * datain : tmp->
    	data_transfer_start) : NULL;

    /*
     * Fill in the NCR53c7x0_cmd structure as follows
     * dsa, with fixed up DSA code
     * datain code
     * dataout code
     */

    /* Copy template code into dsa and perform all necessary fixups */
    if (hostdata->dsa_fixup)
    	hostdata->dsa_fixup(tmp);

    patch_dsa_32(tmp->dsa, dsa_next, 0, le32_to_cpu(0));
    patch_dsa_32(tmp->dsa, dsa_cmnd, 0, le32_to_cpu(virt_to_bus(cmd)));

    if (hostdata->options & OPTION_DEBUG_SYNCHRONOUS) 
	if (hostdata->sync[cmd->target].select_indirect != 
	    ((hostdata->sync[cmd->target].scntl3_sanity << 24) | 
		(cmd->target << 16) | 
		(hostdata->sync[cmd->target].sxfer_sanity << 8))) {
	    printk ("scsi%d :  sanity check failed select_indirect=0x%x\n",
		host->host_no, hostdata->sync[cmd->target].select_indirect);
	    FATAL(host);

	}

    patch_dsa_32(tmp->dsa, dsa_select, 0, le32_to_cpu(hostdata->sync[cmd->target].
    	select_indirect));
    /*
     * Right now, we'll do the WIDE and SYNCHRONOUS negotiations on
     * different commands; although it should be trivial to do them
     * both at the same time.
     */
    if (hostdata->initiate_wdtr & (1 << cmd->target)) {
	memcpy ((void *) (tmp->select + 1), (void *) wdtr_message,
	    sizeof(wdtr_message));
    	patch_dsa_32(tmp->dsa, dsa_msgout, 0, le32_to_cpu(1 + sizeof(wdtr_message)));
	save_flags(flags);
	cli();
	hostdata->initiate_wdtr &= ~(1 << cmd->target);
	restore_flags(flags);
    } else if (hostdata->initiate_sdtr & (1 << cmd->target)) {
	memcpy ((void *) (tmp->select + 1), (void *) sdtr_message, 
	    sizeof(sdtr_message));
    	patch_dsa_32(tmp->dsa, dsa_msgout, 0, le32_to_cpu(1 + sizeof(sdtr_message)));
	tmp->flags |= CMD_FLAG_SDTR;
	save_flags(flags);
	cli();
	hostdata->initiate_sdtr &= ~(1 << cmd->target);
	restore_flags(flags);
    
    }
#if 1
    else if (!(hostdata->talked_to & (1 << cmd->target)) && 
		!(hostdata->options & OPTION_NO_ASYNC)) {
	memcpy ((void *) (tmp->select + 1), (void *) async_message, 
	    sizeof(async_message));
    	patch_dsa_32(tmp->dsa, dsa_msgout, 0, le32_to_cpu(1 + sizeof(async_message)));
	tmp->flags |= CMD_FLAG_SDTR;
    } 
#endif
    else 
    	patch_dsa_32(tmp->dsa, dsa_msgout, 0, le32_to_cpu(1));
    hostdata->talked_to |= (1 << cmd->target);
    tmp->select[0] = (hostdata->options & OPTION_DISCONNECT) ? 
	IDENTIFY (1, cmd->lun) : IDENTIFY (0, cmd->lun);
    patch_dsa_32(tmp->dsa, dsa_msgout, 1, le32_to_cpu(virt_to_bus(tmp->select)));
    patch_dsa_32(tmp->dsa, dsa_cmdout, 0, le32_to_cpu(cmd->cmd_len));
    patch_dsa_32(tmp->dsa, dsa_cmdout, 1, le32_to_cpu(virt_to_bus(cmd->cmnd)));
    patch_dsa_32(tmp->dsa, dsa_dataout, 0, le32_to_cpu(cmd_dataout ? 
    	    virt_to_bus (cmd_dataout)
	: virt_to_bus (hostdata->script) + hostdata->E_other_transfer));
    patch_dsa_32(tmp->dsa, dsa_datain, 0, le32_to_cpu(cmd_datain ? 
    	    virt_to_bus (cmd_datain) 
	: virt_to_bus (hostdata->script) + hostdata->E_other_transfer));
    /* 
     * XXX - need to make endian aware, should use separate variables
     * for both status and message bytes.
     */
    patch_dsa_32(tmp->dsa, dsa_msgin, 0, le32_to_cpu(1));
/* 
 * FIXME : these only works for little endian.  We probably want to 
 * 	provide message and status fields in the NCR53c7x0_cmd 
 *	structure, and assign them to cmd->result when we're done.
 */
    patch_dsa_32(tmp->dsa, dsa_msgin, 1, le32_to_cpu(virt_to_bus(&cmd->result) + 1));
    patch_dsa_32(tmp->dsa, dsa_status, 0, le32_to_cpu(1));
    patch_dsa_32(tmp->dsa, dsa_status, 1, le32_to_cpu(virt_to_bus(&cmd->result)));
    patch_dsa_32(tmp->dsa, dsa_msgout_other, 0, le32_to_cpu(1));
    patch_dsa_32(tmp->dsa, dsa_msgout_other, 1, 
	le32_to_cpu(virt_to_bus(&(hostdata->NCR53c7xx_msg_nop))));
    
    /*
     * Generate code for zero or more of the DATA IN, DATA OUT phases 
     * in the format 
     *
     * CALL data_transfer, WHEN NOT phase
     * MOVE first buffer length, first buffer address, WHEN phase
     * ...
     * MOVE last buffer length, last buffer address, WHEN phase
     * JUMP other_transfer
     */

/* 
 * See if we're getting to data transfer by generating an unconditional 
 * interrupt.
 */
#if 0
    if (datain) {
	cmd_datain[0] = 0x98080000;
	cmd_datain[1] = 0x03ffd00d;
	cmd_datain += 2;
    }
#endif

/* 
 * XXX - I'm undecided whether all of this nonsense is faster
 * in the long run, or whether I should just go and implement a loop
 * on the NCR chip using table indirect mode?
 *
 * In any case, this is how it _must_ be done for 53c700/700-66 chips,
 * so this stays even when we come up with something better.
 *
 * When we're limited to 1 simultaneous command, no overlapping processing,
 * we're seeing 630K/sec, with 7% CPU usage on a slow Syquest 45M
 * drive.
 *
 * Not bad, not good. We'll see.
 */

    for (i = 0; cmd->use_sg ? (i < cmd->use_sg) : !i; cmd_datain += 4, 
	cmd_dataout += 4, ++i) {
	u32 buf = cmd->use_sg ? 
	    virt_to_bus(((struct scatterlist *)cmd->buffer)[i].address) :
	    virt_to_bus(cmd->request_buffer);
	u32 count = cmd->use_sg ?
	    ((struct scatterlist *)cmd->buffer)[i].length :
	    cmd->request_bufflen;

	if (datain) {
	    /* CALL other_in, WHEN NOT DATA_IN */  
	    cmd_datain[0] = le32_to_cpu(((DCMD_TYPE_TCI | DCMD_TCI_OP_CALL | 
		DCMD_TCI_IO) << 24) | 
		DBC_TCI_WAIT_FOR_VALID | DBC_TCI_COMPARE_PHASE);
	    cmd_datain[1] = le32_to_cpu(virt_to_bus (hostdata->script) + 
		hostdata->E_other_in);
	    /* MOVE count, buf, WHEN DATA_IN */
	    cmd_datain[2] = le32_to_cpu(((DCMD_TYPE_BMI | DCMD_BMI_OP_MOVE_I | DCMD_BMI_IO) 
    	    	<< 24) | count);
	    cmd_datain[3] = le32_to_cpu(buf);
#if 0
	    print_insn (host, cmd_datain, "dynamic ", 1);
	    print_insn (host, cmd_datain + 2, "dynamic ", 1);
#endif
	}
	if (dataout) {
	    /* CALL other_out, WHEN NOT DATA_OUT */
	    cmd_dataout[0] = le32_to_cpu(((DCMD_TYPE_TCI | DCMD_TCI_OP_CALL) << 24) | 
		DBC_TCI_WAIT_FOR_VALID | DBC_TCI_COMPARE_PHASE);
	    cmd_dataout[1] = le32_to_cpu(virt_to_bus(hostdata->script) + 
    	    	hostdata->E_other_out);
	    /* MOVE count, buf, WHEN DATA+OUT */
	    cmd_dataout[2] = le32_to_cpu(((DCMD_TYPE_BMI | DCMD_BMI_OP_MOVE_I) << 24) 
		| count);
	    cmd_dataout[3] = le32_to_cpu(buf);
#if 0
	    print_insn (host, cmd_dataout, "dynamic ", 1);
	    print_insn (host, cmd_dataout + 2, "dynamic ", 1);
#endif
	}
    }

    /*
     * Install JUMP instructions after the data transfer routines to return
     * control to the do_other_transfer routines.
     */
  
    
    if (datain) {
	cmd_datain[0] = le32_to_cpu(((DCMD_TYPE_TCI | DCMD_TCI_OP_JUMP) << 24) |
    	    DBC_TCI_TRUE);
	cmd_datain[1] = le32_to_cpu(virt_to_bus(hostdata->script) + 
    	    hostdata->E_other_transfer);
#if 0
	print_insn (host, cmd_datain, "dynamic jump ", 1);
#endif
	cmd_datain += 2; 
    }
#if 0
    if (datain) {
	cmd_datain[0] = 0x98080000;
	cmd_datain[1] = 0x03ffdeed;
	cmd_datain += 2;
    }
#endif
    if (dataout) {
	cmd_dataout[0] = le32_to_cpu(((DCMD_TYPE_TCI | DCMD_TCI_OP_JUMP) << 24) |
    	    DBC_TCI_TRUE);
	cmd_dataout[1] = le32_to_cpu(virt_to_bus(hostdata->script) + 
    	    hostdata->E_other_transfer);
#if 0
	print_insn (host, cmd_dataout, "dynamic jump ", 1);
#endif
	cmd_dataout += 2;
    }
    return tmp;
}

/*
 * Function : int NCR53c7xx_queue_command (Scsi_Cmnd *cmd,
 *      void (*done)(Scsi_Cmnd *))
 *
 * Purpose :  enqueues a SCSI command
 *
 * Inputs : cmd - SCSI command, done - function called on completion, with
 *      a pointer to the command descriptor.
 *
 * Returns : 0
 *
 * Side effects :
 *      cmd is added to the per instance driver issue_queue, with major
 *      twiddling done to the host specific fields of cmd.  If the
 *      process_issue_queue coroutine isn't running, it is restarted.
 * 
 * NOTE : we use the host_scribble field of the Scsi_Cmnd structure to 
 *	hold our own data, and pervert the ptr field of the SCp field
 *	to create a linked list.
 */

int
NCR53c7xx_queue_command (Scsi_Cmnd *cmd, void (* done)(Scsi_Cmnd *)) {
    struct Scsi_Host *host = cmd->host;
    struct NCR53c7x0_hostdata *hostdata = 
	(struct NCR53c7x0_hostdata *) host->hostdata;
    unsigned long flags;
    Scsi_Cmnd *tmp;

    cmd->scsi_done = done;
    cmd->host_scribble = NULL;
    cmd->SCp.ptr = NULL;
    cmd->SCp.buffer = NULL;

    save_flags(flags);
    cli();
    if ((hostdata->options & (OPTION_DEBUG_INIT_ONLY|OPTION_DEBUG_PROBE_ONLY)) 
	|| ((hostdata->options & OPTION_DEBUG_TARGET_LIMIT) &&
	    !(hostdata->debug_lun_limit[cmd->target] & (1 << cmd->lun))) 
	|| cmd->target > host->max_id
	|| cmd->target == host->this_id
	|| hostdata->state == STATE_DISABLED) {
	printk("scsi%d : disabled or bad target %d lun %d\n", host->host_no,
	    cmd->target, cmd->lun);
	cmd->result = DID_BAD_TARGET << 16;
    } else if ((hostdata->options & OPTION_DEBUG_NCOMMANDS_LIMIT) &&
	(hostdata->debug_count_limit == 0)) {
	printk("scsi%d : maximum commands exceeded\n", host->host_no);
	cmd->result = DID_BAD_TARGET << 16;
    } else if (hostdata->options & OPTION_DEBUG_READ_ONLY) {
	switch (cmd->cmnd[0]) {
	case WRITE_6:
	case WRITE_10:
	    printk("scsi%d : WRITE attempted with NO_WRITE debugging flag set\n",
		host->host_no);
	    cmd->result = DID_BAD_TARGET << 16;
	}
    } else {
    	if ((hostdata->options & OPTION_DEBUG_TARGET_LIMIT) &&
	    hostdata->debug_count_limit != -1) 
	    --hostdata->debug_count_limit;
	restore_flags (flags);
	cmd->result = le32_to_cpu(0xffff);	/* The NCR will overwrite message
				       and status with valid data */
	cmd->host_scribble = (unsigned char *) tmp = create_cmd (cmd);
    }
    cli();
    /*
     * REQUEST SENSE commands are inserted at the head of the queue 
     * so that we do not clear the contingent allegiance condition
     * they may be looking at.
     */

    if (!(hostdata->issue_queue) || (cmd->cmnd[0] == REQUEST_SENSE)) {
	cmd->SCp.ptr = (unsigned char *) hostdata->issue_queue;
	hostdata->issue_queue = cmd;
    } else {
	for (tmp = (Scsi_Cmnd *) hostdata->issue_queue; tmp->SCp.ptr; 
		tmp = (Scsi_Cmnd *) tmp->SCp.ptr);
	tmp->SCp.ptr = (unsigned char *) cmd;
    }
    restore_flags (flags);
    run_process_issue_queue();
    return 0;
}

/*
 * Function : void to_schedule_list (struct Scsi_Host *host,
 * 	struct NCR53c7x0_hostdata * hostdata, Scsi_Cmnd *cmd)
 *
 * Purpose : takes a SCSI command which was just removed from the 
 *	issue queue, and deals with it by inserting it in the first
 *	free slot in the schedule list or by terminating it immediately.
 *
 * Inputs : 
 *	host - SCSI host adapter; hostdata - hostdata structure for 
 *	this adapter; cmd - a pointer to the command; should have 
 *	the host_scribble field initialized to point to a valid 
 *	
 * Side effects : 
 *      cmd is added to the per instance schedule list, with minor 
 *      twiddling done to the host specific fields of cmd.
 *
 */

static __inline__ void
to_schedule_list (struct Scsi_Host *host, struct NCR53c7x0_hostdata *hostdata,
    struct NCR53c7x0_cmd *cmd) {
    NCR53c7x0_local_declare();
    Scsi_Cmnd *tmp = cmd->cmd;
    unsigned long flags;
    /* dsa start is negative, so subtraction is used */
    volatile u32 *curr;

    int i;
    NCR53c7x0_local_setup(host);
#if 0
    printk("scsi%d : new dsa is 0x%lx (virt 0x%p)\n", host->host_no, 
	virt_to_bus(dsa), dsa);
#endif

    save_flags(flags);
    cli();
    
    /* 
     * Work around race condition : if an interrupt fired and we 
     * got disabled forget about this command.
     */

    if (hostdata->state == STATE_DISABLED) {
	printk("scsi%d : driver disabled\n", host->host_no);
	tmp->result = DID_BAD_TARGET << 16;
	cmd->next = (struct NCR53c7x0_cmd *) hostdata->free;
	hostdata->free = cmd;
	tmp->scsi_done(tmp);
	restore_flags (flags);
	return;
    }

    for (i = host->can_queue, curr = hostdata->schedule; 
	i > 0  && curr[0] != hostdata->NOP_insn;
	--i, curr += 2 /* JUMP instructions are two words */);

    if (i > 0) {
	++hostdata->busy[tmp->target][tmp->lun];
	cmd->next = hostdata->running_list;
	hostdata->running_list = cmd;

	/* Restore this instruction to a NOP once the command starts */
	cmd->dsa [(hostdata->dsa_jump_dest - hostdata->dsa_start) / 
	    sizeof(u32)] = (u32) le32_to_cpu(virt_to_bus ((void *)curr));
	/* Replace the current jump operand.  */
	curr[1] =
	    le32_to_cpu(virt_to_bus ((void *) cmd->dsa) + hostdata->E_dsa_code_begin -
	    hostdata->E_dsa_code_template);
	/* Replace the NOP instruction with a JUMP */
	curr[0] = le32_to_cpu(((DCMD_TYPE_TCI|DCMD_TCI_OP_JUMP) << 24) |
	    DBC_TCI_TRUE);
    }  else {
	printk ("scsi%d: no free slot\n", host->host_no);
	disable(host);
	tmp->result = DID_ERROR << 16;
	cmd->next = (struct NCR53c7x0_cmd *) hostdata->free;
	hostdata->free = cmd;
	tmp->scsi_done(tmp);
	restore_flags (flags);
	return;
    }

    /* 
     * If the NCR chip is in an idle state, start it running the scheduler
     * immediately.  Otherwise, signal the chip to jump to schedule as 
     * soon as it is idle.
     */
    if (hostdata->idle) {
	hostdata->idle = 0;
	hostdata->state = STATE_RUNNING;
	NCR53c7x0_write32 (DSP_REG,  virt_to_bus ((void *)hostdata->schedule));
    } else {
	NCR53c7x0_write8(hostdata->istat, ISTAT_10_SIGP);
    }

    restore_flags(flags);
}

/*
 * Function : busyp (struct Scsi_Host *host, struct NCR53c7x0_hostdata 
 *	*hostdata, Scsi_Cmnd *cmd)
 *
 * Purpose : decide if we can pass the given SCSI command on to the 
 *	device in question or not.
 *  
 * Returns : non-zero when we're busy, 0 when we aren't.
 */

static __inline__ int
busyp (struct Scsi_Host *host, struct NCR53c7x0_hostdata *hostdata, 
    Scsi_Cmnd *cmd) {
    /* FIXME : in the future, this needs to accommodate SCSI-II tagged
       queuing, and we may be able to play with fairness here a bit.
     */
    return hostdata->busy[cmd->target][cmd->lun];
}

/*
 * Function : process_issue_queue (void)
 *
 * Purpose : transfer commands from the issue queue to NCR start queue 
 *	of each NCR53c7/8xx in the system, avoiding kernel stack 
 *	overflows when the scsi_done() function is invoked recursively.
 * 
 * NOTE : process_issue_queue exits with interrupts *disabled*, so the 
 *	caller must reenable them if it desires.
 * 
 * NOTE : process_issue_queue should be called from both 
 *	NCR53c7x0_queue_command() and from the interrupt handler 
 *	after command completion in case NCR53c7x0_queue_command()
 * 	isn't invoked again but we've freed up resources that are
 *	needed.
 */

static void 
process_issue_queue (unsigned long flags) {
    Scsi_Cmnd *tmp, *prev;
    struct Scsi_Host *host;
    struct NCR53c7x0_hostdata *hostdata;
    int done;

    /*
     * We run (with interrupts disabled) until we're sure that none of 
     * the host adapters have anything that can be done, at which point 
     * we set process_issue_queue_running to 0 and exit.
     *
     * Interrupts are enabled before doing various other internal 
     * instructions, after we've decided that we need to run through
     * the loop again.
     *
     */

    do {
	cli(); /* Freeze request queues */
	done = 1;
	for (host = first_host; host && host->hostt == the_template; 
	    host = host->next) {
	    hostdata = (struct NCR53c7x0_hostdata *) host->hostdata;
	    cli();
	    if (hostdata->issue_queue) {
	    	if (hostdata->state == STATE_DISABLED) {
		    tmp = (Scsi_Cmnd *) hostdata->issue_queue;
		    hostdata->issue_queue = (Scsi_Cmnd *) tmp->SCp.ptr;
		    tmp->result = DID_BAD_TARGET << 16;
		    if (tmp->host_scribble) {
			((struct NCR53c7x0_cmd *)tmp->host_scribble)->next = 
			    hostdata->free;
			hostdata->free = 
			    (struct NCR53c7x0_cmd *)tmp->host_scribble;
			tmp->host_scribble = NULL;
		    }
		    tmp->scsi_done (tmp);
		    done = 0;
		} else 
		    for (tmp = (Scsi_Cmnd *) hostdata->issue_queue, 
			prev = NULL; tmp; prev = tmp, tmp = (Scsi_Cmnd *) 
			tmp->SCp.ptr) 
			if (!tmp->host_scribble || 
			    !busyp (host, hostdata, tmp)) {
				if (prev)
				    prev->SCp.ptr = tmp->SCp.ptr;
				else
				    hostdata->issue_queue = (Scsi_Cmnd *) 
					tmp->SCp.ptr;
			    tmp->SCp.ptr = NULL;
			    if (tmp->host_scribble) {
				if (hostdata->options & OPTION_DEBUG_QUEUES) 
				    printk ("scsi%d : moving command for target %d lun %d to start list\n",
					host->host_no, tmp->target, tmp->lun);
		

			    	to_schedule_list (host, hostdata, 
				    (struct NCR53c7x0_cmd *)
				    tmp->host_scribble);
			    } else {
			    	tmp->result = le32_to_cpu(tmp->result);
				if (((tmp->result & 0xff) == 0xff) ||
			    	    ((tmp->result & 0xff00) == 0xff00)) {
				    printk ("scsi%d : danger Will Robinson!\n",
					host->host_no);
				    tmp->result = DID_ERROR << 16;
				    disable (host);
				}
				tmp->scsi_done(tmp);
			    }
			    done = 0;
			} /* if target/lun is not busy */
	    } /* if hostdata->issue_queue */
	    if (!done)
		restore_flags (flags);
    	} /* for host */
    } while (!done);
    process_issue_queue_running = 0;
}

/*
 * Function : static void intr_scsi (struct Scsi_Host *host, 
 * 	struct NCR53c7x0_cmd *cmd)
 *
 * Purpose : handle all SCSI interrupts, indicated by the setting 
 * 	of the SIP bit in the ISTAT register.
 *
 * Inputs : host, cmd - host and NCR command causing the interrupt, cmd
 * 	may be NULL.
 */

static void 
intr_scsi (struct Scsi_Host *host, struct NCR53c7x0_cmd *cmd) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = 
    	(struct NCR53c7x0_hostdata *) host->hostdata;
    unsigned char sstat0_sist0, sist1, 		/* Registers */
	    fatal; 				/* Did a fatal interrupt 
						   occur ? */
   
    int is_8xx_chip;		 
    NCR53c7x0_local_setup(host);

    fatal = 0;

    is_8xx_chip = ((unsigned) (hostdata->chip - 800)) < 100;
    if (is_8xx_chip) {
    	sstat0_sist0 = NCR53c7x0_read8(SIST0_REG_800);
	udelay(1);
    	sist1 = NCR53c7x0_read8(SIST1_REG_800);
    } else {
    	sstat0_sist0 = NCR53c7x0_read8(SSTAT0_REG);
    	sist1 = 0;
    }

    if (hostdata->options & OPTION_DEBUG_INTR) 
	printk ("scsi%d : SIST0 0x%0x, SIST1 0x%0x\n", host->host_no,
	    sstat0_sist0, sist1);

    /* 250ms selection timeout */
    if ((is_8xx_chip && (sist1 & SIST1_800_STO)) || 
        (!is_8xx_chip && (sstat0_sist0 & SSTAT0_700_STO))) {
	fatal = 1;
	if (hostdata->options & OPTION_DEBUG_INTR) {
	    printk ("scsi%d : Selection Timeout\n", host->host_no);
    	    if (cmd) {
    	    	printk("scsi%d : target %d, lun %d, command ",
    	    	    host->host_no, cmd->cmd->target, cmd->cmd->lun);
    	    	print_command (cmd->cmd->cmnd);
		printk("scsi%d : dsp = 0x%x (virt 0x%p)\n", host->host_no,
		    NCR53c7x0_read32(DSP_REG),
		    bus_to_virt(NCR53c7x0_read32(DSP_REG)));
    	    } else {
    	    	printk("scsi%d : no command\n", host->host_no);
    	    }
    	}
/*
 * XXX - question : how do we want to handle the Illegal Instruction
 * 	interrupt, which may occur before or after the Selection Timeout
 * 	interrupt?
 */

	if (1) {
	    hostdata->idle = 1;
	    hostdata->expecting_sto = 0;

	    if (hostdata->test_running) {
		hostdata->test_running = 0;
		hostdata->test_completed = 3;
	    } else if (cmd) {
		abnormal_finished(cmd, DID_BAD_TARGET << 16);
	    }
#if 0	    
	    hostdata->intrs = 0;
#endif
	}
    } 

/*
 * FIXME : in theory, we can also get a UDC when a STO occurs.
 */
    if (sstat0_sist0 & SSTAT0_UDC) {
	fatal = 1;
	if (cmd) {
	    printk("scsi%d : target %d lun %d unexpected disconnect\n",
		host->host_no, cmd->cmd->target, cmd->cmd->lun);
	    print_lots (host);
	    abnormal_finished(cmd, DID_ERROR << 16);
	} else 
	     printk("scsi%d : unexpected disconnect (no command)\n",
		host->host_no);

	hostdata->dsp = (u32 *) hostdata->schedule;
	hostdata->dsp_changed = 1;
    }

    /* SCSI PARITY error */
    if (sstat0_sist0 & SSTAT0_PAR) {
	fatal = 1;
	if (cmd && cmd->cmd) {
	    printk("scsi%d : target %d lun %d parity error.\n",
		host->host_no, cmd->cmd->target, cmd->cmd->lun);
	    abnormal_finished (cmd, DID_PARITY << 16); 
	} else
	    printk("scsi%d : parity error\n", host->host_no);
	/* Should send message out, parity error */

	/* XXX - Reduce synchronous transfer rate! */
	hostdata->dsp = hostdata->script + hostdata->E_initiator_abort /
    	    sizeof(u32);
	hostdata->dsp_changed = 1; 
    /* SCSI GROSS error */
    } 

    if (sstat0_sist0 & SSTAT0_SGE) {
	fatal = 1;
	printk("scsi%d : gross error\n", host->host_no);
	/* Reset SCSI offset */
	if ((hostdata->chip / 100) == 8) {
	    NCR53c7x0_write8 (STEST2_REG_800, STEST2_800_ROF);
	}
	
	/* 
         * A SCSI gross error may occur when we have 
	 *
	 * - A synchronous offset which causes the SCSI FIFO to be overwritten.
	 *
	 * - A REQ which causes the maximum synchronous offset programmed in 
	 * 	the SXFER register to be exceeded.
	 *
	 * - A phase change with an outstanding synchronous offset.
	 *
	 * - Residual data in the synchronous data FIFO, with a transfer
	 *	other than a synchronous receive is started.$#
	 */
		

	/* XXX Should deduce synchronous transfer rate! */
	hostdata->dsp = hostdata->script + hostdata->E_initiator_abort /
    	    sizeof(u32);
	hostdata->dsp_changed = 1;
    /* Phase mismatch */
    } 

    if (sstat0_sist0 & SSTAT0_MA) {
	fatal = 1;
	if (hostdata->options & OPTION_DEBUG_INTR)
	    printk ("scsi%d : SSTAT0_MA\n", host->host_no);
	intr_phase_mismatch (host, cmd);
    }

#if 0
    if (sstat0_sist0 & SIST0_800_RSL) 
	printk ("scsi%d : Oh no Mr. Bill!\n", host->host_no);
#endif
    
/*
 * If a fatal SCSI interrupt occurs, we must insure that the DMA and
 * SCSI FIFOs were flushed.
 */

    if (fatal) {
	if (!hostdata->dstat_valid) {
	    hostdata->dstat = NCR53c7x0_read8(DSTAT_REG);
	    hostdata->dstat_valid = 1;
	}

/* XXX - code check for 700/800 chips */
	if (!(hostdata->dstat & DSTAT_DFE)) {
	    printk ("scsi%d : DMA FIFO not empty\n", host->host_no);
    	    if (NCR53c7x0_read8 (CTEST2_REG_800) & CTEST2_800_DDIR) {
		printk ("scsi%d: Flushing DMA FIFO\n", 
			host->host_no);
    	    	NCR53c7x0_write8 (CTEST3_REG_800, CTEST3_800_FLF);
    	    	while (!((hostdata->dstat = NCR53c7x0_read8(DSTAT_REG)) &
    	    	    DSTAT_DFE));
    	    } else {
    	    	NCR53c7x0_write8 (CTEST3_REG_800, CTEST3_800_CLF);
    	    	while (NCR53c7x0_read8 (CTEST3_REG_800) & CTEST3_800_CLF);
    	    }
	    hostdata->dstat |= DSTAT_DFE;
    	}
    }
}

/*
 * Function : do_NCR53c7x0_intr()
 *
 * Purpose : A quick wrapper function added to grab the io_request_lock
 *      spin lock prior to entering the real interrupt handler.  Needed
 *      for 2.1.95 and above.
 */
static void
do_NCR53c7x0_intr(int irq, void *dev_id, struct pt_regs * regs) {
    unsigned long flags;

    spin_lock_irqsave(&io_request_lock, flags);
    NCR53c7x0_intr(irq, dev_id, regs);
    spin_unlock_irqrestore(&io_request_lock, flags);
}

/*
 * Function : static void NCR53c7x0_intr (int irq, void *dev_id, struct pt_regs * regs)
 *
 * Purpose : handle NCR53c7x0 interrupts for all NCR devices sharing
 *	the same IRQ line.  
 * 
 * Inputs : Since we're using the SA_INTERRUPT interrupt handler
 *	semantics, irq indicates the interrupt which invoked 
 *	this handler.  
 */

static void 
NCR53c7x0_intr (int irq, void *dev_id, struct pt_regs * regs) {
    NCR53c7x0_local_declare();
    struct Scsi_Host *host;			/* Host we are looking at */
    unsigned char istat; 			/* Values of interrupt regs */
    struct NCR53c7x0_hostdata *hostdata;	/* host->hostdata */
    struct NCR53c7x0_cmd *cmd,			/* command which halted */
	**cmd_prev_ptr;
    u32 *dsa;					/* DSA */
    int done = 1;				/* Indicates when handler 
						   should terminate */
    int interrupted = 0;			/* This HA generated 
						   an interrupt */
    int have_intfly;				/* Don't print warning 
						   messages when we stack
						   INTFLYs */
    unsigned long flags;				

#ifdef NCR_DEBUG
    char buf[80];				/* Debugging sprintf buffer */
    size_t buflen;				/* Length of same */
#endif
    do {
	done = 1;
	for (host = first_host; host; host = host->next) 
	    if (host->hostt == the_template && host->irq == irq) {
    	    NCR53c7x0_local_setup(host);

	    hostdata = (struct NCR53c7x0_hostdata *) host->hostdata;
	    hostdata->dsp_changed = 0;
	    interrupted = 0;
	    have_intfly = 0;

	    do {
		int is_8xx_chip;

		hostdata->dstat_valid = 0;
		interrupted = 0;
		/*
		 * Only read istat once, since reading it again will unstack
		 * interrupts?
		 */
		istat = NCR53c7x0_read8(hostdata->istat);

		/*
		 * INTFLY interrupts are used by the NCR53c720, NCR53c810,
		 * and NCR53c820 to signify completion of a command.  Since 
		 * the SCSI processor continues running, we can't just look
		 * at the contents of the DSA register and continue running.
		 */
/* XXX - this is too big, offends my sense of aesthetics, and should 
   move to intr_intfly() */
		is_8xx_chip = ((unsigned) (hostdata->chip - 800)) < 100;
		if ((hostdata->options & OPTION_INTFLY) && 
		    (is_8xx_chip && (istat & ISTAT_800_INTF))) {
		    char search_found = 0;	/* Got at least one ? */
		    done = 0;
		    interrupted = 1;

		    /* 
		     * Clear the INTF bit by writing a one.  
		     * This reset operation is self-clearing.
		     */
		    NCR53c7x0_write8(hostdata->istat, istat|ISTAT_800_INTF);

		    if (hostdata->options & OPTION_DEBUG_INTR)
			printk ("scsi%d : INTFLY\n", host->host_no); 

		    /*
		     * Traverse our list of running commands, and look
		     * for those with valid (non-0xff ff) status and message
		     * bytes encoded in the result which signify command
		     * completion.
		     */


		    save_flags(flags);
		    cli();
restart:
		    for (cmd_prev_ptr = (struct NCR53c7x0_cmd **) 
			 &(hostdata->running_list), cmd = 
			 (struct NCR53c7x0_cmd *) hostdata->running_list; cmd ;
			 cmd_prev_ptr = (struct NCR53c7x0_cmd **) &(cmd->next), 
    	    	    	 cmd = (struct NCR53c7x0_cmd *) cmd->next) {
			Scsi_Cmnd *tmp;

			if (!cmd) {
			    printk("scsi%d : very weird.\n", host->host_no);
			    break;
			}

			if (!(tmp = cmd->cmd)) {
			    printk("scsi%d : weird.  NCR53c7x0_cmd has no Scsi_Cmnd\n",
				host->host_no);
				continue;
			}
#if 0
			printk ("scsi%d : looking at result of 0x%x\n",
			    host->host_no, cmd->cmd->result);
#endif

#ifdef __powerpc__
			if (tmp->result == le32_to_cpu(0xffff))
			    continue;
			tmp->result = le32_to_cpu(tmp->result);
#else			
			if (((tmp->result & 0xff) == 0xff) ||
			    ((tmp->result & 0xff00) == 0xff00))
			    continue;
#endif			    

			search_found = 1;

			/* Important - remove from list _before_ done is called */
			if (cmd_prev_ptr)
			    *cmd_prev_ptr = (struct NCR53c7x0_cmd *) cmd->next;

			--hostdata->busy[tmp->target][tmp->lun];
    	    	    	cmd->next = hostdata->free;
    	    	    	hostdata->free = cmd;

    	    	    	tmp->host_scribble = NULL;

			if (hostdata->options & OPTION_DEBUG_INTR) {
			    printk ("scsi%d : command complete : pid %lu, id %d,lun %d result 0x%x ", 
				host->host_no, tmp->pid, tmp->target, tmp->lun, tmp->result);
			    print_command (tmp->cmnd);
			}
			
#if 0
			hostdata->options &= ~OPTION_DEBUG_INTR;
#endif
			tmp->scsi_done(tmp);
			goto restart;

		    }
		    restore_flags(flags);
		   
    /*
     * I think that we're stacking INTFLY interrupts; taking care of 
     * all the finished commands on the first one, and then getting
     * worried when we see the next one.  The magic with have_intfly
     * should tell if this is the case..
     */
		
		    if (!search_found && !have_intfly)  {
			printk ("scsi%d : WARNING : INTFLY with no completed commands.\n",
			    host->host_no);
		    } else if (!have_intfly)  {
			have_intfly = 1; 
    		    	run_process_issue_queue();
		    }
		}

		if (istat & (ISTAT_SIP|ISTAT_DIP)) {
		    done = 0;
		    interrupted = 1;
    	    	    hostdata->state = STATE_HALTED;

		    if (NCR53c7x0_read8 ((hostdata->chip / 100) == 8 ?
			SSTAT1_REG : SSTAT2_REG) & SSTAT2_FF_MASK) 
			printk ("scsi%d : SCSI FIFO not empty\n", 
			    host->host_no);

		    /*
		     * NCR53c700 and NCR53c700-66 change the current SCSI
		     * process, hostdata->curr, in the Linux driver so
		     * cmd = hostdata->curr.
		     *
		     * With other chips, we must look through the commands
		     * executing and find the command structure which 
		     * corresponds to the DSA register.
		     */

		    if (hostdata->options & OPTION_700) {
			cmd = (struct NCR53c7x0_cmd *) hostdata->curr;
		    } else {
			dsa = bus_to_virt(NCR53c7x0_read32(DSA_REG));
			for (cmd = (struct NCR53c7x0_cmd *) 
			    hostdata->running_list; cmd &&
    	    	    	    (dsa + (hostdata->dsa_start / sizeof(u32))) != 
    	    	    	    	cmd->dsa;
			    cmd = (struct NCR53c7x0_cmd *)(cmd->next));
		    }
		    if (hostdata->options & OPTION_DEBUG_INTR) {
			if (cmd) {
			    printk("scsi%d : interrupt for pid %lu, id %d, lun %d ", 
				host->host_no, cmd->cmd->pid, (int) cmd->cmd->target,
				(int) cmd->cmd->lun);
			    print_command (cmd->cmd->cmnd);
			} else {
			    printk("scsi%d : no active command\n", host->host_no);
			}
		    }
		    if (istat & ISTAT_SIP) {
			if (hostdata->options & OPTION_DEBUG_INTR) 
			    printk ("scsi%d : ISTAT_SIP\n", host->host_no);
			intr_scsi (host, cmd);
		    }
		
		    if (istat & ISTAT_DIP) {
			if (hostdata->options & OPTION_DEBUG_INTR) 
			    printk ("scsi%d : ISTAT_DIP\n", host->host_no);
			intr_dma (host, cmd);
		    }

		    if (!hostdata->dstat_valid) {
			hostdata->dstat = NCR53c7x0_read8(DSTAT_REG);
			hostdata->dstat_valid = 1;
		    }

	    /* XXX - code check for 700/800 chips */
		    if (!(hostdata->dstat & DSTAT_DFE)) {
			printk ("scsi%d : DMA FIFO not empty\n", host->host_no);
			if (NCR53c7x0_read8 (CTEST2_REG_800) & CTEST2_800_DDIR) {
			    printk ("scsi%d: Flushing DMA FIFO\n", 
				host->host_no);
			    NCR53c7x0_write8 (CTEST3_REG_800, CTEST3_800_FLF);
			    while (!((hostdata->dstat = NCR53c7x0_read8(DSTAT_REG)) &
				DSTAT_DFE));
			} else 
			{
			    NCR53c7x0_write8 (CTEST3_REG_800, CTEST3_800_CLF);
			    while (NCR53c7x0_read8 (CTEST3_REG_800) & CTEST3_800_CLF);
			}
			hostdata->dstat |= DSTAT_DFE;
		    }
		}
	    } while (interrupted);



	    if (hostdata->intrs != -1)
		hostdata->intrs++;
#if 0
	    if (hostdata->intrs > 40) {
		printk("scsi%d : too many interrupts, halting", host->host_no);
		disable(host);
	    }
#endif

	    if (!hostdata->idle && hostdata->state == STATE_HALTED) {
		if (!hostdata->dsp_changed) {
		    hostdata->dsp = (u32 *) 
			bus_to_virt(NCR53c7x0_read32(DSP_REG));
		}
			
#if 0
		printk("scsi%d : new dsp is 0x%lx (virt 0x%p)\n",
		    host->host_no,  virt_to_bus(hostdata->dsp), hostdata->dsp);
#endif
		
		hostdata->state = STATE_RUNNING;
		NCR53c7x0_write32 (DSP_REG, virt_to_bus(hostdata->dsp));
	    }
	}
    } while (!done);
}


/* 
 * Function : static int abort_connected (struct Scsi_Host *host)
 *
 * Purpose : Assuming that the NCR SCSI processor is currently 
 * 	halted, break the currently established nexus.  Clean
 *	up of the NCR53c7x0_cmd and Scsi_Cmnd structures should
 *	be done on receipt of the abort interrupt.
 *
 * Inputs : host - SCSI host
 *
 */

static int 
abort_connected (struct Scsi_Host *host) {
#ifdef NEW_ABORT
    NCR53c7x0_local_declare();
#endif
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
/* FIXME : this probably should change for production kernels; at the 
   least, counter should move to a per-host structure. */
    static int counter = 5;
#ifdef NEW_ABORT
    int sstat, phase, offset;
    u32 *script;
    NCR53c7x0_local_setup(host);
#endif

    if (--counter <= 0) {
	disable(host);
	return 0;
    }

    printk ("scsi%d : DANGER : abort_connected() called \n",
	host->host_no);

#ifdef NEW_ABORT

/*
 * New strategy : Rather than using a generic abort routine,
 * we'll specifically try to source or sink the appropriate
 * amount of data for the phase we're currently in (taking into 
 * account the current synchronous offset) 
 */

    sstat = (NCR53c8x0_read8 ((chip / 100) == 8 ? SSTAT1_REG : SSTAT2_REG);
    offset = OFFSET (sstat & SSTAT2_FF_MASK) >> SSTAT2_FF_SHIFT;
    phase = sstat & SSTAT2_PHASE_MASK;

/*
 * SET ATN
 * MOVE source_or_sink, WHEN CURRENT PHASE 
 * < repeat for each outstanding byte >
 * JUMP send_abort_message
 */

    script = hostdata->abort_script = kmalloc (
	8  /* instruction size */ * (
	    1 /* set ATN */ +
	    (!offset ? 1 : offset) /* One transfer per outstanding byte */ +
	    1 /* send abort message */),
	GFP_ATOMIC);


#else /* def NEW_ABORT */
    hostdata->dsp = hostdata->script + hostdata->E_initiator_abort /
	    sizeof(u32);
#endif /* def NEW_ABORT */
    hostdata->dsp_changed = 1;

/* XXX - need to flag the command as aborted after the abort_connected
 	 code runs 
 */
    return 0;
}

/*
 * Function : static int datapath_residual (Scsi_Host *host)
 *
 * Purpose : return residual data count of what's in the chip.
 *
 * Inputs : host - SCSI host
 */

static int
datapath_residual (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
    	host->hostdata;
    int count, synchronous, sstat;
    NCR53c7x0_local_setup(host);
    /* COMPAT : the 700 and 700-66 need to use DFIFO_00_BO_MASK */
    count = ((NCR53c7x0_read8 (DFIFO_REG) & DFIFO_10_BO_MASK) -
	(NCR53c7x0_read32 (DBC_REG) & DFIFO_10_BO_MASK)) & DFIFO_10_BO_MASK;
    synchronous = NCR53c7x0_read8 (SXFER_REG) & SXFER_MO_MASK;
    /* COMPAT : DDIR is elsewhere on non-'8xx chips. */
    if (NCR53c7x0_read8 (CTEST2_REG_800) & CTEST2_800_DDIR) {
    /* Receive */
	if (synchronous) 
	    count += (NCR53c7x0_read8 ((hostdata->chip / 100) == 8 ? 
		SSTAT1_REG : SSTAT2_REG) & SSTAT2_FF_MASK) >> SSTAT2_FF_SHIFT;
	else
	    if (NCR53c7x0_read8 ((hostdata->chip / 100) == 8 ? 
		SSTAT0_REG : SSTAT1_REG) & SSTAT1_ILF)
		++count;
    } else {
    /* Send */
	sstat = ((hostdata->chip / 100) == 8) ?  NCR53c7x0_read8 (SSTAT0_REG) :
	    NCR53c7x0_read8 (SSTAT1_REG);
	if (sstat & SSTAT1_OLF)
	    ++count;
	if (synchronous && (sstat & SSTAT1_ORF))
	    ++count;
    }
    return count;
}

/* 
 * Function : static const char * sbcl_to_phase (int sbcl)_
 *
 * Purpose : Convert SBCL register to user-parsable phase representation
 *
 * Inputs : sbcl - value of sbcl register
 */


static const char *
sbcl_to_phase (int sbcl) {
    switch (sbcl & SBCL_PHASE_MASK) {
    case SBCL_PHASE_DATAIN:
	return "DATAIN";
    case SBCL_PHASE_DATAOUT:
	return "DATAOUT";
    case SBCL_PHASE_MSGIN:
	return "MSGIN";
    case SBCL_PHASE_MSGOUT:
	return "MSGOUT";
    case SBCL_PHASE_CMDOUT:
	return "CMDOUT";
    case SBCL_PHASE_STATIN:
	return "STATUSIN";
    default:
	return "unknown";
    }
}

/* 
 * Function : static const char * sstat2_to_phase (int sstat)_
 *
 * Purpose : Convert SSTAT2 register to user-parsable phase representation
 *
 * Inputs : sstat - value of sstat register
 */


static const char *
sstat2_to_phase (int sstat) {
    switch (sstat & SSTAT2_PHASE_MASK) {
    case SSTAT2_PHASE_DATAIN:
	return "DATAIN";
    case SSTAT2_PHASE_DATAOUT:
	return "DATAOUT";
    case SSTAT2_PHASE_MSGIN:
	return "MSGIN";
    case SSTAT2_PHASE_MSGOUT:
	return "MSGOUT";
    case SSTAT2_PHASE_CMDOUT:
	return "CMDOUT";
    case SSTAT2_PHASE_STATIN:
	return "STATUSIN";
    default:
	return "unknown";
    }
}

/* 
 * Function : static void intr_phase_mismatch (struct Scsi_Host *host, 
 *	struct NCR53c7x0_cmd *cmd)
 *
 * Purpose : Handle phase mismatch interrupts
 *
 * Inputs : host, cmd - host and NCR command causing the interrupt, cmd
 * 	may be NULL.
 *
 * Side effects : The abort_connected() routine is called or the NCR chip 
 *	is restarted, jumping to the command_complete entry point, or 
 *	patching the address and transfer count of the current instruction 
 *	and calling the msg_in entry point as appropriate.
 */

static void 
intr_phase_mismatch (struct Scsi_Host *host, struct NCR53c7x0_cmd *cmd) {
    NCR53c7x0_local_declare();
    u32 dbc_dcmd, *dsp, *dsp_next;
    unsigned char dcmd, sbcl;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
    	host->hostdata;
    int residual;
    enum {ACTION_ABORT, ACTION_ABORT_PRINT, ACTION_CONTINUE} action = 
	ACTION_ABORT_PRINT;
    const char *where = NULL;
    NCR53c7x0_local_setup(host);

    /*
     * Corrective action is based on where in the SCSI SCRIPT(tm) the error 
     * occurred, as well as which SCSI phase we are currently in.
     */
    dsp_next = bus_to_virt(NCR53c7x0_read32(DSP_REG));

    /* 
     * Fetch the current instruction, and remove the operands for easier 
     * interpretation.
     */
    dbc_dcmd = NCR53c7x0_read32(DBC_REG);
    dcmd = (dbc_dcmd & 0xff000000) >> 24;
    /*
     * Like other processors, the NCR adjusts the instruction pointer before
     * instruction decode.  Set the DSP address back to what it should
     * be for this instruction based on its size (2 or 3 32 bit words).
     */
    dsp = dsp_next - NCR53c7x0_insn_size(dcmd);


    /*
     * Read new SCSI phase from the SBCL lines.  Since all of our code uses 
     * a WHEN conditional instead of an IF conditional, we don't need to 
     * wait for a new REQ.
     */
    sbcl = NCR53c7x0_read8(SBCL_REG) & SBCL_PHASE_MASK;

    if (!cmd) {
	action = ACTION_ABORT_PRINT;
	where = "no current command";
    /*
     * The way my SCSI SCRIPTS(tm) are architected, recoverable phase
     * mismatches should only occur where we're doing a multi-byte  
     * BMI instruction.  Specifically, this means 
     *
     *  - select messages (a SCSI-I target may ignore additional messages
     * 		after the IDENTIFY; any target may reject a SDTR or WDTR)
     *
     *  - command out (targets may send a message to signal an error 
     * 		condition, or go into STATUSIN after they've decided 
     *		they don't like the command.
     *
     *	- reply_message (targets may reject a multi-byte message in the 
     *		middle)
     *
     * 	- data transfer routines (command completion with buffer space
     *		left, disconnect message, or error message)
     */
    } else if (((dsp >= cmd->data_transfer_start && 
	dsp < cmd->data_transfer_end)) || dsp == (cmd->residual + 2)) {
	if ((dcmd & (DCMD_TYPE_MASK|DCMD_BMI_OP_MASK|DCMD_BMI_INDIRECT|
		DCMD_BMI_MSG|DCMD_BMI_CD)) == (DCMD_TYPE_BMI|
		DCMD_BMI_OP_MOVE_I)) {
	    residual = datapath_residual (host);
	    if (hostdata->options & OPTION_DEBUG_DISCONNECT)
	    	printk ("scsi%d : handling residual transfer (+ %d bytes from DMA FIFO)\n", 
		    host->host_no, residual);

	    /*
	     * The first instruction is a CALL to the alternate handler for 
	     * this data transfer phase, so we can do calls to 
	     * munge_msg_restart as we would if control were passed 
	     * from normal dynamic code.
	     */
	    if (dsp != cmd->residual + 2) {
		cmd->residual[0] = le32_to_cpu(((DCMD_TYPE_TCI | DCMD_TCI_OP_CALL |
			((dcmd & DCMD_BMI_IO) ? DCMD_TCI_IO : 0)) << 24) | 
		    DBC_TCI_WAIT_FOR_VALID | DBC_TCI_COMPARE_PHASE);
		cmd->residual[1] = le32_to_cpu(virt_to_bus(hostdata->script)
		    + ((dcmd & DCMD_BMI_IO)
		       ? hostdata->E_other_in : hostdata->E_other_out));
	    }

	    /*
	     * The second instruction is the a data transfer block
	     * move instruction, reflecting the pointer and count at the 
	     * time of the phase mismatch.
	     */
	    cmd->residual[2] = le32_to_cpu(dbc_dcmd + residual);
	    cmd->residual[3] = le32_to_cpu(NCR53c7x0_read32(DNAD_REG) - residual);

	    /*
	     * The third and final instruction is a jump to the instruction
	     * which follows the instruction which had to be 'split'
	     */
	    if (dsp != cmd->residual + 2) {
		cmd->residual[4] = le32_to_cpu(((DCMD_TYPE_TCI|DCMD_TCI_OP_JUMP) 
		    << 24) | DBC_TCI_TRUE);
		cmd->residual[5] = le32_to_cpu(virt_to_bus(dsp_next));
	    }

	    /*
	     * For the sake of simplicity, transfer control to the 
	     * conditional CALL at the start of the residual buffer.
	     */
	    hostdata->dsp = cmd->residual;
	    hostdata->dsp_changed = 1;
	    action = ACTION_CONTINUE;
	} else {
	    where = "non-BMI dynamic DSA code";
	    action = ACTION_ABORT_PRINT;
	}
    } else if (dsp == (hostdata->script + hostdata->E_select_msgout / 4)) {
	/* Release ATN */
	NCR53c7x0_write8 (SOCL_REG, 0);
	switch (sbcl) {
    /* 
     * Some devices (SQ555 come to mind) grab the IDENTIFY message
     * sent on selection, and decide to go into COMMAND OUT phase
     * rather than accepting the rest of the messages or rejecting
     * them.  Handle these devices gracefully.
     */
	case SBCL_PHASE_CMDOUT:
	    hostdata->dsp = dsp + 2 /* two _words_ */;
	    hostdata->dsp_changed = 1;
	    printk ("scsi%d : target %d ignored SDTR and went into COMMAND OUT\n", 
		host->host_no, cmd->cmd->target);
	    cmd->flags &= ~CMD_FLAG_SDTR;
	    action = ACTION_CONTINUE;
	    break;
	case SBCL_PHASE_MSGIN:
	    hostdata->dsp = hostdata->script + hostdata->E_msg_in / 
		sizeof(u32);
	    hostdata->dsp_changed = 1;
	    action = ACTION_CONTINUE;
	    break;
	default:
	    where="select message out";
	    action = ACTION_ABORT_PRINT;
	}
    /*
     * Some SCSI devices will interpret a command as they read the bytes
     * off the SCSI bus, and may decide that the command is Bogus before 
     * they've read the entire command off the bus.
     */
    } else if (dsp == hostdata->script + hostdata->E_cmdout_cmdout / sizeof 
	(u32)) {
	hostdata->dsp = hostdata->script + hostdata->E_data_transfer /
	    sizeof (u32);
	hostdata->dsp_changed = 1;
	action = ACTION_CONTINUE;
    /* FIXME : we need to handle message reject, etc. within msg_respond. */
#ifdef notyet
    } else if (dsp == hostdata->script + hostdata->E_reply_message) {
	switch (sbcl) {
    /* Any other phase mismatches abort the currently executing command.  */
#endif
    } else {
	where = "unknown location";
	action = ACTION_ABORT_PRINT;
    }

    /* Flush DMA FIFO */
    if (!hostdata->dstat_valid) {
	hostdata->dstat = NCR53c7x0_read8(DSTAT_REG);
	hostdata->dstat_valid = 1;
    }
    if (!(hostdata->dstat & DSTAT_DFE)) {
	if (NCR53c7x0_read8 (CTEST2_REG_800) & CTEST2_800_DDIR) {
	    printk ("scsi%d: Flushing DMA FIFO\n", 
		    host->host_no);
	    NCR53c7x0_write8 (CTEST3_REG_800, CTEST3_800_FLF);
	    /* FIXME : what about stacked DMA interrupts? */
	    while (!((hostdata->dstat = NCR53c7x0_read8(DSTAT_REG)) &
		DSTAT_DFE));
	} else {
	    NCR53c7x0_write8 (CTEST3_REG_800, CTEST3_800_CLF);
	    while (NCR53c7x0_read8 (CTEST3_REG_800) & CTEST3_800_CLF);
	}
	hostdata->dstat |= DSTAT_DFE;
    }

    switch (action) {
    case ACTION_ABORT_PRINT:
	printk("scsi%d : %s : unexpected phase %s.\n",
	     host->host_no, where ? where : "unknown location", 
	     sbcl_to_phase(sbcl));
	print_lots (host);
    /* Fall through to ACTION_ABORT */
    case ACTION_ABORT:
	abort_connected (host);
	break;
    case ACTION_CONTINUE:
	break;
    }

#if 0
    if (hostdata->dsp_changed) {
	printk("scsi%d: new dsp 0x%p\n", host->host_no, hostdata->dsp);
	print_insn (host, hostdata->dsp, "", 1);
    }
#endif
    
}

/*
 * Function : static void intr_bf (struct Scsi_Host *host, 
 * 	struct NCR53c7x0_cmd *cmd)
 *
 * Purpose : handle BUS FAULT interrupts 
 *
 * Inputs : host, cmd - host and NCR command causing the interrupt, cmd
 * 	may be NULL.
 */

static void
intr_bf (struct Scsi_Host *host, struct NCR53c7x0_cmd *cmd) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	    host->hostdata;
    u32 *dsp,
	*next_dsp,		/* Current dsp */
    	*dsa,
	dbc_dcmd;		/* DCMD (high eight bits) + DBC */
    unsigned short pci_status;
    int tmp;
    unsigned long flags;
    char *reason = NULL;
    /* Default behavior is for a silent error, with a retry until we've
       exhausted retries. */
    enum {MAYBE, ALWAYS, NEVER} retry = MAYBE;
    int report = 0;
    NCR53c7x0_local_setup(host);

    dbc_dcmd = NCR53c7x0_read32 (DBC_REG);
    next_dsp = bus_to_virt (NCR53c7x0_read32(DSP_REG));
    dsp = next_dsp - NCR53c7x0_insn_size ((dbc_dcmd >> 24) & 0xff);
/* FIXME - check chip type  */
    dsa = bus_to_virt (NCR53c7x0_read32(DSA_REG));

    /*
     * Bus faults can be caused by either a Bad Address or 
     * Target Abort. We should check the Received Target Abort
     * bit of the PCI status register and Master Abort Bit.
     *
     * 	- Master Abort bit indicates that no device claimed
     *		the address with DEVSEL within five clocks
     *
     *	- Target Abort bit indicates that a target claimed it,
     *		but changed its mind once it saw the byte enables.
     *
     */

    if ((hostdata->chip / 100) == 8) {
	save_flags (flags);
	cli();
	tmp = pcibios_read_config_word (hostdata->pci_bus, 
	    hostdata->pci_device_fn, PCI_STATUS, &pci_status);
	restore_flags (flags);
	if (tmp == PCIBIOS_SUCCESSFUL) {
	    if (pci_status & PCI_STATUS_REC_TARGET_ABORT) {
		reason = "PCI target abort";
		pci_status &= ~PCI_STATUS_REC_TARGET_ABORT;
	    } else if (pci_status & PCI_STATUS_REC_MASTER_ABORT) {
		reason = "No device asserted PCI DEVSEL within five bus clocks";
		pci_status &= ~PCI_STATUS_REC_MASTER_ABORT;
	    } else if (pci_status & PCI_STATUS_PARITY) {
		report = 1;   
		pci_status &= ~PCI_STATUS_PARITY;
	    }
	} else {
	    printk ("scsi%d : couldn't read status register : error %d\n",
		host->host_no, tmp);
	    retry = NEVER;
	}
    }

#ifndef notyet
    report = 1;
#endif
    if (report && reason) {
	printk(KERN_ALERT "scsi%d : BUS FAULT reason = %s\n",
	     host->host_no, reason ? reason : "unknown");
	print_lots (host);
    }

#ifndef notyet
    retry = NEVER;
#endif

    /* 
     * TODO : we should attempt to recover from any spurious bus 
     * faults.  After X retries, we should figure that things are 
     * sufficiently wedged, and call NCR53c7xx_reset.
     *
     * This code should only get executed once we've decided that we 
     * cannot retry.
     */

    if (retry == NEVER) {
    	printk(KERN_ALERT "          mail drew@PoohSticks.ORG\n");
    	FATAL (host);
    }
}

/*
 * Function : static void intr_dma (struct Scsi_Host *host, 
 * 	struct NCR53c7x0_cmd *cmd)
 *
 * Purpose : handle all DMA interrupts, indicated by the setting 
 * 	of the DIP bit in the ISTAT register.
 *
 * Inputs : host, cmd - host and NCR command causing the interrupt, cmd
 * 	may be NULL.
 */

static void 
intr_dma (struct Scsi_Host *host, struct NCR53c7x0_cmd *cmd) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    unsigned char dstat;	/* DSTAT */	
    u32 *dsp,
	*next_dsp,		/* Current dsp */
    	*dsa,
	dbc_dcmd;		/* DCMD (high eight bits) + DBC */
    int tmp;
    unsigned long flags;
    NCR53c7x0_local_setup(host);

    if (!hostdata->dstat_valid) {
	hostdata->dstat = NCR53c7x0_read8(DSTAT_REG);
	hostdata->dstat_valid = 1;
    }
    
    dstat = hostdata->dstat;
    
    if (hostdata->options & OPTION_DEBUG_INTR)
	printk("scsi%d : DSTAT=0x%x\n", host->host_no, (int) dstat);

    dbc_dcmd = NCR53c7x0_read32 (DBC_REG);
    next_dsp = bus_to_virt(NCR53c7x0_read32(DSP_REG));
    dsp = next_dsp - NCR53c7x0_insn_size ((dbc_dcmd >> 24) & 0xff);
/* XXX - check chip type */
    dsa = bus_to_virt(NCR53c7x0_read32(DSA_REG));

    /*
     * DSTAT_ABRT is the aborted interrupt.  This is set whenever the 
     * SCSI chip is aborted.  
     * 
     * With NCR53c700 and NCR53c700-66 style chips, we should only 
     * get this when the chip is currently running the accept 
     * reselect/select code and we have set the abort bit in the 
     * ISTAT register.
     *
     */
    
    if (dstat & DSTAT_ABRT) {
#if 0
	/* XXX - add code here to deal with normal abort */
	if ((hostdata->options & OPTION_700) && (hostdata->state ==
	    STATE_ABORTING)) {
	} else 
#endif
	{
	    printk(KERN_ALERT "scsi%d : unexpected abort interrupt at\n" 
		   "         ", host->host_no);
	    print_insn (host, dsp, KERN_ALERT "s ", 1);
	    FATAL (host);
	}
    }

    /*
     * DSTAT_SSI is the single step interrupt.  Should be generated 
     * whenever we have single stepped or are tracing.
     */

    if (dstat & DSTAT_SSI) {
	if (hostdata->options & OPTION_DEBUG_TRACE) {
	} else if (hostdata->options & OPTION_DEBUG_SINGLE) {
	    print_insn (host, dsp, "s ", 0);
	    save_flags(flags);
	    cli();
/* XXX - should we do this, or can we get away with writing dsp? */

	    NCR53c7x0_write8 (DCNTL_REG, (NCR53c7x0_read8(DCNTL_REG) & 
    	    	~DCNTL_SSM) | DCNTL_STD);
	    restore_flags(flags);
	} else {
	    printk(KERN_ALERT "scsi%d : unexpected single step interrupt at\n"
		   "         ", host->host_no);
	    print_insn (host, dsp, KERN_ALERT "", 1);
	    printk(KERN_ALERT "         mail drew@PoohSticks.ORG\n");
    	    FATAL (host);
    	}
    }

    /*
     * DSTAT_IID / DSTAT_OPC (same bit, same meaning, only the name 
     * is different) is generated whenever an illegal instruction is 
     * encountered.  
     * 
     * XXX - we may want to emulate INTFLY here, so we can use 
     *    the same SCSI SCRIPT (tm) for NCR53c710 through NCR53c810  
     *	  chips.
     */

    if (dstat & DSTAT_OPC) {
    /* 
     * Ascertain if this IID interrupts occurred before or after a STO 
     * interrupt.  Since the interrupt handling code now leaves 
     * DSP unmodified until _after_ all stacked interrupts have been
     * processed, reading the DSP returns the original DSP register.
     * This means that if dsp lies between the select code, and 
     * message out following the selection code (where the IID interrupt
     * would have to have occurred by due to the implicit wait for REQ),
     * we have an IID interrupt resulting from a STO condition and 
     * can ignore it.
     */

	if (((dsp >= (hostdata->script + hostdata->E_select / sizeof(u32))) &&
	    (dsp <= (hostdata->script + hostdata->E_select_msgout / 
    	    sizeof(u32) + 8))) || (hostdata->test_running == 2)) {
	    if (hostdata->options & OPTION_DEBUG_INTR) 
		printk ("scsi%d : ignoring DSTAT_IID for SSTAT_STO\n",
		    host->host_no);
	    if (hostdata->expecting_iid) {
		hostdata->expecting_iid = 0;
		hostdata->idle = 1;
		if (hostdata->test_running == 2) {
		    hostdata->test_running = 0;
		    hostdata->test_completed = 3;
		} else if (cmd) 
			abnormal_finished (cmd, DID_BAD_TARGET << 16);
	    } else {
		hostdata->expecting_sto = 1;
	    }
    /*
     * We can't guarantee we'll be able to execute the WAIT DISCONNECT
     * instruction within the 3.4us of bus free and arbitration delay
     * that a target can RESELECT in and assert REQ after we've dropped
     * ACK.  If this happens, we'll get an illegal instruction interrupt.
     * Doing away with the WAIT DISCONNECT instructions broke everything,
     * so instead I'll settle for moving one WAIT DISCONNECT a few 
     * instructions closer to the CLEAR ACK before it to minimize the
     * chances of this happening, and handle it if it occurs anyway.
     *
     * Simply continue with what we were doing, and control should
     * be transferred to the schedule routine which will ultimately
     * pass control onto the reselection or selection (not yet)
     * code.
     */
	} else if (dbc_dcmd == 0x48000000 && (NCR53c7x0_read8 (SBCL_REG) &
	    SBCL_REQ)) {
	    if (!(hostdata->options & OPTION_NO_PRINT_RACE))
	    {
		printk("scsi%d: REQ before WAIT DISCONNECT IID\n", 
		    host->host_no);
		hostdata->options |= OPTION_NO_PRINT_RACE;
	    }
	} else {
	    printk(KERN_ALERT "scsi%d : illegal instruction\n", host->host_no);
	    print_lots (host);
	    printk(KERN_ALERT "         mail drew@PoohSticks.ORG with ALL\n"
		              "         boot messages and diagnostic output\n");
    	    FATAL (host);
	}
    }

    /* 
     * DSTAT_BF are bus fault errors
     */
    
    if (dstat & DSTAT_800_BF) {
	intr_bf (host, cmd);
    }
	

    /* 
     * DSTAT_SIR interrupts are generated by the execution of 
     * the INT instruction.  Since the exact values available 
     * are determined entirely by the SCSI script running, 
     * and are local to a particular script, a unique handler
     * is called for each script.
     */

    if (dstat & DSTAT_SIR) {
	if (hostdata->options & OPTION_DEBUG_INTR)
	    printk ("scsi%d : DSTAT_SIR\n", host->host_no);
	switch ((tmp = hostdata->dstat_sir_intr (host, cmd))) {
	case SPECIFIC_INT_NOTHING:
	case SPECIFIC_INT_RESTART:
	    break;
	case SPECIFIC_INT_ABORT:
	    abort_connected(host);
	    break;
	case SPECIFIC_INT_PANIC:
	    printk(KERN_ALERT "scsi%d : failure at ", host->host_no);
	    print_insn (host, dsp, KERN_ALERT "", 1);
	    printk(KERN_ALERT "          dstat_sir_intr() returned SPECIFIC_INT_PANIC\n");
    	    FATAL (host);
	    break;
	case SPECIFIC_INT_BREAK:
	    intr_break (host, cmd);
	    break;
	default:
	    printk(KERN_ALERT "scsi%d : failure at ", host->host_no);
	    print_insn (host, dsp, KERN_ALERT "", 1);
	    printk(KERN_ALERT"          dstat_sir_intr() returned unknown value %d\n", 
		tmp);
    	    FATAL (host);
	}
    } 

    if ((hostdata->chip / 100) == 8 && (dstat & DSTAT_800_MDPE)) {
	printk(KERN_ALERT "scsi%d : Master Data Parity Error\n",
	    host->host_no);
	FATAL (host);
    }
}

/*
 * Function : static int print_insn (struct Scsi_Host *host, 
 * 	u32 *insn, int kernel)
 *
 * Purpose : print numeric representation of the instruction pointed
 * 	to by insn to the debugging or kernel message buffer
 *	as appropriate.  
 *
 * 	If desired, a user level program can interpret this 
 * 	information.
 *
 * Inputs : host, insn - host, pointer to instruction, prefix - 
 *	string to prepend, kernel - use printk instead of debugging buffer.
 *
 * Returns : size, in u32s, of instruction printed.
 */

/*
 * FIXME: should change kernel parameter so that it takes an ENUM
 * 	specifying severity - either KERN_ALERT or KERN_PANIC so
 *	all panic messages are output with the same severity.
 */

static int 
print_insn (struct Scsi_Host *host, const u32 *insn, 
    const char *prefix, int kernel) {
    char buf[160], 		/* Temporary buffer and pointer.  ICKY 
				   arbitrary length.  */

		
	*tmp;			
    unsigned char dcmd;		/* dcmd register for *insn */
    int size;

    /* 
     * Check to see if the instruction pointer is not bogus before 
     * indirecting through it; avoiding red-zone at start of 
     * memory.
     *
     * FIXME: icky magic needs to happen here on non-intel boxes which
     * don't have kernel memory mapped in like this.  Might be reasonable
     * to use vverify()?
     */

    if (virt_to_phys((void *)insn) < PAGE_SIZE || 
	virt_to_phys((void *)(insn + 8)) > virt_to_phys(high_memory) ||
	((((dcmd = (insn[0] >> 24) & 0xff) & DCMD_TYPE_MMI) == DCMD_TYPE_MMI) &&
	virt_to_phys((void *)(insn + 12)) > virt_to_phys(high_memory))) {
	size = 0;
	sprintf (buf, "%s%p: address out of range\n",
	    prefix, insn);
    } else {
/* 
 * FIXME : (void *) cast in virt_to_bus should be unnecessary, because
 * 	it should take const void * as argument.
 */
	sprintf(buf, "%s0x%lx (virt 0x%p) : 0x%08x 0x%08x (virt 0x%p)", 
	    (prefix ? prefix : ""), virt_to_bus((void *) insn), insn,  
	    insn[0], insn[1], bus_to_virt (le32_to_cpu(insn[1])));
	tmp = buf + strlen(buf);
	if ((dcmd & DCMD_TYPE_MASK) == DCMD_TYPE_MMI)  {
	    sprintf (tmp, " 0x%08x (virt 0x%p)\n", insn[2], 
		bus_to_virt(le32_to_cpu(insn[2])));
	    size = 3;
	} else {
	    sprintf (tmp, "\n");
	    size = 2;
	}
    }

    if (kernel) 
	printk ("%s", buf);
#ifdef NCR_DEBUG
    else {
	size_t len = strlen(buf);
	debugger_kernel_write(host, buf, len);
    }
#endif
    return size;
}

/*
 * Function : static const char *ncr_state (int state)
 *
 * Purpose : convert state (probably from hostdata->state) to a string
 *
 * Inputs : state
 *
 * Returns : char * representation of state, "unknown" on error.
 */

#if 0
static const char *
ncr_state (int state) {
    switch (state) {
    case STATE_HALTED: return "halted";
    case STATE_WAITING: return "waiting";
    case STATE_RUNNING: return "running";
    case STATE_ABORTING: return "aborting";
    case STATE_DISABLED: return "disabled";
    default: return "unknown";
    }
}
#endif

/*
 * Function : int NCR53c7xx_abort (Scsi_Cmnd *cmd)
 * 
 * Purpose : Abort an errant SCSI command, doing all necessary
 *	cleanup of the issue_queue, running_list, shared Linux/NCR
 *	dsa issue and reconnect queues.
 *
 * Inputs : cmd - command to abort, code - entire result field
 *
 * Returns : 0 on success, -1 on failure.
 */

int 
NCR53c7xx_abort (Scsi_Cmnd *cmd) {
    NCR53c7x0_local_declare();
    struct Scsi_Host *host = cmd->host;
    struct NCR53c7x0_hostdata *hostdata = host ? (struct NCR53c7x0_hostdata *) 
	host->hostdata : NULL;
    unsigned long flags;
    unsigned long result;
    struct NCR53c7x0_cmd *curr, **prev;
    Scsi_Cmnd *me, **last;
#if 0
    static long cache_pid = -1;
#endif


    if (!host) {
	printk ("Bogus SCSI command pid %ld; no host structure\n",
	    cmd->pid);
	return SCSI_ABORT_ERROR;
    } else if (!hostdata) {
	printk ("Bogus SCSI host %d; no hostdata\n", host->host_no);
	return SCSI_ABORT_ERROR;
    }
    NCR53c7x0_local_setup(host);

/*
 * CHECK : I don't think that reading ISTAT will unstack any interrupts,
 *	since we need to write the INTF bit to clear it, and SCSI/DMA
 * 	interrupts don't clear until we read SSTAT/SIST and DSTAT registers.
 *	
 *	See that this is the case.
 *
 * I suspect that several of our failures may be coming from a new fatal
 * interrupt (possibly due to a phase mismatch) happening after we've left
 * the interrupt handler, but before the PIC has had the interrupt condition
 * cleared.
 */

    if (NCR53c7x0_read8(hostdata->istat) & 
	(ISTAT_DIP|ISTAT_SIP|
	    (hostdata->chip / 100 == 8 ? ISTAT_800_INTF : 0))) {
	printk ("scsi%d : dropped interrupt for command %ld\n", host->host_no,
	    cmd->pid);
	NCR53c7x0_intr (host->irq, NULL, NULL);
	return SCSI_ABORT_BUSY;
    }
	
    save_flags(flags);
    cli();
#if 0
    if (cache_pid == cmd->pid) 
	panic ("scsi%d : bloody fetus %d\n", host->host_no, cmd->pid);
    else
	cache_pid = cmd->pid;
#endif
	

/*
 * The command could be hiding in the issue_queue.  This would be very
 * nice, as commands can't be moved from the high level driver's issue queue 
 * into the shared queue until an interrupt routine is serviced, and this
 * moving is atomic.  
 *
 * If this is the case, we don't have to worry about anything - we simply
 * pull the command out of the old queue, and call it aborted.
 */

    for (me = (Scsi_Cmnd *) hostdata->issue_queue, 
         last = (Scsi_Cmnd **) &(hostdata->issue_queue);
	 me && me != cmd;  last = (Scsi_Cmnd **)&(me->SCp.ptr), 
	 me = (Scsi_Cmnd *)me->SCp.ptr);

    if (me) {
	*last = (Scsi_Cmnd *) me->SCp.ptr;
	if (me->host_scribble) {
	    ((struct NCR53c7x0_cmd *)me->host_scribble)->next = hostdata->free;
	    hostdata->free = (struct NCR53c7x0_cmd *) me->host_scribble;
	    me->host_scribble = NULL;
	}
	cmd->result = DID_ABORT << 16;
	cmd->scsi_done(cmd);
	printk ("scsi%d : found command %ld in Linux issue queue\n", 
	    host->host_no, me->pid);
	restore_flags(flags);
    	run_process_issue_queue();
	return SCSI_ABORT_SUCCESS;
    }

/* 
 * That failing, the command could be in our list of already executing 
 * commands.  If this is the case, drastic measures are called for.  
 */ 

    for (curr = (struct NCR53c7x0_cmd *) hostdata->running_list, 
    	 prev = (struct NCR53c7x0_cmd **) &(hostdata->running_list);
	 curr && curr->cmd != cmd; prev = (struct NCR53c7x0_cmd **) 
         &(curr->next), curr = (struct NCR53c7x0_cmd *) curr->next);

    if (curr) {
	result = le32_to_cpu(cmd->result);
	if ((result & 0xff) != 0xff && (result & 0xff00) != 0xff00) {
	    if (prev)
		*prev = (struct NCR53c7x0_cmd *) curr->next;
	    curr->next = (struct NCR53c7x0_cmd *) hostdata->free;
	    cmd->host_scribble = NULL;
	    hostdata->free = curr;
	    cmd->scsi_done(cmd);
	printk ("scsi%d : found finished command %ld in running list\n", 
	    host->host_no, cmd->pid);
	    restore_flags(flags);
	    return SCSI_ABORT_NOT_RUNNING;
	} else {
	    printk ("scsi%d : DANGER : command running, can not abort.\n",
		cmd->host->host_no);
	    restore_flags(flags);
	    return SCSI_ABORT_BUSY;
	}
    }

/* 
 * And if we couldn't find it in any of our queues, it must have been 
 * a dropped interrupt.
 */

    curr = (struct NCR53c7x0_cmd *) cmd->host_scribble;
    if (curr) {
	curr->next = hostdata->free;
	hostdata->free = curr;
	cmd->host_scribble = NULL;
    }

    result = le32_to_cpu(cmd->result);
    if (((result & 0xff00) == 0xff00) ||
	((result & 0xff) == 0xff)) {
	printk ("scsi%d : did this command ever run?\n", host->host_no);
	cmd->result = DID_ABORT << 16;
    } else {
	printk ("scsi%d : probably lost INTFLY, normal completion\n", 
	    host->host_no);
/* 
 * FIXME : We need to add an additional flag which indicates if a 
 * command was ever counted as BUSY, so if we end up here we can
 * decrement the busy count if and only if it is necessary.
 */
        --hostdata->busy[cmd->target][cmd->lun];
    }
    restore_flags(flags);
    cmd->scsi_done(cmd);

/* 
 * We need to run process_issue_queue since termination of this command 
 * may allow another queued command to execute first? 
 */
    return SCSI_ABORT_NOT_RUNNING;
}

/*
 * Function : int NCR53c7xx_reset (Scsi_Cmnd *cmd) 
 * 
 * Purpose : perform a hard reset of the SCSI bus and NCR
 * 	chip.
 *
 * Inputs : cmd - command which caused the SCSI RESET
 *
 * Returns : 0 on success.
 */
 
int 
NCR53c7xx_reset (Scsi_Cmnd *cmd, unsigned int reset_flags) {
    NCR53c7x0_local_declare();
    unsigned long flags;
    int found = 0;
    struct NCR53c7x0_cmd * c;
    Scsi_Cmnd *tmp;
    /*
     * When we call scsi_done(), it's going to wake up anything sleeping on the
     * resources which were in use by the aborted commands, and we'll start to 
     * get new commands.
     *
     * We can't let this happen until after we've re-initialized the driver
     * structures, and can't reinitialize those structures until after we've 
     * dealt with their contents.
     *
     * So, we need to find all of the commands which were running, stick
     * them on a linked list of completed commands (we'll use the host_scribble
     * pointer), do our reinitialization, and then call the done function for
     * each command.  
     */
    Scsi_Cmnd *nuke_list = NULL;
    struct Scsi_Host *host = cmd->host;
    struct NCR53c7x0_hostdata *hostdata = 
    	(struct NCR53c7x0_hostdata *) host->hostdata;

    NCR53c7x0_local_setup(host);
    save_flags(flags);
    cli();
    ncr_halt (host);
    print_lots (host);
    dump_events (host, 30);
    ncr_scsi_reset (host);
    for (tmp = nuke_list = return_outstanding_commands (host, 1 /* free */,
	0 /* issue */ ); tmp; tmp = (Scsi_Cmnd *) tmp->SCp.buffer)
	if (tmp == cmd) {
	    found = 1;
	    break;
	}
	    
    /* 
     * If we didn't find the command which caused this reset in our running
     * list, then we've lost it.  See that it terminates normally anyway.
     */
    if (!found) {
    	c = (struct NCR53c7x0_cmd *) cmd->host_scribble;
    	if (c) {
	    cmd->host_scribble = NULL;
    	    c->next = hostdata->free;
    	    hostdata->free = c;
    	} else
	    printk ("scsi%d: lost command %ld\n", host->host_no, cmd->pid);
	cmd->SCp.buffer = (struct scatterlist *) nuke_list;
	nuke_list = cmd;
    }

    NCR53c7x0_driver_init (host);
    hostdata->soft_reset (host);
    if (hostdata->resets == 0) 
	disable(host);
    else if (hostdata->resets != -1)
	--hostdata->resets;
    restore_flags(flags);
    for (; nuke_list; nuke_list = tmp) {
	tmp = (Scsi_Cmnd *) nuke_list->SCp.buffer;
    	nuke_list->result = DID_RESET << 16;
	nuke_list->scsi_done (nuke_list);
    }
    restore_flags(flags);
    return SCSI_RESET_SUCCESS;
}

/*
 * The NCR SDMS bios follows Annex A of the SCSI-CAM draft, and 
 * therefore shares the scsicam_bios_param function.
 */

/*
 * Function : int insn_to_offset (Scsi_Cmnd *cmd, u32 *insn)
 *
 * Purpose : convert instructions stored at NCR pointer into data 
 *	pointer offset.
 * 
 * Inputs : cmd - SCSI command; insn - pointer to instruction.  Either current
 *	DSP, or saved data pointer.
 *
 * Returns : offset on success, -1 on failure.
 */


static int 
insn_to_offset (Scsi_Cmnd *cmd, u32 *insn) {
    struct NCR53c7x0_hostdata *hostdata = 
	(struct NCR53c7x0_hostdata *) cmd->host->hostdata;
    struct NCR53c7x0_cmd *ncmd = 
	(struct NCR53c7x0_cmd *) cmd->host_scribble;
    int offset = 0, buffers;
    struct scatterlist *segment;
    char *ptr;
    int found = 0;

/*
 * With the current code implementation, if the insn is inside dynamically 
 * generated code, the data pointer will be the instruction preceding 
 * the next transfer segment.
 */

    if (!check_address ((unsigned long) ncmd, sizeof (struct NCR53c7x0_cmd)) &&
	((insn >= ncmd->data_transfer_start &&  
    	    insn < ncmd->data_transfer_end) ||
    	(insn >= ncmd->residual &&
    	    insn < (ncmd->residual + 
    	    	sizeof(ncmd->residual))))) {
	    ptr = bus_to_virt(le32_to_cpu(insn[3]));

	    if ((buffers = cmd->use_sg)) {
    	    	for (offset = 0, 
		     	segment = (struct scatterlist *) cmd->buffer;
    	    	     buffers && !((found = ((ptr >= segment->address) && 
    	    	    	    (ptr < (segment->address + segment->length)))));
    	    	     --buffers, offset += segment->length, ++segment)
#if 0
		    printk("scsi%d: comparing 0x%p to 0x%p\n", 
			cmd->host->host_no, saved, segment->address);
#else
		    ;
#endif
    	    	    offset += ptr - segment->address;
    	    } else {
		found = 1;
    	    	offset = ptr - (char *) (cmd->request_buffer);
    	    }
    } else if ((insn >= hostdata->script + 
		hostdata->E_data_transfer / sizeof(u32)) &&
	       (insn <= hostdata->script +
		hostdata->E_end_data_transfer / sizeof(u32))) {
    	found = 1;
	offset = 0;
    }
    return found ? offset : -1;
}



/*
 * Function : void print_progress (Scsi_Cmnd *cmd) 
 * 
 * Purpose : print the current location of the saved data pointer
 *
 * Inputs : cmd - command we are interested in
 *
 */

static void 
print_progress (Scsi_Cmnd *cmd) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_cmd *ncmd = 
	(struct NCR53c7x0_cmd *) cmd->host_scribble;
    int offset, i;
    char *where;
    u32 *ptr;
    NCR53c7x0_local_setup (cmd->host);
    for (i = 0; i < 2; ++i) {
	if (check_address ((unsigned long) ncmd, 
	    sizeof (struct NCR53c7x0_cmd)) == -1) 
	    continue;
	if (!i) {
	    where = "saved";
	    ptr = bus_to_virt(le32_to_cpu(ncmd->saved_data_pointer));
	} else {
	    where = "active";
	    ptr = bus_to_virt (NCR53c7x0_read32 (DSP_REG) -
		NCR53c7x0_insn_size (NCR53c7x0_read8 (DCMD_REG)) *
		sizeof(u32));
	} 
	offset = insn_to_offset (cmd, ptr);

	if (offset != -1) 
	    printk ("scsi%d : %s data pointer at offset %d\n",
		cmd->host->host_no, where, offset);
	else {
	    int size;
	    printk ("scsi%d : can't determine %s data pointer offset\n",
		cmd->host->host_no, where);
	    if (ncmd) {
		size = print_insn (cmd->host, 
		    bus_to_virt(le32_to_cpu(ncmd->saved_data_pointer)), "", 1);
		print_insn (cmd->host, 
		    bus_to_virt(le32_to_cpu(ncmd->saved_data_pointer)) + size * sizeof(u32),
		    "", 1);
	    }
	}
    }
}


static void 
print_dsa (struct Scsi_Host *host, u32 *dsa, const char *prefix) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    int i, len;
    char *ptr;
    Scsi_Cmnd *cmd;

    if (check_address ((unsigned long) dsa, hostdata->dsa_end - 
	hostdata->dsa_start) == -1) {
	printk("scsi%d : bad dsa virt 0x%p\n", host->host_no, dsa);
	return;
    }
    printk("%sscsi%d : dsa at phys 0x%lx (virt 0x%p)\n"
	    "        + %d : dsa_msgout length = %u, data = 0x%x (virt 0x%p)\n" ,
    	    prefix ? prefix : "",
    	    host->host_no,  virt_to_bus (dsa), dsa, hostdata->dsa_msgout,
    	    le32_to_cpu(dsa[hostdata->dsa_msgout / sizeof(u32)]),
	    le32_to_cpu(dsa[hostdata->dsa_msgout / sizeof(u32) + 1]),
	    bus_to_virt (le32_to_cpu(dsa[hostdata->dsa_msgout / sizeof(u32) + 1])));

    /* 
     * Only print messages if they're sane in length so we don't
     * blow the kernel printk buffer on something which won't buy us
     * anything.
     */

    if (le32_to_cpu(dsa[hostdata->dsa_msgout / sizeof(u32)]) < 
	    sizeof (hostdata->free->select)) 
	for (i = le32_to_cpu(dsa[hostdata->dsa_msgout / sizeof(u32)]),
	    ptr = bus_to_virt (le32_to_cpu(dsa[hostdata->dsa_msgout / sizeof(u32) + 1])); 
	    i > 0 && !check_address ((unsigned long) ptr, 1);
	    ptr += len, i -= len) {
	    printk("               ");
	    len = print_msg (ptr);
	    printk("\n");
	    if (!len)
		break;
	}

    printk("        + %d : select_indirect = 0x%x\n",
	hostdata->dsa_select, le32_to_cpu(dsa[hostdata->dsa_select / sizeof(u32)]));
    cmd = (Scsi_Cmnd *) bus_to_virt(le32_to_cpu(dsa[hostdata->dsa_cmnd / sizeof(u32)]));
    printk("        + %d : dsa_cmnd = 0x%x ", hostdata->dsa_cmnd,
	   (u32) virt_to_bus(cmd));
    if (cmd) {
	printk("               result = 0x%x, target = %d, lun = %d, cmd = ",
	    cmd->result, cmd->target, cmd->lun);
	print_command(cmd->cmnd);
    } else
	printk("\n");
    printk("        + %d : dsa_next = 0x%x\n", hostdata->dsa_next,
	le32_to_cpu(dsa[hostdata->dsa_next / sizeof(u32)]));
    if (cmd) { 
	printk("scsi%d target %d : sxfer_sanity = 0x%x, scntl3_sanity = 0x%x\n"
	       "                   script : ",
	    host->host_no, cmd->target,
	    hostdata->sync[cmd->target].sxfer_sanity,
	    hostdata->sync[cmd->target].scntl3_sanity);
	for (i = 0; i < (sizeof(hostdata->sync[cmd->target].script) / 4); ++i)
	    printk ("0x%x ", hostdata->sync[cmd->target].script[i]);
	printk ("\n");
    	print_progress (cmd);
    }
}
/*
 * Function : void print_queues (Scsi_Host *host) 
 * 
 * Purpose : print the contents of the NCR issue and reconnect queues
 *
 * Inputs : host - SCSI host we are interested in
 *
 */

static void 
print_queues (struct Scsi_Host *host) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    u32 *dsa, *next_dsa;
    volatile u32 *curr;
    int left;
    Scsi_Cmnd *cmd, *next_cmd;
    unsigned long flags;

    printk ("scsi%d : issue queue\n", host->host_no);

    for (left = host->can_queue, cmd = (Scsi_Cmnd *) hostdata->issue_queue; 
	    left >= 0 && cmd; 
	    cmd = next_cmd) {
	next_cmd = (Scsi_Cmnd *) cmd->SCp.ptr;
	save_flags(flags);
	cli();
	if (cmd->host_scribble) {
	    if (check_address ((unsigned long) (cmd->host_scribble), 
		sizeof (cmd->host_scribble)) == -1)
		printk ("scsi%d: scsi pid %ld bad pointer to NCR53c7x0_cmd\n",
		    host->host_no, cmd->pid);
	    /* print_dsa does sanity check on address, no need to check */
	    else
	    	print_dsa (host, bus_to_virt(le32_to_cpu(((struct NCR53c7x0_cmd *) cmd->host_scribble)-> dsa)), "");
	} else 
	    printk ("scsi%d : scsi pid %ld for target %d lun %d has no NCR53c7x0_cmd\n",
		host->host_no, cmd->pid, cmd->target, cmd->lun);
	restore_flags(flags);
    }

    if (left <= 0) {
	printk ("scsi%d : loop detected in issue queue\n",
	    host->host_no);
    }

    /*
     * Traverse the NCR reconnect and start DSA structures, printing out 
     * each element until we hit the end or detect a loop.  Currently,
     * the reconnect structure is a linked list; and the start structure
     * is an array.  Eventually, the reconnect structure will become a 
     * list as well, since this simplifies the code.
     */

    printk ("scsi%d : schedule dsa array :\n", host->host_no);
    for (left = host->can_queue, curr = hostdata->schedule;
	    left > 0; curr += 2, --left)
	if (curr[0] != hostdata->NOP_insn) 
/* FIXME : convert pointer to dsa_begin to pointer to dsa. */
	    print_dsa (host, bus_to_virt (le32_to_cpu(curr[1]) - 
		(hostdata->E_dsa_code_begin - 
		hostdata->E_dsa_code_template)), "");
    printk ("scsi%d : end schedule dsa array\n", host->host_no);
    
    printk ("scsi%d : reconnect_dsa_head :\n", host->host_no);
	    
    for (left = host->can_queue, 
	dsa = bus_to_virt (le32_to_cpu(hostdata->reconnect_dsa_head));
	left >= 0 && dsa; 
	dsa = next_dsa) {
	save_flags (flags);
	cli();
	if (check_address ((unsigned long) dsa, sizeof(dsa)) == -1) {
	    printk ("scsi%d: bad DSA pointer 0x%p", host->host_no,
		dsa);
	    next_dsa = NULL;
	}
	else 
	{
	    next_dsa = bus_to_virt(le32_to_cpu(dsa[hostdata->dsa_next / sizeof(u32)]));
	    print_dsa (host, dsa, "");
	}
	restore_flags(flags);
    }
    printk ("scsi%d : end reconnect_dsa_head\n", host->host_no);
    if (left < 0)
	printk("scsi%d: possible loop in ncr reconnect list\n",
	    host->host_no);
}

static void
print_lots (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = 
	(struct NCR53c7x0_hostdata *) host->hostdata;
    u32 *dsp_next, *dsp, *dsa, dbc_dcmd;
    unsigned char dcmd, sbcl;
    int i, size;
    NCR53c7x0_local_setup(host);

    if ((dsp_next = bus_to_virt(NCR53c7x0_read32 (DSP_REG)))) {
    	dbc_dcmd = NCR53c7x0_read32(DBC_REG);
    	dcmd = (dbc_dcmd & 0xff000000) >> 24;
    	dsp = dsp_next - NCR53c7x0_insn_size(dcmd);
	dsa = bus_to_virt(NCR53c7x0_read32(DSA_REG));
	sbcl = NCR53c7x0_read8 (SBCL_REG);
	    
	
    	printk ("scsi%d : DCMD|DBC=0x%x, DNAD=0x%x (virt 0x%p)\n"
		"         DSA=0x%lx (virt 0x%p)\n"
	        "         DSPS=0x%x, TEMP=0x%x (virt 0x%p), DMODE=0x%x\n"
		"         SXFER=0x%x, SCNTL3=0x%x\n"
		"         %s%s%sphase=%s, %d bytes in SCSI FIFO\n"
		"         STEST0=0x%x\n",
	    host->host_no, dbc_dcmd, NCR53c7x0_read32(DNAD_REG), 
		bus_to_virt(NCR53c7x0_read32(DNAD_REG)),
	    virt_to_bus(dsa), dsa, 
	    NCR53c7x0_read32(DSPS_REG), NCR53c7x0_read32(TEMP_REG), 
	    bus_to_virt (NCR53c7x0_read32(TEMP_REG)),
	    (int) NCR53c7x0_read8(hostdata->dmode),
	    (int) NCR53c7x0_read8(SXFER_REG), 
	    (int) NCR53c7x0_read8(SCNTL3_REG_800),
	    (sbcl & SBCL_BSY) ? "BSY " : "",
	    (sbcl & SBCL_SEL) ? "SEL " : "",
	    (sbcl & SBCL_REQ) ? "REQ " : "",
	    sstat2_to_phase(NCR53c7x0_read8 (((hostdata->chip / 100) == 8) ?
	    	SSTAT1_REG : SSTAT2_REG)),
	    (NCR53c7x0_read8 ((hostdata->chip / 100) == 8 ? 
		SSTAT1_REG : SSTAT2_REG) & SSTAT2_FF_MASK) >> SSTAT2_FF_SHIFT,
	    NCR53c7x0_read8 (STEST0_REG_800));
	printk ("scsi%d : DSP 0x%lx (virt 0x%p) ->\n", host->host_no, 
	    virt_to_bus(dsp), dsp);
    	for (i = 6; i > 0; --i, dsp += size)
	    size = print_insn (host, dsp, "", 1);
	if (NCR53c7x0_read8 (SCNTL1_REG) & SCNTL1_CON)  {
	    printk ("scsi%d : connected (SDID=0x%x, SSID=0x%x)\n",
		host->host_no, NCR53c7x0_read8 (SDID_REG_800),
		NCR53c7x0_read8 (SSID_REG_800));
	    print_dsa (host, dsa, "");
	}

#if 1
	print_queues (host);
#endif
    }
}

/*
 * Function : static int shutdown (struct Scsi_Host *host)
 * 
 * Purpose : does a clean (we hope) shutdown of the NCR SCSI 
 *	chip.  Use prior to dumping core, unloading the NCR driver,
 * 
 * Returns : 0 on success
 */
static int 
shutdown (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    unsigned long flags;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    NCR53c7x0_local_setup(host);
    save_flags (flags);
    cli();
/* Get in a state where we can reset the SCSI bus */
    ncr_halt (host);
    ncr_scsi_reset (host);
    hostdata->soft_reset(host);

    disable (host);
    restore_flags (flags);
    return 0;
}

/*
 * Function : void ncr_scsi_reset (struct Scsi_Host *host)
 *
 * Purpose : reset the SCSI bus.
 */

static void 
ncr_scsi_reset (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    unsigned long flags;
    int sien = 0;
    NCR53c7x0_local_setup(host);
    save_flags (flags);
    cli();
    if ((hostdata->chip / 100) == 8) {
	sien = NCR53c7x0_read8(SIEN0_REG_800);
	NCR53c7x0_write8(SIEN0_REG_800, sien & ~SIEN_RST);
    }
    NCR53c7x0_write8(SCNTL1_REG, SCNTL1_RST);
    udelay(25);	/* Minimum amount of time to assert RST */
    NCR53c7x0_write8(SCNTL1_REG, 0);
    if ((hostdata->chip / 100) == 8) {
	NCR53c7x0_write8(SIEN0_REG_800, sien);
    }
    restore_flags (flags);
}

/* 
 * Function : void hard_reset (struct Scsi_Host *host)
 *
 */

static void 
hard_reset (struct Scsi_Host *host) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    unsigned long flags;
    save_flags (flags);
    cli();
    ncr_scsi_reset(host);
    NCR53c7x0_driver_init (host);
    if (hostdata->soft_reset)
	hostdata->soft_reset (host);
    restore_flags(flags);
}


/*
 * Function : Scsi_Cmnd *return_outstanding_commands (struct Scsi_Host *host,
 *	int free, int issue)
 *
 * Purpose : return a linked list (using the SCp.buffer field as next,
 *	so we don't perturb hostdata.  We don't use a field of the 
 *	NCR53c7x0_cmd structure since we may not have allocated one 
 *	for the command causing the reset.) of Scsi_Cmnd structures that 
 *  	had propagated below the Linux issue queue level.  If free is set, 
 *	free the NCR53c7x0_cmd structures which are associated with 
 *	the Scsi_Cmnd structures, and clean up any internal 
 *	NCR lists that the commands were on.  If issue is set,
 *	also return commands in the issue queue.
 *
 * Returns : linked list of commands
 *
 * NOTE : the caller should insure that the NCR chip is halted
 *	if the free flag is set. 
 */

static Scsi_Cmnd *
return_outstanding_commands (struct Scsi_Host *host, int free, int issue) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    struct NCR53c7x0_cmd *c;
    int i;
    u32 *curr;
    Scsi_Cmnd *list = NULL, *tmp;
    for (c = (struct NCR53c7x0_cmd *) hostdata->running_list; c; 
    	c = (struct NCR53c7x0_cmd *) c->next)  {
	if (c->cmd->SCp.buffer) {
	    printk ("scsi%d : loop detected in running list!\n", host->host_no);
	    break;
	} else {
	    printk ("Duh? Bad things happening in the NCR driver\n");
	    break;
	}

	c->cmd->SCp.buffer = (struct scatterlist *) list;
	list = c->cmd;
	if (free) {
    	    c->next = hostdata->free;
    	    hostdata->free = c;
	}
    }

    if (free) { 
	for (i = 0, curr = (u32 *) hostdata->schedule; 
	    i < host->can_queue; ++i, curr += 2) {
	    curr[0] = hostdata->NOP_insn;
	    curr[1] = le32_to_cpu(0xdeadbeef);
	}
	hostdata->curr = NULL;
    }

    if (issue) {
	for (tmp = (Scsi_Cmnd *) hostdata->issue_queue; tmp; tmp = tmp->next) {
	    if (tmp->SCp.buffer) {
		printk ("scsi%d : loop detected in issue queue!\n", 
			host->host_no);
		break;
	    }
	    tmp->SCp.buffer = (struct scatterlist *) list;
	    list = tmp;
	}
	if (free)
	    hostdata->issue_queue = NULL;
		
    }
    return list;
}

/* 
 * Function : static int disable (struct Scsi_Host *host)
 *
 * Purpose : disables the given NCR host, causing all commands
 * 	to return a driver error.  Call this so we can unload the
 * 	module during development and try again.  Eventually, 
 * 	we should be able to find clean workarounds for these
 * 	problems.
 *
 * Inputs : host - hostadapter to twiddle
 *
 * Returns : 0 on success.
 */

static int 
disable (struct Scsi_Host *host) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    unsigned long flags;
    Scsi_Cmnd *nuke_list, *tmp;
    save_flags(flags);
    cli();
    if (hostdata->state != STATE_HALTED)
	ncr_halt (host);
    nuke_list = return_outstanding_commands (host, 1 /* free */, 1 /* issue */);
    hard_reset (host);
    hostdata->state = STATE_DISABLED;
    restore_flags(flags);
    printk ("scsi%d : nuking commands\n", host->host_no);
    for (; nuke_list; nuke_list = tmp) {
	    tmp = (Scsi_Cmnd *) nuke_list->SCp.buffer;
	    nuke_list->result = DID_ERROR << 16;
	    nuke_list->scsi_done(nuke_list);
    }
    printk ("scsi%d : done. \n", host->host_no);
    printk (KERN_ALERT "scsi%d : disabled.  Unload and reload\n",
    	host->host_no);
    return 0;
}

/*
 * Function : static int ncr_halt (struct Scsi_Host *host)
 * 
 * Purpose : halts the SCSI SCRIPTS(tm) processor on the NCR chip
 *
 * Inputs : host - SCSI chip to halt
 *
 * Returns : 0 on success
 */

static int 
ncr_halt (struct Scsi_Host *host) {
    NCR53c7x0_local_declare();
    unsigned long flags;
    unsigned char istat, tmp;
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    int stage;
    NCR53c7x0_local_setup(host);

    save_flags(flags);
    cli();
    /* Stage 0 : eat all interrupts
       Stage 1 : set ABORT
       Stage 2 : eat all but abort interrupts
       Stage 3 : eat all interrupts
     */
    for (stage = 0;;) {
	if (stage == 1) {
	    NCR53c7x0_write8(hostdata->istat, ISTAT_ABRT);
	    ++stage;
	}
	istat = NCR53c7x0_read8 (hostdata->istat);
	if (istat & ISTAT_SIP) {
	    if ((hostdata->chip / 100) == 8) {
		tmp = NCR53c7x0_read8(SIST0_REG_800);
		udelay(1);
		tmp = NCR53c7x0_read8(SIST1_REG_800);
	    } else {
		tmp = NCR53c7x0_read8(SSTAT0_REG);
	    }
	} else if (istat & ISTAT_DIP) {
	    tmp = NCR53c7x0_read8(DSTAT_REG);
	    if (stage == 2) {
		if (tmp & DSTAT_ABRT) {
		    NCR53c7x0_write8(hostdata->istat, 0);
		    ++stage;
		} else {
		    printk(KERN_ALERT "scsi%d : could not halt NCR chip\n", 
			host->host_no);
		    disable (host);
	    	}
    	    }
	}
	if (!(istat & (ISTAT_SIP|ISTAT_DIP))) {
	    if (stage == 0)
	    	++stage;
	    else if (stage == 3)
		break;
	}
    }
    hostdata->state = STATE_HALTED;
    restore_flags(flags);
#if 0
    print_lots (host);
#endif
    return 0;
}

/* 
 * Function: event_name (int event)
 * 
 * Purpose: map event enum into user-readable strings.
 */

static const char *
event_name (int event) {
    switch (event) {
    case EVENT_NONE:		return "none";
    case EVENT_ISSUE_QUEUE:	return "to issue queue";
    case EVENT_START_QUEUE:	return "to start queue";
    case EVENT_SELECT:		return "selected";
    case EVENT_DISCONNECT:	return "disconnected";
    case EVENT_RESELECT:	return "reselected";
    case EVENT_COMPLETE:	return "completed";
    case EVENT_IDLE:		return "idle";
    case EVENT_SELECT_FAILED:	return "select failed";
    case EVENT_BEFORE_SELECT:	return "before select";
    case EVENT_RESELECT_FAILED:	return "reselect failed";
    default:			return "unknown";
    }
}

/*
 * Function : void dump_events (struct Scsi_Host *host, count)
 *
 * Purpose : print last count events which have occurred.
 */ 
static void
dump_events (struct Scsi_Host *host, int count) {
    struct NCR53c7x0_hostdata *hostdata = (struct NCR53c7x0_hostdata *)
	host->hostdata;
    struct NCR53c7x0_event event;
    int i;
    unsigned long flags;
    if (hostdata->events) {
	if (count > hostdata->event_size)
	    count = hostdata->event_size;
	for (i = hostdata->event_index; count > 0; 
	    i = (i ? i - 1 : hostdata->event_size -1), --count) {
	    save_flags(flags);
/*
 * By copying the event we're currently examining with interrupts
 * disabled, we can do multiple printk(), etc. operations and 
 * still be guaranteed that they're happening on the same 
 * event structure.
 */
	    cli();
#if 0
	    event = hostdata->events[i];
#else
	    memcpy ((void *) &event, (void *) &(hostdata->events[i]),
		sizeof(event));
#endif

	    restore_flags(flags);
	    printk ("scsi%d : %s event %d at %ld secs %ld usecs target %d lun %d\n",
		host->host_no, event_name (event.event), count,
		(long) event.time.tv_sec, (long) event.time.tv_usec,
		event.target, event.lun);
	    if (event.dsa) 
		printk ("         event for dsa 0x%lx (virt 0x%p)\n", 
		    virt_to_bus(event.dsa), event.dsa);
	    if (event.pid != -1) {
		printk ("         event for pid %ld ", event.pid);
		print_command (event.cmnd);
	    }
	}
    }
}

/*
 * Function: check_address
 *
 * Purpose: Check to see if a possibly corrupt pointer will fault the 
 *	kernel.
 *
 * Inputs: addr - address; size - size of area
 *
 * Returns: 0 if area is OK, -1 on error.
 *
 * NOTES: should be implemented in terms of vverify on kernels 
 *	that have it.
 */

static int 
check_address (unsigned long addr, int size) {
    return (virt_to_phys((void *)addr) < PAGE_SIZE || virt_to_phys((void *)(addr + size)) > virt_to_phys(high_memory) ?  -1 : 0);
}

#ifdef MODULE
int 
NCR53c7x0_release(struct Scsi_Host *host) {
    struct NCR53c7x0_hostdata *hostdata = 
	(struct NCR53c7x0_hostdata *) host->hostdata;
    struct NCR53c7x0_cmd *cmd, *tmp;
    shutdown (host);
    if (host->irq != SCSI_IRQ_NONE)
	{
	    int irq_count;
	    struct Scsi_Host *tmp;
	    for (irq_count = 0, tmp = first_host; tmp; tmp = tmp->next)
		if (tmp->hostt == the_template && tmp->irq == host->irq)
		    ++irq_count;
	    if (irq_count == 1)
		free_irq(host->irq, NULL);
	}
    if (host->dma_channel != DMA_NONE)
	free_dma(host->dma_channel);
    if (host->io_port)
	release_region(host->io_port, host->n_io_port);
    
    for (cmd = (struct NCR53c7x0_cmd *) hostdata->free; cmd; cmd = tmp, 
	--hostdata->num_cmds) {
	tmp = (struct NCR53c7x0_cmd *) cmd->next;
    /* 
     * If we're going to loop, try to stop it to get a more accurate
     * count of the leaked commands.
     */
	cmd->next = NULL;
	if (cmd->free)
	    cmd->free ((void *) cmd->real, cmd->size);
    }
    if (hostdata->num_cmds)
	printk ("scsi%d : leaked %d NCR53c7x0_cmd structures\n",
	    host->host_no, hostdata->num_cmds);
    if (hostdata->events) 
	vfree ((void *)hostdata->events);
    return 1;
}
#endif /* def MODULE */
MODULE_LICENSE("GPL");

static Scsi_Host_Template driver_template = NCR53c7xx;
#include "scsi_module.c"
