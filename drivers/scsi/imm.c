/* imm.c   --  low level driver for the IOMEGA MatchMaker
 * parallel port SCSI host adapter.
 * 
 * (The IMM is the embedded controller in the ZIP Plus drive.)
 * 
 * Current Maintainer: David Campbell (Perth, Western Australia)
 *                     campbell@torque.net
 *
 * My unoffical company acronym list is 21 pages long:
 *      FLA:    Four letter acronym with built in facility for
 *              future expansion to five letters.
 */

#include <linux/config.h>

/* The following #define is to avoid a clash with hosts.c */
#define IMM_CODE 1
#define IMM_PROBE_SPP   0x0001
#define IMM_PROBE_PS2   0x0002
#define IMM_PROBE_ECR   0x0010
#define IMM_PROBE_EPP17 0x0100
#define IMM_PROBE_EPP19 0x0200

void imm_reset_pulse(unsigned int base);
static int device_check(int host_no);

#include <linux/blk.h>
#include <asm/io.h>
#include <linux/parport.h>
#include "sd.h"
#include "hosts.h"
typedef struct {
    struct pardevice *dev;	/* Parport device entry         */
    int base;			/* Actual port address          */
    int base_hi;		/* Hi Base address for ECP-ISA chipset */
    int mode;			/* Transfer mode                */
    int host;			/* Host number (for proc)       */
    Scsi_Cmnd *cur_cmd;		/* Current queued command       */
    struct tq_struct imm_tq;	/* Polling interrupt stuff       */
    unsigned long jstart;	/* Jiffies at start             */
    unsigned failed:1;		/* Failure flag                 */
    unsigned dp:1;		/* Data phase present           */
    unsigned rd:1;		/* Read data in data phase      */
    unsigned p_busy:1;		/* Parport sharing busy flag    */
} imm_struct;

#define IMM_EMPTY \
{	dev:		NULL,		\
	base:		-1,		\
	base_hi:	0,		\
	mode:		IMM_AUTODETECT,	\
	host:		-1,		\
	cur_cmd:	NULL,		\
	imm_tq:		{ routine: imm_interrupt },    \
	jstart:		0,		\
	failed:		0,		\
	dp:		0,		\
	rd:		0,		\
	p_busy:		0		\
}

#include "imm.h"
#define NO_HOSTS 4
static imm_struct imm_hosts[NO_HOSTS] =
{IMM_EMPTY, IMM_EMPTY, IMM_EMPTY, IMM_EMPTY};

#define IMM_BASE(x)	imm_hosts[(x)].base
#define IMM_BASE_HI(x)     imm_hosts[(x)].base_hi

int parbus_base[NO_HOSTS] =
{0x03bc, 0x0378, 0x0278, 0x0000};

void imm_wakeup(void *ref)
{
    imm_struct *imm_dev = (imm_struct *) ref;

    if (!imm_dev->p_busy)
	return;

    if (parport_claim(imm_dev->dev)) {
	printk("imm: bug in imm_wakeup\n");
	return;
    }
    imm_dev->p_busy = 0;
    imm_dev->base = imm_dev->dev->port->base;
    if (imm_dev->cur_cmd)
	imm_dev->cur_cmd->SCp.phase++;
    return;
}

int imm_release(struct Scsi_Host *host)
{
    int host_no = host->unique_id;

    printk("Releasing imm%i\n", host_no);
    parport_unregister_device(imm_hosts[host_no].dev);
    return 0;
}

static int imm_pb_claim(int host_no)
{
    if (parport_claim(imm_hosts[host_no].dev)) {
	imm_hosts[host_no].p_busy = 1;
	return 1;
    }
    if (imm_hosts[host_no].cur_cmd)
	imm_hosts[host_no].cur_cmd->SCp.phase++;
    return 0;
}

#define imm_pb_release(x) parport_release(imm_hosts[(x)].dev)

/***************************************************************************
 *                   Parallel port probing routines                        *
 ***************************************************************************/

static Scsi_Host_Template driver_template = IMM;
#include  "scsi_module.c"

int imm_detect(Scsi_Host_Template * host)
{
    struct Scsi_Host *hreg;
    int ports;
    int i, nhosts, try_again;
    struct parport *pb;

    /*
     * unlock to allow the lowlevel parport driver to probe
     * the irqs
     */
    spin_unlock_irq(&io_request_lock);
    pb = parport_enumerate();

    printk("imm: Version %s\n", IMM_VERSION);
    nhosts = 0;
    try_again = 0;

    if (!pb) {
	printk("imm: parport reports no devices.\n");
	spin_lock_irq(&io_request_lock);
	return 0;
    }
  retry_entry:
    for (i = 0; pb; i++, pb = pb->next) {
	int modes, ppb;

	imm_hosts[i].dev =
	    parport_register_device(pb, "imm", NULL, imm_wakeup,
				    NULL, 0, (void *) &imm_hosts[i]);

	if (!imm_hosts[i].dev)
	    continue;

	/* Claim the bus so it remembers what we do to the control
	 * registers. [ CTR and ECP ]
	 */
	if (imm_pb_claim(i)) {
	    unsigned long now = jiffies;
	    while (imm_hosts[i].p_busy) {
		schedule();	/* We are safe to schedule here */
		if (time_after(jiffies, now + 3 * HZ)) {
		    printk(KERN_ERR "imm%d: failed to claim parport because a "
		      "pardevice is owning the port for too longtime!\n",
			   i);
		    parport_unregister_device (imm_hosts[i].dev);
		    spin_lock_irq(&io_request_lock);
		    return 0;
		}
	    }
	}
	ppb = IMM_BASE(i) = imm_hosts[i].dev->port->base;
	IMM_BASE_HI(i) = imm_hosts[i].dev->port->base_hi;
	w_ctr(ppb, 0x0c);
	modes = imm_hosts[i].dev->port->modes;

	/* Mode detection works up the chain of speed
	 * This avoids a nasty if-then-else-if-... tree
	 */
	imm_hosts[i].mode = IMM_NIBBLE;

	if (modes & PARPORT_MODE_TRISTATE)
	    imm_hosts[i].mode = IMM_PS2;

	/* Done configuration */
	imm_pb_release(i);

	if (imm_init(i)) {
	    parport_unregister_device(imm_hosts[i].dev);
	    continue;
	}
	/* now the glue ... */
	switch (imm_hosts[i].mode) {
	case IMM_NIBBLE:
	    ports = 3;
	    break;
	case IMM_PS2:
	    ports = 3;
	    break;
	case IMM_EPP_8:
	case IMM_EPP_16:
	case IMM_EPP_32:
	    ports = 8;
	    break;
	default:		/* Never gets here */
	    continue;
	}

	host->can_queue = IMM_CAN_QUEUE;
	host->sg_tablesize = imm_sg;
	hreg = scsi_register(host, 0);
	if(hreg == NULL)
		continue;
	hreg->io_port = pb->base;
	hreg->n_io_port = ports;
	hreg->dma_channel = -1;
	hreg->unique_id = i;
	imm_hosts[i].host = hreg->host_no;
	nhosts++;
    }
    if (nhosts == 0) {
	if (try_again == 1) {
	    spin_lock_irq(&io_request_lock);
	    return 0;
	}
	try_again = 1;
	goto retry_entry;
    } else {
	spin_lock_irq (&io_request_lock);
	return 1;		/* return number of hosts detected */
    }
}

/* This is to give the imm driver a way to modify the timings (and other
 * parameters) by writing to the /proc/scsi/imm/0 file.
 * Very simple method really... (To simple, no error checking :( )
 * Reason: Kernel hackers HATE having to unload and reload modules for
 * testing...
 * Also gives a method to use a script to obtain optimum timings (TODO)
 */
static inline int imm_proc_write(int hostno, char *buffer, int length)
{
    unsigned long x;

    if ((length > 5) && (strncmp(buffer, "mode=", 5) == 0)) {
	x = simple_strtoul(buffer + 5, NULL, 0);
	imm_hosts[hostno].mode = x;
	return length;
    }
    printk("imm /proc: invalid variable\n");
    return (-EINVAL);
}

int imm_proc_info(char *buffer, char **start, off_t offset,
		  int length, int hostno, int inout)
{
    int i;
    int len = 0;

    for (i = 0; i < 4; i++)
	if (imm_hosts[i].host == hostno)
	    break;

    if (inout)
	return imm_proc_write(i, buffer, length);

    len += sprintf(buffer + len, "Version : %s\n", IMM_VERSION);
    len += sprintf(buffer + len, "Parport : %s\n", imm_hosts[i].dev->port->name);
    len += sprintf(buffer + len, "Mode    : %s\n", IMM_MODE_STRING[imm_hosts[i].mode]);

    /* Request for beyond end of buffer */
    if (offset > len)
	return 0;

    *start = buffer + offset;
    len -= offset;
    if (len > length)
	len = length;
    return len;
}

#if IMM_DEBUG > 0
#define imm_fail(x,y) printk("imm: imm_fail(%i) from %s at line %d\n",\
	   y, __FUNCTION__, __LINE__); imm_fail_func(x,y);
static inline void imm_fail_func(int host_no, int error_code)
#else
static inline void imm_fail(int host_no, int error_code)
#endif
{
    /* If we fail a device then we trash status / message bytes */
    if (imm_hosts[host_no].cur_cmd) {
	imm_hosts[host_no].cur_cmd->result = error_code << 16;
	imm_hosts[host_no].failed = 1;
    }
}

/*
 * Wait for the high bit to be set.
 * 
 * In principle, this could be tied to an interrupt, but the adapter
 * doesn't appear to be designed to support interrupts.  We spin on
 * the 0x80 ready bit. 
 */
static unsigned char imm_wait(int host_no)
{
    int k;
    unsigned short ppb = IMM_BASE(host_no);
    unsigned char r;

    w_ctr(ppb, 0x0c);

    k = IMM_SPIN_TMO;
    do {
	r = r_str(ppb);
	k--;
	udelay(1);
    }
    while (!(r & 0x80) && (k));

    /*
     * STR register (LPT base+1) to SCSI mapping:
     *
     * STR      imm     imm
     * ===================================
     * 0x80     S_REQ   S_REQ
     * 0x40     !S_BSY  (????)
     * 0x20     !S_CD   !S_CD
     * 0x10     !S_IO   !S_IO
     * 0x08     (????)  !S_BSY
     *
     * imm      imm     meaning
     * ==================================
     * 0xf0     0xb8    Bit mask
     * 0xc0     0x88    ZIP wants more data
     * 0xd0     0x98    ZIP wants to send more data
     * 0xe0     0xa8    ZIP is expecting SCSI command data
     * 0xf0     0xb8    end of transfer, ZIP is sending status
     */
    w_ctr(ppb, 0x04);
    if (k)
	return (r & 0xb8);

    /* Counter expired - Time out occurred */
    imm_fail(host_no, DID_TIME_OUT);
    printk("imm timeout in imm_wait\n");
    return 0;			/* command timed out */
}

static int imm_negotiate(imm_struct * tmp)
{
    /*
     * The following is supposedly the IEEE 1284-1994 negotiate
     * sequence. I have yet to obtain a copy of the above standard
     * so this is a bit of a guess...
     *
     * A fair chunk of this is based on the Linux parport implementation
     * of IEEE 1284.
     *
     * Return 0 if data available
     *        1 if no data available
     */

    unsigned short base = tmp->base;
    unsigned char a, mode;

    switch (tmp->mode) {
    case IMM_NIBBLE:
	mode = 0x00;
	break;
    case IMM_PS2:
	mode = 0x01;
	break;
    default:
	return 0;
    }

    w_ctr(base, 0x04);
    udelay(5);
    w_dtr(base, mode);
    udelay(100);
    w_ctr(base, 0x06);
    udelay(5);
    a = (r_str(base) & 0x20) ? 0 : 1;
    udelay(5);
    w_ctr(base, 0x07);
    udelay(5);
    w_ctr(base, 0x06);

    if (a) {
	printk("IMM: IEEE1284 negotiate indicates no data available.\n");
	imm_fail(tmp->host, DID_ERROR);
    }
    return a;
}

/* 
 * Clear EPP timeout bit. 
 */
static inline void epp_reset(unsigned short ppb)
{
    int i;

    i = r_str(ppb);
    w_str(ppb, i);
    w_str(ppb, i & 0xfe);
}

/* 
 * Wait for empty ECP fifo (if we are in ECP fifo mode only)
 */
static inline void ecp_sync(unsigned short hostno)
{
    int i, ppb_hi=IMM_BASE_HI(hostno);

    if (ppb_hi == 0) return;

    if ((r_ecr(ppb_hi) & 0xe0) == 0x60) { /* mode 011 == ECP fifo mode */
        for (i = 0; i < 100; i++) {
	    if (r_ecr(ppb_hi) & 0x01)
	        return;
	    udelay(5);
	}
        printk("imm: ECP sync failed as data still present in FIFO.\n");
    }
}

static int imm_byte_out(unsigned short base, const char *buffer, int len)
{
    int i;

    w_ctr(base, 0x4);		/* apparently a sane mode */
    for (i = len >> 1; i; i--) {
	w_dtr(base, *buffer++);
	w_ctr(base, 0x5);	/* Drop STROBE low */
	w_dtr(base, *buffer++);
	w_ctr(base, 0x0);	/* STROBE high + INIT low */
    }
    w_ctr(base, 0x4);		/* apparently a sane mode */
    return 1;			/* All went well - we hope! */
}

static int imm_nibble_in(unsigned short base, char *buffer, int len)
{
    unsigned char l;
    int i;

    /*
     * The following is based on documented timing signals
     */
    w_ctr(base, 0x4);
    for (i = len; i; i--) {
	w_ctr(base, 0x6);
	l = (r_str(base) & 0xf0) >> 4;
	w_ctr(base, 0x5);
	*buffer++ = (r_str(base) & 0xf0) | l;
	w_ctr(base, 0x4);
    }
    return 1;			/* All went well - we hope! */
}

static int imm_byte_in(unsigned short base, char *buffer, int len)
{
    int i;

    /*
     * The following is based on documented timing signals
     */
    w_ctr(base, 0x4);
    for (i = len; i; i--) {
	w_ctr(base, 0x26);
	*buffer++ = r_dtr(base);
	w_ctr(base, 0x25);
    }
    return 1;			/* All went well - we hope! */
}

static int imm_out(int host_no, char *buffer, int len)
{
    int r;
    unsigned short ppb = IMM_BASE(host_no);

    r = imm_wait(host_no);

    /*
     * Make sure that:
     * a) the SCSI bus is BUSY (device still listening)
     * b) the device is listening
     */
    if ((r & 0x18) != 0x08) {
	imm_fail(host_no, DID_ERROR);
	printk("IMM: returned SCSI status %2x\n", r);
	return 0;
    }
    switch (imm_hosts[host_no].mode) {
    case IMM_EPP_32:
    case IMM_EPP_16:
    case IMM_EPP_8:
	epp_reset(ppb);
	w_ctr(ppb, 0x4);
#ifdef CONFIG_SCSI_IZIP_EPP16
	if (!(((long) buffer | len) & 0x01))
	    outsw(ppb + 4, buffer, len >> 1);
#else
	if (!(((long) buffer | len) & 0x03))
	    outsl(ppb + 4, buffer, len >> 2);
#endif
	else
	    outsb(ppb + 4, buffer, len);
	w_ctr(ppb, 0xc);
	r = !(r_str(ppb) & 0x01);
	w_ctr(ppb, 0xc);
	ecp_sync(host_no);
	break;

    case IMM_NIBBLE:
    case IMM_PS2:
	/* 8 bit output, with a loop */
	r = imm_byte_out(ppb, buffer, len);
	break;

    default:
	printk("IMM: bug in imm_out()\n");
	r = 0;
    }
    return r;
}

static int imm_in(int host_no, char *buffer, int len)
{
    int r;
    unsigned short ppb = IMM_BASE(host_no);

    r = imm_wait(host_no);

    /*
     * Make sure that:
     * a) the SCSI bus is BUSY (device still listening)
     * b) the device is sending data
     */
    if ((r & 0x18) != 0x18) {
	imm_fail(host_no, DID_ERROR);
	return 0;
    }
    switch (imm_hosts[host_no].mode) {
    case IMM_NIBBLE:
	/* 4 bit input, with a loop */
	r = imm_nibble_in(ppb, buffer, len);
	w_ctr(ppb, 0xc);
	break;

    case IMM_PS2:
	/* 8 bit input, with a loop */
	r = imm_byte_in(ppb, buffer, len);
	w_ctr(ppb, 0xc);
	break;

    case IMM_EPP_32:
    case IMM_EPP_16:
    case IMM_EPP_8:
	epp_reset(ppb);
	w_ctr(ppb, 0x24);
#ifdef CONFIG_SCSI_IZIP_EPP16
	if (!(((long) buffer | len) & 0x01))
	    insw(ppb + 4, buffer, len >> 1);
#else
	if (!(((long) buffer | len) & 0x03))
	    insl(ppb + 4, buffer, len >> 2);
#endif
	else
	    insb(ppb + 4, buffer, len);
	w_ctr(ppb, 0x2c);
	r = !(r_str(ppb) & 0x01);
	w_ctr(ppb, 0x2c);
	ecp_sync(host_no);
	break;

    default:
	printk("IMM: bug in imm_ins()\n");
	r = 0;
	break;
    }
    return r;
}

static int imm_cpp(unsigned short ppb, unsigned char b)
{
    /*
     * Comments on udelay values refer to the
     * Command Packet Protocol (CPP) timing diagram.
     */

    unsigned char s1, s2, s3;
    w_ctr(ppb, 0x0c);
    udelay(2);			/* 1 usec - infinite */
    w_dtr(ppb, 0xaa);
    udelay(10);			/* 7 usec - infinite */
    w_dtr(ppb, 0x55);
    udelay(10);			/* 7 usec - infinite */
    w_dtr(ppb, 0x00);
    udelay(10);			/* 7 usec - infinite */
    w_dtr(ppb, 0xff);
    udelay(10);			/* 7 usec - infinite */
    s1 = r_str(ppb) & 0xb8;
    w_dtr(ppb, 0x87);
    udelay(10);			/* 7 usec - infinite */
    s2 = r_str(ppb) & 0xb8;
    w_dtr(ppb, 0x78);
    udelay(10);			/* 7 usec - infinite */
    s3 = r_str(ppb) & 0x38;
    /*
     * Values for b are:
     * 0000 00aa    Assign address aa to current device
     * 0010 00aa    Select device aa in EPP Winbond mode
     * 0010 10aa    Select device aa in EPP mode
     * 0011 xxxx    Deselect all devices
     * 0110 00aa    Test device aa
     * 1101 00aa    Select device aa in ECP mode
     * 1110 00aa    Select device aa in Compatible mode
     */
    w_dtr(ppb, b);
    udelay(2);			/* 1 usec - infinite */
    w_ctr(ppb, 0x0c);
    udelay(10);			/* 7 usec - infinite */
    w_ctr(ppb, 0x0d);
    udelay(2);			/* 1 usec - infinite */
    w_ctr(ppb, 0x0c);
    udelay(10);			/* 7 usec - infinite */
    w_dtr(ppb, 0xff);
    udelay(10);			/* 7 usec - infinite */

    /*
     * The following table is electrical pin values.
     * (BSY is inverted at the CTR register)
     *
     *       BSY  ACK  POut SEL  Fault
     * S1    0    X    1    1    1
     * S2    1    X    0    1    1
     * S3    L    X    1    1    S
     *
     * L => Last device in chain
     * S => Selected
     *
     * Observered values for S1,S2,S3 are:
     * Disconnect => f8/58/78
     * Connect    => f8/58/70
     */
    if ((s1 == 0xb8) && (s2 == 0x18) && (s3 == 0x30))
	return 1;		/* Connected */
    if ((s1 == 0xb8) && (s2 == 0x18) && (s3 == 0x38))
	return 0;		/* Disconnected */

    return -1;			/* No device present */
}

static inline int imm_connect(int host_no, int flag)
{
    unsigned short ppb = IMM_BASE(host_no);

    imm_cpp(ppb, 0xe0);		/* Select device 0 in compatible mode */
    imm_cpp(ppb, 0x30);		/* Disconnect all devices */

    if ((imm_hosts[host_no].mode == IMM_EPP_8) ||
	(imm_hosts[host_no].mode == IMM_EPP_16) ||
	(imm_hosts[host_no].mode == IMM_EPP_32))
	return imm_cpp(ppb, 0x28);	/* Select device 0 in EPP mode */
    return imm_cpp(ppb, 0xe0);	/* Select device 0 in compatible mode */
}

static void imm_disconnect(int host_no)
{
    unsigned short ppb = IMM_BASE(host_no);

    imm_cpp(ppb, 0x30);		/* Disconnect all devices */
}

static int imm_select(int host_no, int target)
{
    int k;
    unsigned short ppb = IMM_BASE(host_no);

    /*
     * Firstly we want to make sure there is nothing
     * holding onto the SCSI bus.
     */
    w_ctr(ppb, 0xc);

    k = IMM_SELECT_TMO;
    do {
	k--;
    } while ((r_str(ppb) & 0x08) && (k));

    if (!k)
	return 0;

    /*
     * Now assert the SCSI ID (HOST and TARGET) on the data bus
     */
    w_ctr(ppb, 0x4);
    w_dtr(ppb, 0x80 | (1 << target));
    udelay(1);

    /*
     * Deassert SELIN first followed by STROBE
     */
    w_ctr(ppb, 0xc);
    w_ctr(ppb, 0xd);

    /*
     * ACK should drop low while SELIN is deasserted.
     * FAULT should drop low when the SCSI device latches the bus.
     */
    k = IMM_SELECT_TMO;
    do {
	k--;
    }
    while (!(r_str(ppb) & 0x08) && (k));

    /*
     * Place the interface back into a sane state (status mode)
     */
    w_ctr(ppb, 0xc);
    return (k) ? 1 : 0;
}

static int imm_init(int host_no)
{
    int retv;

#if defined(CONFIG_PARPORT) || defined(CONFIG_PARPORT_MODULE)
    if (imm_pb_claim(host_no))
	while (imm_hosts[host_no].p_busy)
	    schedule();		/* We can safe schedule here */
#endif
    retv = imm_connect(host_no, 0);

    if (retv == 1) {
	imm_reset_pulse(IMM_BASE(host_no));
	udelay(1000);		/* Delay to allow devices to settle */
	imm_disconnect(host_no);
	udelay(1000);		/* Another delay to allow devices to settle */
	retv = device_check(host_no);
	imm_pb_release(host_no);
	return retv;
    }
    imm_pb_release(host_no);
    return 1;
}

static inline int imm_send_command(Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;
    int k;

    /* NOTE: IMM uses byte pairs */
    for (k = 0; k < cmd->cmd_len; k += 2)
	if (!imm_out(host_no, &cmd->cmnd[k], 2))
	    return 0;
    return 1;
}

/*
 * The bulk flag enables some optimisations in the data transfer loops,
 * it should be true for any command that transfers data in integral
 * numbers of sectors.
 * 
 * The driver appears to remain stable if we speed up the parallel port
 * i/o in this function, but not elsewhere.
 */
static int imm_completion(Scsi_Cmnd * cmd)
{
    /* Return codes:
     * -1     Error
     *  0     Told to schedule
     *  1     Finished data transfer
     */
    int host_no = cmd->host->unique_id;
    unsigned short ppb = IMM_BASE(host_no);
    unsigned long start_jiffies = jiffies;

    unsigned char r, v;
    int fast, bulk, status;

    v = cmd->cmnd[0];
    bulk = ((v == READ_6) ||
	    (v == READ_10) ||
	    (v == WRITE_6) ||
	    (v == WRITE_10));

    /*
     * We only get here if the drive is ready to comunicate,
     * hence no need for a full imm_wait.
     */
    w_ctr(ppb, 0x0c);
    r = (r_str(ppb) & 0xb8);

    /*
     * while (device is not ready to send status byte)
     *     loop;
     */
    while (r != (unsigned char) 0xb8) {
	/*
	 * If we have been running for more than a full timer tick
	 * then take a rest.
	 */
	if (time_after(jiffies, start_jiffies + 1))
	    return 0;

	/*
	 * FAIL if:
	 * a) Drive status is screwy (!ready && !present)
	 * b) Drive is requesting/sending more data than expected
	 */
	if (((r & 0x88) != 0x88) || (cmd->SCp.this_residual <= 0)) {
	    imm_fail(host_no, DID_ERROR);
	    return -1;		/* ERROR_RETURN */
	}
	/* determine if we should use burst I/O */
	if (imm_hosts[host_no].rd == 0) {
	    fast = (bulk && (cmd->SCp.this_residual >= IMM_BURST_SIZE)) ? IMM_BURST_SIZE : 2;
	    status = imm_out(host_no, cmd->SCp.ptr, fast);
	} else {
	    fast = (bulk && (cmd->SCp.this_residual >= IMM_BURST_SIZE)) ? IMM_BURST_SIZE : 1;
	    status = imm_in(host_no, cmd->SCp.ptr, fast);
	}

	cmd->SCp.ptr += fast;
	cmd->SCp.this_residual -= fast;

	if (!status) {
	    imm_fail(host_no, DID_BUS_BUSY);
	    return -1;		/* ERROR_RETURN */
	}
	if (cmd->SCp.buffer && !cmd->SCp.this_residual) {
	    /* if scatter/gather, advance to the next segment */
	    if (cmd->SCp.buffers_residual--) {
		cmd->SCp.buffer++;
		cmd->SCp.this_residual = cmd->SCp.buffer->length;
		cmd->SCp.ptr = cmd->SCp.buffer->address;

		/*
		 * Make sure that we transfer even number of bytes
		 * otherwise it makes imm_byte_out() messy.
		 */
		if (cmd->SCp.this_residual & 0x01)
		    cmd->SCp.this_residual++;
	    }
	}
	/* Now check to see if the drive is ready to comunicate */
	w_ctr(ppb, 0x0c);
	r = (r_str(ppb) & 0xb8);

	/* If not, drop back down to the scheduler and wait a timer tick */
	if (!(r & 0x80))
	    return 0;
    }
    return 1;			/* FINISH_RETURN */
}

/* deprecated synchronous interface */
int imm_command(Scsi_Cmnd * cmd)
{
    static int first_pass = 1;
    int host_no = cmd->host->unique_id;

    if (first_pass) {
	printk("imm: using non-queuing interface\n");
	first_pass = 0;
    }
    if (imm_hosts[host_no].cur_cmd) {
	printk("IMM: bug in imm_command\n");
	return 0;
    }
    imm_hosts[host_no].failed = 0;
    imm_hosts[host_no].jstart = jiffies;
    imm_hosts[host_no].cur_cmd = cmd;
    cmd->result = DID_ERROR << 16;	/* default return code */
    cmd->SCp.phase = 0;

    imm_pb_claim(host_no);

    while (imm_engine(&imm_hosts[host_no], cmd))
	schedule();

    if (cmd->SCp.phase)		/* Only disconnect if we have connected */
	imm_disconnect(cmd->host->unique_id);

    imm_pb_release(host_no);
    imm_hosts[host_no].cur_cmd = 0;
    return cmd->result;
}

/*
 * Since the IMM itself doesn't generate interrupts, we use
 * the scheduler's task queue to generate a stream of call-backs and
 * complete the request when the drive is ready.
 */
static void imm_interrupt(void *data)
{
    imm_struct *tmp = (imm_struct *) data;
    Scsi_Cmnd *cmd = tmp->cur_cmd;
    unsigned long flags;

    if (!cmd) {
	printk("IMM: bug in imm_interrupt\n");
	return;
    }
    if (imm_engine(tmp, cmd)) {
	tmp->imm_tq.data = (void *) tmp;
	tmp->imm_tq.sync = 0;
	queue_task(&tmp->imm_tq, &tq_timer);
	return;
    }
    /* Command must of completed hence it is safe to let go... */
#if IMM_DEBUG > 0
    switch ((cmd->result >> 16) & 0xff) {
    case DID_OK:
	break;
    case DID_NO_CONNECT:
	printk("imm: no device at SCSI ID %i\n", cmd->target);
	break;
    case DID_BUS_BUSY:
	printk("imm: BUS BUSY - EPP timeout detected\n");
	break;
    case DID_TIME_OUT:
	printk("imm: unknown timeout\n");
	break;
    case DID_ABORT:
	printk("imm: told to abort\n");
	break;
    case DID_PARITY:
	printk("imm: parity error (???)\n");
	break;
    case DID_ERROR:
	printk("imm: internal driver error\n");
	break;
    case DID_RESET:
	printk("imm: told to reset device\n");
	break;
    case DID_BAD_INTR:
	printk("imm: bad interrupt (???)\n");
	break;
    default:
	printk("imm: bad return code (%02x)\n", (cmd->result >> 16) & 0xff);
    }
#endif

    if (cmd->SCp.phase > 1)
	imm_disconnect(cmd->host->unique_id);
    if (cmd->SCp.phase > 0)
	imm_pb_release(cmd->host->unique_id);

    spin_lock_irqsave(&io_request_lock, flags);
    tmp->cur_cmd = 0;
    cmd->scsi_done(cmd);
    spin_unlock_irqrestore(&io_request_lock, flags);
    return;
}

static int imm_engine(imm_struct * tmp, Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;
    unsigned short ppb = IMM_BASE(host_no);
    unsigned char l = 0, h = 0;
    int retv, x;

    /* First check for any errors that may of occurred
     * Here we check for internal errors
     */
    if (tmp->failed)
	return 0;

    switch (cmd->SCp.phase) {
    case 0:			/* Phase 0 - Waiting for parport */
	if ((jiffies - tmp->jstart) > HZ) {
	    /*
	     * We waited more than a second
	     * for parport to call us
	     */
	    imm_fail(host_no, DID_BUS_BUSY);
	    return 0;
	}
	return 1;		/* wait until imm_wakeup claims parport */
	/* Phase 1 - Connected */
    case 1:
	imm_connect(host_no, CONNECT_EPP_MAYBE);
	cmd->SCp.phase++;

	/* Phase 2 - We are now talking to the scsi bus */
    case 2:
	if (!imm_select(host_no, cmd->target)) {
	    imm_fail(host_no, DID_NO_CONNECT);
	    return 0;
	}
	cmd->SCp.phase++;

	/* Phase 3 - Ready to accept a command */
    case 3:
	w_ctr(ppb, 0x0c);
	if (!(r_str(ppb) & 0x80))
	    return 1;

	if (!imm_send_command(cmd))
	    return 0;
	cmd->SCp.phase++;

	/* Phase 4 - Setup scatter/gather buffers */
    case 4:
	if (cmd->use_sg) {
	    /* if many buffers are available, start filling the first */
	    cmd->SCp.buffer = (struct scatterlist *) cmd->request_buffer;
	    cmd->SCp.this_residual = cmd->SCp.buffer->length;
	    cmd->SCp.ptr = cmd->SCp.buffer->address;
	} else {
	    /* else fill the only available buffer */
	    cmd->SCp.buffer = NULL;
	    cmd->SCp.this_residual = cmd->request_bufflen;
	    cmd->SCp.ptr = cmd->request_buffer;
	}
	cmd->SCp.buffers_residual = cmd->use_sg;
	cmd->SCp.phase++;
	if (cmd->SCp.this_residual & 0x01)
	    cmd->SCp.this_residual++;
	/* Phase 5 - Pre-Data transfer stage */
    case 5:
	/* Spin lock for BUSY */
	w_ctr(ppb, 0x0c);
	if (!(r_str(ppb) & 0x80))
	    return 1;

	/* Require negotiation for read requests */
	x = (r_str(ppb) & 0xb8);
	tmp->rd = (x & 0x10) ? 1 : 0;
	tmp->dp = (x & 0x20) ? 0 : 1;

	if ((tmp->dp) && (tmp->rd))
	    if (imm_negotiate(tmp))
		return 0;
	cmd->SCp.phase++;

	/* Phase 6 - Data transfer stage */
    case 6:
	/* Spin lock for BUSY */
	w_ctr(ppb, 0x0c);
	if (!(r_str(ppb) & 0x80))
	    return 1;

	if (tmp->dp) {
	    retv = imm_completion(cmd);
	    if (retv == -1)
		return 0;
	    if (retv == 0)
		return 1;
	}
	cmd->SCp.phase++;

	/* Phase 7 - Post data transfer stage */
    case 7:
	if ((tmp->dp) && (tmp->rd)) {
	    if ((tmp->mode == IMM_NIBBLE) || (tmp->mode == IMM_PS2)) {
		w_ctr(ppb, 0x4);
		w_ctr(ppb, 0xc);
		w_ctr(ppb, 0xe);
		w_ctr(ppb, 0x4);
	    }
	}
	cmd->SCp.phase++;

	/* Phase 8 - Read status/message */
    case 8:
	/* Check for data overrun */
	if (imm_wait(host_no) != (unsigned char) 0xb8) {
	    imm_fail(host_no, DID_ERROR);
	    return 0;
	}
	if (imm_negotiate(tmp))
	    return 0;
	if (imm_in(host_no, &l, 1)) {	/* read status byte */
	    /* Check for optional message byte */
	    if (imm_wait(host_no) == (unsigned char) 0xb8)
		imm_in(host_no, &h, 1);
	    cmd->result = (DID_OK << 16) + (l & STATUS_MASK);
	}
	if ((tmp->mode == IMM_NIBBLE) || (tmp->mode == IMM_PS2)) {
	    w_ctr(ppb, 0x4);
	    w_ctr(ppb, 0xc);
	    w_ctr(ppb, 0xe);
	    w_ctr(ppb, 0x4);
	}
	return 0;		/* Finished */
	break;

    default:
	printk("imm: Invalid scsi phase\n");
    }
    return 0;
}

int imm_queuecommand(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
    int host_no = cmd->host->unique_id;

    if (imm_hosts[host_no].cur_cmd) {
	printk("IMM: bug in imm_queuecommand\n");
	return 0;
    }
    imm_hosts[host_no].failed = 0;
    imm_hosts[host_no].jstart = jiffies;
    imm_hosts[host_no].cur_cmd = cmd;
    cmd->scsi_done = done;
    cmd->result = DID_ERROR << 16;	/* default return code */
    cmd->SCp.phase = 0;		/* bus free */

    imm_pb_claim(host_no);

    imm_hosts[host_no].imm_tq.data = imm_hosts + host_no;
    imm_hosts[host_no].imm_tq.sync = 0;
    queue_task(&imm_hosts[host_no].imm_tq, &tq_immediate);
    mark_bh(IMMEDIATE_BH);

    return 0;
}

/*
 * Apparently the disk->capacity attribute is off by 1 sector 
 * for all disk drives.  We add the one here, but it should really
 * be done in sd.c.  Even if it gets fixed there, this will still
 * work.
 */
int imm_biosparam(Disk * disk, kdev_t dev, int ip[])
{
    ip[0] = 0x40;
    ip[1] = 0x20;
    ip[2] = (disk->capacity + 1) / (ip[0] * ip[1]);
    if (ip[2] > 1024) {
	ip[0] = 0xff;
	ip[1] = 0x3f;
	ip[2] = (disk->capacity + 1) / (ip[0] * ip[1]);
    }
    return 0;
}

int imm_abort(Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;
    /*
     * There is no method for aborting commands since Iomega
     * have tied the SCSI_MESSAGE line high in the interface
     */

    switch (cmd->SCp.phase) {
    case 0:			/* Do not have access to parport */
    case 1:			/* Have not connected to interface */
	imm_hosts[host_no].cur_cmd = NULL;	/* Forget the problem */
	return SUCCESS;
	break;
    default:			/* SCSI command sent, can not abort */
	return FAILED;
	break;
    }
}

void imm_reset_pulse(unsigned int base)
{
    w_ctr(base, 0x04);
    w_dtr(base, 0x40);
    udelay(1);
    w_ctr(base, 0x0c);
    w_ctr(base, 0x0d);
    udelay(50);
    w_ctr(base, 0x0c);
    w_ctr(base, 0x04);
}

int imm_reset(Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;

    if (cmd->SCp.phase)
	imm_disconnect(host_no);
    imm_hosts[host_no].cur_cmd = NULL;	/* Forget the problem */

    imm_connect(host_no, CONNECT_NORMAL);
    imm_reset_pulse(IMM_BASE(host_no));
    udelay(1000);		/* device settle delay */
    imm_disconnect(host_no);
    udelay(1000);		/* device settle delay */
    return SUCCESS;
}

static int device_check(int host_no)
{
    /* This routine looks for a device and then attempts to use EPP
       to send a command. If all goes as planned then EPP is available. */

    static char cmd[6] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int loop, old_mode, status, k, ppb = IMM_BASE(host_no);
    unsigned char l;

    old_mode = imm_hosts[host_no].mode;
    for (loop = 0; loop < 8; loop++) {
	/* Attempt to use EPP for Test Unit Ready */
	if ((ppb & 0x0007) == 0x0000)
	    imm_hosts[host_no].mode = IMM_EPP_32;

      second_pass:
	imm_connect(host_no, CONNECT_EPP_MAYBE);
	/* Select SCSI device */
	if (!imm_select(host_no, loop)) {
	    imm_disconnect(host_no);
	    continue;
	}
	printk("imm: Found device at ID %i, Attempting to use %s\n", loop,
	       IMM_MODE_STRING[imm_hosts[host_no].mode]);

	/* Send SCSI command */
	status = 1;
	w_ctr(ppb, 0x0c);
	for (l = 0; (l < 3) && (status); l++)
	    status = imm_out(host_no, &cmd[l << 1], 2);

	if (!status) {
            imm_disconnect(host_no);
            imm_connect(host_no, CONNECT_EPP_MAYBE);
            imm_reset_pulse(IMM_BASE(host_no));
            udelay(1000);
            imm_disconnect(host_no);
            udelay(1000);
            if (imm_hosts[host_no].mode == IMM_EPP_32) {
                imm_hosts[host_no].mode = old_mode;
                goto second_pass;
            }
	    printk("imm: Unable to establish communication, aborting driver load.\n");
	    return 1;
	}
	w_ctr(ppb, 0x0c);

	k = 1000000;		/* 1 Second */
	do {
	    l = r_str(ppb);
	    k--;
	    udelay(1);
	} while (!(l & 0x80) && (k));

	l &= 0xb8;

	if (l != 0xb8) {
	    imm_disconnect(host_no);
	    imm_connect(host_no, CONNECT_EPP_MAYBE);
	    imm_reset_pulse(IMM_BASE(host_no));
	    udelay(1000);
	    imm_disconnect(host_no);
	    udelay(1000);
	    if (imm_hosts[host_no].mode == IMM_EPP_32) {
		imm_hosts[host_no].mode = old_mode;
		goto second_pass;
	    }
	    printk("imm: Unable to establish communication, aborting driver load.\n");
	    return 1;
	}
	imm_disconnect(host_no);
	printk("imm: Communication established at 0x%x with ID %i using %s\n", ppb, loop,
	       IMM_MODE_STRING[imm_hosts[host_no].mode]);
	imm_connect(host_no, CONNECT_EPP_MAYBE);
	imm_reset_pulse(IMM_BASE(host_no));
	udelay(1000);
	imm_disconnect(host_no);
	udelay(1000);
	return 0;
    }
    printk("imm: No devices found, aborting driver load.\n");
    return 1;
}
MODULE_LICENSE("GPL");
