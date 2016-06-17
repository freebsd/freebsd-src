/* ppa.c   --  low level driver for the IOMEGA PPA3 
 * parallel port SCSI host adapter.
 * 
 * (The PPA3 is the embedded controller in the ZIP drive.)
 * 
 * (c) 1995,1996 Grant R. Guenther, grant@torque.net,
 * under the terms of the GNU General Public License.
 * 
 * Current Maintainer: David Campbell (Perth, Western Australia, GMT+0800)
 *                     campbell@torque.net
 */

#include <linux/config.h>

/* The following #define is to avoid a clash with hosts.c */
#define PPA_CODE 1

#include <linux/blk.h>
#include <asm/io.h>
#include <linux/parport.h>
#include "sd.h"
#include "hosts.h"
int ppa_release(struct Scsi_Host *);
static void ppa_reset_pulse(unsigned int base);

typedef struct {
    struct pardevice *dev;	/* Parport device entry         */
    int base;			/* Actual port address          */
    int mode;			/* Transfer mode                */
    int host;			/* Host number (for proc)       */
    Scsi_Cmnd *cur_cmd;		/* Current queued command       */
    struct tq_struct ppa_tq;	/* Polling interrupt stuff       */
    unsigned long jstart;	/* Jiffies at start             */
    unsigned long recon_tmo;    /* How many usecs to wait for reconnection (6th bit) */
    unsigned int failed:1;	/* Failure flag                 */
    unsigned int p_busy:1;	/* Parport sharing busy flag    */
} ppa_struct;

#define PPA_EMPTY	\
{	dev:		NULL,		\
	base:		-1,		\
	mode:		PPA_AUTODETECT,	\
	host:		-1,		\
	cur_cmd:	NULL,		\
	ppa_tq:		{ routine: ppa_interrupt },	\
	jstart:		0,		\
	recon_tmo:      PPA_RECON_TMO,	\
	failed:		0,		\
	p_busy:		0		\
}

#include  "ppa.h"

#define NO_HOSTS 4
static ppa_struct ppa_hosts[NO_HOSTS] =
{PPA_EMPTY, PPA_EMPTY, PPA_EMPTY, PPA_EMPTY};

#define PPA_BASE(x)	ppa_hosts[(x)].base

void ppa_wakeup(void *ref)
{
    ppa_struct *ppa_dev = (ppa_struct *) ref;

    if (!ppa_dev->p_busy)
	return;

    if (parport_claim(ppa_dev->dev)) {
	printk("ppa: bug in ppa_wakeup\n");
	return;
    }
    ppa_dev->p_busy = 0;
    ppa_dev->base = ppa_dev->dev->port->base;
    if (ppa_dev->cur_cmd)
	ppa_dev->cur_cmd->SCp.phase++;
    return;
}

int ppa_release(struct Scsi_Host *host)
{
    int host_no = host->unique_id;

    printk("Releasing ppa%i\n", host_no);
    parport_unregister_device(ppa_hosts[host_no].dev);
    return 0;
}

static int ppa_pb_claim(int host_no)
{
    if (parport_claim(ppa_hosts[host_no].dev)) {
	ppa_hosts[host_no].p_busy = 1;
	return 1;
    }
    if (ppa_hosts[host_no].cur_cmd)
	ppa_hosts[host_no].cur_cmd->SCp.phase++;
    return 0;
}

#define ppa_pb_release(x) parport_release(ppa_hosts[(x)].dev)

/***************************************************************************
 *                   Parallel port probing routines                        *
 ***************************************************************************/

static Scsi_Host_Template driver_template = PPA;
#include  "scsi_module.c"

/*
 * Start of Chipset kludges
 */

int ppa_detect(Scsi_Host_Template * host)
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

    printk("ppa: Version %s\n", PPA_VERSION);
    nhosts = 0;
    try_again = 0;

    if (!pb) {
	printk("ppa: parport reports no devices.\n");
	spin_lock_irq(&io_request_lock);
	return 0;
    }
  retry_entry:
    for (i = 0; pb; i++, pb = pb->next) {
	int modes, ppb, ppb_hi;

	ppa_hosts[i].dev =
	    parport_register_device(pb, "ppa", NULL, ppa_wakeup,
				    NULL, 0, (void *) &ppa_hosts[i]);

	if (!ppa_hosts[i].dev)
	    continue;

	/* Claim the bus so it remembers what we do to the control
	 * registers. [ CTR and ECP ]
	 */
	if (ppa_pb_claim(i)) {
	    unsigned long now = jiffies;
	    while (ppa_hosts[i].p_busy) {
		schedule();	/* We are safe to schedule here */
		if (time_after(jiffies, now + 3 * HZ)) {
		    printk(KERN_ERR "ppa%d: failed to claim parport because a "
		      "pardevice is owning the port for too longtime!\n",
			   i);
		    parport_unregister_device(ppa_hosts[i].dev);
		    spin_lock_irq(&io_request_lock);
		    return 0;
		}
	    }
	}
	ppb = PPA_BASE(i) = ppa_hosts[i].dev->port->base;
	ppb_hi =  ppa_hosts[i].dev->port->base_hi;
	w_ctr(ppb, 0x0c);
	modes = ppa_hosts[i].dev->port->modes;

	/* Mode detection works up the chain of speed
	 * This avoids a nasty if-then-else-if-... tree
	 */
	ppa_hosts[i].mode = PPA_NIBBLE;

	if (modes & PARPORT_MODE_TRISTATE)
	    ppa_hosts[i].mode = PPA_PS2;

	if (modes & PARPORT_MODE_ECP) {
	    w_ecr(ppb_hi, 0x20);
	    ppa_hosts[i].mode = PPA_PS2;
	}
	if ((modes & PARPORT_MODE_EPP) && (modes & PARPORT_MODE_ECP))
	    w_ecr(ppb_hi, 0x80);

	/* Done configuration */
	ppa_pb_release(i);

	if (ppa_init(i)) {
	    parport_unregister_device(ppa_hosts[i].dev);
	    continue;
	}
	/* now the glue ... */
	switch (ppa_hosts[i].mode) {
	case PPA_NIBBLE:
	    ports = 3;
	    break;
	case PPA_PS2:
	    ports = 3;
	    break;
	case PPA_EPP_8:
	case PPA_EPP_16:
	case PPA_EPP_32:
	    ports = 8;
	    break;
	default:		/* Never gets here */
	    continue;
	}

	host->can_queue = PPA_CAN_QUEUE;
	host->sg_tablesize = ppa_sg;
	hreg = scsi_register(host, 0);
	if(hreg == NULL)
		continue;
	hreg->io_port = pb->base;
	hreg->n_io_port = ports;
	hreg->dma_channel = -1;
	hreg->unique_id = i;
	ppa_hosts[i].host = hreg->host_no;
	nhosts++;
    }
    if (nhosts == 0) {
	if (try_again == 1) {
	    printk("WARNING - no ppa compatible devices found.\n");
	    printk("  As of 31/Aug/1998 Iomega started shipping parallel\n");
	    printk("  port ZIP drives with a different interface which is\n");
	    printk("  supported by the imm (ZIP Plus) driver. If the\n");
	    printk("  cable is marked with \"AutoDetect\", this is what has\n");
	    printk("  happened.\n");
	    spin_lock_irq(&io_request_lock);
	    return 0;
	}
	try_again = 1;
	goto retry_entry;
    } else {
	spin_lock_irq(&io_request_lock);
	return 1;		/* return number of hosts detected */
    }
}

/* This is to give the ppa driver a way to modify the timings (and other
 * parameters) by writing to the /proc/scsi/ppa/0 file.
 * Very simple method really... (To simple, no error checking :( )
 * Reason: Kernel hackers HATE having to unload and reload modules for
 * testing...
 * Also gives a method to use a script to obtain optimum timings (TODO)
 */

static inline int ppa_proc_write(int hostno, char *buffer, int length)
{
    unsigned long x;

    if ((length > 5) && (strncmp(buffer, "mode=", 5) == 0)) {
	x = simple_strtoul(buffer + 5, NULL, 0);
	ppa_hosts[hostno].mode = x;
	return length;
    }
    if ((length > 10) && (strncmp(buffer, "recon_tmo=", 10) == 0)) {
	x = simple_strtoul(buffer + 10, NULL, 0);
	ppa_hosts[hostno].recon_tmo = x;
        printk("ppa: recon_tmo set to %ld\n", x);
	return length;
    }
    printk("ppa /proc: invalid variable\n");
    return (-EINVAL);
}

int ppa_proc_info(char *buffer, char **start, off_t offset,
		  int length, int hostno, int inout)
{
    int i;
    int len = 0;

    for (i = 0; i < 4; i++)
	if (ppa_hosts[i].host == hostno)
	    break;

    if (inout)
	return ppa_proc_write(i, buffer, length);

    len += sprintf(buffer + len, "Version : %s\n", PPA_VERSION);
    len += sprintf(buffer + len, "Parport : %s\n", ppa_hosts[i].dev->port->name);
    len += sprintf(buffer + len, "Mode    : %s\n", PPA_MODE_STRING[ppa_hosts[i].mode]);
#if PPA_DEBUG > 0
    len += sprintf(buffer + len, "recon_tmo : %lu\n", ppa_hosts[i].recon_tmo);
#endif

    /* Request for beyond end of buffer */
    if (offset > length)
	return 0;

    *start = buffer + offset;
    len -= offset;
    if (len > length)
	len = length;
    return len;
}

static int device_check(int host_no);

#if PPA_DEBUG > 0
#define ppa_fail(x,y) printk("ppa: ppa_fail(%i) from %s at line %d\n",\
	   y, __FUNCTION__, __LINE__); ppa_fail_func(x,y);
static inline void ppa_fail_func(int host_no, int error_code)
#else
static inline void ppa_fail(int host_no, int error_code)
#endif
{
    /* If we fail a device then we trash status / message bytes */
    if (ppa_hosts[host_no].cur_cmd) {
	ppa_hosts[host_no].cur_cmd->result = error_code << 16;
	ppa_hosts[host_no].failed = 1;
    }
}

/*
 * Wait for the high bit to be set.
 * 
 * In principle, this could be tied to an interrupt, but the adapter
 * doesn't appear to be designed to support interrupts.  We spin on
 * the 0x80 ready bit. 
 */
static unsigned char ppa_wait(int host_no)
{
    int k;
    unsigned short ppb = PPA_BASE(host_no);
    unsigned char r;

    k = PPA_SPIN_TMO;
    /* Wait for bit 6 and 7 - PJC */
    for (r = r_str (ppb); ((r & 0xc0)!=0xc0) && (k); k--) {
	    udelay (1);
	    r = r_str (ppb);
    }

    /*
     * return some status information.
     * Semantics: 0xc0 = ZIP wants more data
     *            0xd0 = ZIP wants to send more data
     *            0xe0 = ZIP is expecting SCSI command data
     *            0xf0 = end of transfer, ZIP is sending status
     */
    if (k)
	return (r & 0xf0);

    /* Counter expired - Time out occurred */
    ppa_fail(host_no, DID_TIME_OUT);
    printk("ppa timeout in ppa_wait\n");
    return 0;			/* command timed out */
}

/*
 * Clear EPP Timeout Bit 
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
    int i, ppb_hi=ppa_hosts[hostno].dev->port->base_hi;

    if (ppb_hi == 0) return;

    if ((r_ecr(ppb_hi) & 0xe0) == 0x60) { /* mode 011 == ECP fifo mode */
        for (i = 0; i < 100; i++) {
            if (r_ecr(ppb_hi) & 0x01)
                return;
            udelay(5);
        }
        printk("ppa: ECP sync failed as data still present in FIFO.\n");
    }
}

static int ppa_byte_out(unsigned short base, const char *buffer, int len)
{
    int i;

    for (i = len; i; i--) {
	w_dtr(base, *buffer++);
	w_ctr(base, 0xe);
	w_ctr(base, 0xc);
    }
    return 1;			/* All went well - we hope! */
}

static int ppa_byte_in(unsigned short base, char *buffer, int len)
{
    int i;

    for (i = len; i; i--) {
	*buffer++ = r_dtr(base);
	w_ctr(base, 0x27);
	w_ctr(base, 0x25);
    }
    return 1;			/* All went well - we hope! */
}

static int ppa_nibble_in(unsigned short base, char *buffer, int len)
{
    for (; len; len--) {
	unsigned char h;

	w_ctr(base, 0x4);
	h = r_str(base) & 0xf0;
	w_ctr(base, 0x6);
	*buffer++ = h | ((r_str(base) & 0xf0) >> 4);
    }
    return 1;			/* All went well - we hope! */
}

static int ppa_out(int host_no, char *buffer, int len)
{
    int r;
    unsigned short ppb = PPA_BASE(host_no);

    r = ppa_wait(host_no);

    if ((r & 0x50) != 0x40) {
	ppa_fail(host_no, DID_ERROR);
	return 0;
    }
    switch (ppa_hosts[host_no].mode) {
    case PPA_NIBBLE:
    case PPA_PS2:
	/* 8 bit output, with a loop */
	r = ppa_byte_out(ppb, buffer, len);
	break;

    case PPA_EPP_32:
    case PPA_EPP_16:
    case PPA_EPP_8:
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

    default:
	printk("PPA: bug in ppa_out()\n");
	r = 0;
    }
    return r;
}

static int ppa_in(int host_no, char *buffer, int len)
{
    int r;
    unsigned short ppb = PPA_BASE(host_no);

    r = ppa_wait(host_no);

    if ((r & 0x50) != 0x50) {
	ppa_fail(host_no, DID_ERROR);
	return 0;
    }
    switch (ppa_hosts[host_no].mode) {
    case PPA_NIBBLE:
	/* 4 bit input, with a loop */
	r = ppa_nibble_in(ppb, buffer, len);
	w_ctr(ppb, 0xc);
	break;

    case PPA_PS2:
	/* 8 bit input, with a loop */
	w_ctr(ppb, 0x25);
	r = ppa_byte_in(ppb, buffer, len);
	w_ctr(ppb, 0x4);
	w_ctr(ppb, 0xc);
	break;

    case PPA_EPP_32:
    case PPA_EPP_16:
    case PPA_EPP_8:
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
	printk("PPA: bug in ppa_ins()\n");
	r = 0;
	break;
    }
    return r;
}

/* end of ppa_io.h */
static inline void ppa_d_pulse(unsigned short ppb, unsigned char b)
{
    w_dtr(ppb, b);
    w_ctr(ppb, 0xc);
    w_ctr(ppb, 0xe);
    w_ctr(ppb, 0xc);
    w_ctr(ppb, 0x4);
    w_ctr(ppb, 0xc);
}

static void ppa_disconnect(int host_no)
{
    unsigned short ppb = PPA_BASE(host_no);

    ppa_d_pulse(ppb, 0);
    ppa_d_pulse(ppb, 0x3c);
    ppa_d_pulse(ppb, 0x20);
    ppa_d_pulse(ppb, 0xf);
}

static inline void ppa_c_pulse(unsigned short ppb, unsigned char b)
{
    w_dtr(ppb, b);
    w_ctr(ppb, 0x4);
    w_ctr(ppb, 0x6);
    w_ctr(ppb, 0x4);
    w_ctr(ppb, 0xc);
}

static inline void ppa_connect(int host_no, int flag)
{
    unsigned short ppb = PPA_BASE(host_no);

    ppa_c_pulse(ppb, 0);
    ppa_c_pulse(ppb, 0x3c);
    ppa_c_pulse(ppb, 0x20);
    if ((flag == CONNECT_EPP_MAYBE) &&
	IN_EPP_MODE(ppa_hosts[host_no].mode))
	ppa_c_pulse(ppb, 0xcf);
    else
	ppa_c_pulse(ppb, 0x8f);
}

static int ppa_select(int host_no, int target)
{
    int k;
    unsigned short ppb = PPA_BASE(host_no);

    /*
     * Bit 6 (0x40) is the device selected bit.
     * First we must wait till the current device goes off line...
     */
    k = PPA_SELECT_TMO;
    do {
	k--;
	udelay(1);
    } while ((r_str(ppb) & 0x40) && (k));
    if (!k)
	return 0;

    w_dtr(ppb, (1 << target));
    w_ctr(ppb, 0xe);
    w_ctr(ppb, 0xc);
    w_dtr(ppb, 0x80);		/* This is NOT the initator */
    w_ctr(ppb, 0x8);

    k = PPA_SELECT_TMO;
    do {
	k--;
	udelay(1);
    }
    while (!(r_str(ppb) & 0x40) && (k));
    if (!k)
	return 0;

    return 1;
}

/* 
 * This is based on a trace of what the Iomega DOS 'guest' driver does.
 * I've tried several different kinds of parallel ports with guest and
 * coded this to react in the same ways that it does.
 * 
 * The return value from this function is just a hint about where the
 * handshaking failed.
 * 
 */
static int ppa_init(int host_no)
{
    int retv;
    unsigned short ppb = PPA_BASE(host_no);

#if defined(CONFIG_PARPORT) || defined(CONFIG_PARPORT_MODULE)
    if (ppa_pb_claim(host_no))
	while (ppa_hosts[host_no].p_busy)
	    schedule();		/* We can safe schedule here */
#endif

    ppa_disconnect(host_no);
    ppa_connect(host_no, CONNECT_NORMAL);

    retv = 2;			/* Failed */

    w_ctr(ppb, 0xe);
    if ((r_str(ppb) & 0x08) == 0x08)
	retv--;

    w_ctr(ppb, 0xc);
    if ((r_str(ppb) & 0x08) == 0x00)
	retv--;

    if (!retv)
	ppa_reset_pulse(ppb);
    udelay(1000);		/* Allow devices to settle down */
    ppa_disconnect(host_no);
    udelay(1000);		/* Another delay to allow devices to settle */

    if (!retv)
	retv = device_check(host_no);

    ppa_pb_release(host_no);
    return retv;
}

static inline int ppa_send_command(Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;
    int k;

    w_ctr(PPA_BASE(host_no), 0x0c);

    for (k = 0; k < cmd->cmd_len; k++)
	if (!ppa_out(host_no, &cmd->cmnd[k], 1))
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
static int ppa_completion(Scsi_Cmnd * cmd)
{
    /* Return codes:
     * -1     Error
     *  0     Told to schedule
     *  1     Finished data transfer
     */
    int host_no = cmd->host->unique_id;
    unsigned short ppb = PPA_BASE(host_no);
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
     * hence no need for a full ppa_wait.
     */
    r = (r_str(ppb) & 0xf0);

    while (r != (unsigned char) 0xf0) {
	/*
	 * If we have been running for more than a full timer tick
	 * then take a rest.
	 */
	if (time_after(jiffies, start_jiffies + 1))
	    return 0;

	if ((cmd->SCp.this_residual <= 0)) {
	    ppa_fail(host_no, DID_ERROR);
	    return -1;		/* ERROR_RETURN */
	}

	/* On some hardware we have SCSI disconnected (6th bit low)
	 * for about 100usecs. It is too expensive to wait a 
	 * tick on every loop so we busy wait for no more than
	 * 500usecs to give the drive a chance first. We do not 
	 * change things for "normal" hardware since generally 
	 * the 6th bit is always high.
	 * This makes the CPU load higher on some hardware 
	 * but otherwise we can not get more then 50K/secs 
	 * on this problem hardware.
	 */
	if ((r & 0xc0) != 0xc0) {
	   /* Wait for reconnection should be no more than 
	    * jiffy/2 = 5ms = 5000 loops
	    */
	   unsigned long k = ppa_hosts[host_no].recon_tmo; 
	   for (; k && ((r = (r_str(ppb) & 0xf0)) & 0xc0) != 0xc0; k--)
	     udelay(1);

	   if(!k) 
	     return 0;
	}	   

	/* determine if we should use burst I/O */ 
	fast = (bulk && (cmd->SCp.this_residual >= PPA_BURST_SIZE)) 
	     ? PPA_BURST_SIZE : 1;

	if (r == (unsigned char) 0xc0)
	    status = ppa_out(host_no, cmd->SCp.ptr, fast);
	else
	    status = ppa_in(host_no, cmd->SCp.ptr, fast);

	cmd->SCp.ptr += fast;
	cmd->SCp.this_residual -= fast;

	if (!status) {
	    ppa_fail(host_no, DID_BUS_BUSY);
	    return -1;		/* ERROR_RETURN */
	}
	if (cmd->SCp.buffer && !cmd->SCp.this_residual) {
	    /* if scatter/gather, advance to the next segment */
	    if (cmd->SCp.buffers_residual--) {
		cmd->SCp.buffer++;
		cmd->SCp.this_residual = cmd->SCp.buffer->length;
		cmd->SCp.ptr = cmd->SCp.buffer->address;
	    }
	}
	/* Now check to see if the drive is ready to comunicate */
	r = (r_str(ppb) & 0xf0);
	/* If not, drop back down to the scheduler and wait a timer tick */
	if (!(r & 0x80))
	    return 0;
    }
    return 1;			/* FINISH_RETURN */
}

/* deprecated synchronous interface */
int ppa_command(Scsi_Cmnd * cmd)
{
    static int first_pass = 1;
    int host_no = cmd->host->unique_id;

    if (first_pass) {
	printk("ppa: using non-queuing interface\n");
	first_pass = 0;
    }
    if (ppa_hosts[host_no].cur_cmd) {
	printk("PPA: bug in ppa_command\n");
	return 0;
    }
    ppa_hosts[host_no].failed = 0;
    ppa_hosts[host_no].jstart = jiffies;
    ppa_hosts[host_no].cur_cmd = cmd;
    cmd->result = DID_ERROR << 16;	/* default return code */
    cmd->SCp.phase = 0;

    ppa_pb_claim(host_no);

    while (ppa_engine(&ppa_hosts[host_no], cmd))
	schedule();

    if (cmd->SCp.phase)		/* Only disconnect if we have connected */
	ppa_disconnect(cmd->host->unique_id);

    ppa_pb_release(host_no);
    ppa_hosts[host_no].cur_cmd = 0;
    return cmd->result;
}

/*
 * Since the PPA itself doesn't generate interrupts, we use
 * the scheduler's task queue to generate a stream of call-backs and
 * complete the request when the drive is ready.
 */
static void ppa_interrupt(void *data)
{
    ppa_struct *tmp = (ppa_struct *) data;
    Scsi_Cmnd *cmd = tmp->cur_cmd;
    unsigned long flags;

    if (!cmd) {
	printk("PPA: bug in ppa_interrupt\n");
	return;
    }
    if (ppa_engine(tmp, cmd)) {
	tmp->ppa_tq.data = (void *) tmp;
	tmp->ppa_tq.sync = 0;
	queue_task(&tmp->ppa_tq, &tq_timer);
	return;
    }
    /* Command must of completed hence it is safe to let go... */
#if PPA_DEBUG > 0
    switch ((cmd->result >> 16) & 0xff) {
    case DID_OK:
	break;
    case DID_NO_CONNECT:
	printk("ppa: no device at SCSI ID %i\n", cmd->target);
	break;
    case DID_BUS_BUSY:
	printk("ppa: BUS BUSY - EPP timeout detected\n");
	break;
    case DID_TIME_OUT:
	printk("ppa: unknown timeout\n");
	break;
    case DID_ABORT:
	printk("ppa: told to abort\n");
	break;
    case DID_PARITY:
	printk("ppa: parity error (???)\n");
	break;
    case DID_ERROR:
	printk("ppa: internal driver error\n");
	break;
    case DID_RESET:
	printk("ppa: told to reset device\n");
	break;
    case DID_BAD_INTR:
	printk("ppa: bad interrupt (???)\n");
	break;
    default:
	printk("ppa: bad return code (%02x)\n", (cmd->result >> 16) & 0xff);
    }
#endif

    if (cmd->SCp.phase > 1)
	ppa_disconnect(cmd->host->unique_id);
    if (cmd->SCp.phase > 0)
	ppa_pb_release(cmd->host->unique_id);

    tmp->cur_cmd = 0;
    
    spin_lock_irqsave(&io_request_lock, flags);
    cmd->scsi_done(cmd);
    spin_unlock_irqrestore(&io_request_lock, flags);
    return;
}

static int ppa_engine(ppa_struct * tmp, Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;
    unsigned short ppb = PPA_BASE(host_no);
    unsigned char l = 0, h = 0;
    int retv;

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
	    ppa_fail(host_no, DID_BUS_BUSY);
	    return 0;
	}
	return 1;		/* wait until ppa_wakeup claims parport */
    case 1:			/* Phase 1 - Connected */
	{			/* Perform a sanity check for cable unplugged */
	    int retv = 2;	/* Failed */

	    ppa_connect(host_no, CONNECT_EPP_MAYBE);

	    w_ctr(ppb, 0xe);
	    if ((r_str(ppb) & 0x08) == 0x08)
		retv--;

	    w_ctr(ppb, 0xc);
	    if ((r_str(ppb) & 0x08) == 0x00)
		retv--;

	    if (retv) {
		if ((jiffies - tmp->jstart) > (1 * HZ)) {
		    printk("ppa: Parallel port cable is unplugged!!\n");
		    ppa_fail(host_no, DID_BUS_BUSY);
		    return 0;
		} else {
		    ppa_disconnect(host_no);
		    return 1;	/* Try again in a jiffy */
		}
	    }
	    cmd->SCp.phase++;
	}

    case 2:			/* Phase 2 - We are now talking to the scsi bus */
	if (!ppa_select(host_no, cmd->target)) {
	    ppa_fail(host_no, DID_NO_CONNECT);
	    return 0;
	}
	cmd->SCp.phase++;

    case 3:			/* Phase 3 - Ready to accept a command */
	w_ctr(ppb, 0x0c);
	if (!(r_str(ppb) & 0x80))
	    return 1;

	if (!ppa_send_command(cmd))
	    return 0;
	cmd->SCp.phase++;

    case 4:			/* Phase 4 - Setup scatter/gather buffers */
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

    case 5:			/* Phase 5 - Data transfer stage */
	w_ctr(ppb, 0x0c);
	if (!(r_str(ppb) & 0x80))
	    return 1;

	retv = ppa_completion(cmd);
	if (retv == -1)
	    return 0;
	if (retv == 0)
	    return 1;
	cmd->SCp.phase++;

    case 6:			/* Phase 6 - Read status/message */
	cmd->result = DID_OK << 16;
	/* Check for data overrun */
	if (ppa_wait(host_no) != (unsigned char) 0xf0) {
	    ppa_fail(host_no, DID_ERROR);
	    return 0;
	}
	if (ppa_in(host_no, &l, 1)) {	/* read status byte */
	    /* Check for optional message byte */
	    if (ppa_wait(host_no) == (unsigned char) 0xf0)
		ppa_in(host_no, &h, 1);
	    cmd->result = (DID_OK << 16) + (h << 8) + (l & STATUS_MASK);
	}
	return 0;		/* Finished */
	break;

    default:
	printk("ppa: Invalid scsi phase\n");
    }
    return 0;
}

int ppa_queuecommand(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
    int host_no = cmd->host->unique_id;

    if (ppa_hosts[host_no].cur_cmd) {
	printk("PPA: bug in ppa_queuecommand\n");
	return 0;
    }
    ppa_hosts[host_no].failed = 0;
    ppa_hosts[host_no].jstart = jiffies;
    ppa_hosts[host_no].cur_cmd = cmd;
    cmd->scsi_done = done;
    cmd->result = DID_ERROR << 16;	/* default return code */
    cmd->SCp.phase = 0;		/* bus free */

    ppa_pb_claim(host_no);

    ppa_hosts[host_no].ppa_tq.data = ppa_hosts + host_no;
    ppa_hosts[host_no].ppa_tq.sync = 0;
    queue_task(&ppa_hosts[host_no].ppa_tq, &tq_immediate);
    mark_bh(IMMEDIATE_BH);

    return 0;
}

/*
 * Apparently the disk->capacity attribute is off by 1 sector 
 * for all disk drives.  We add the one here, but it should really
 * be done in sd.c.  Even if it gets fixed there, this will still
 * work.
 */
int ppa_biosparam(Disk * disk, kdev_t dev, int ip[])
{
    ip[0] = 0x40;
    ip[1] = 0x20;
    ip[2] = (disk->capacity + 1) / (ip[0] * ip[1]);
    if (ip[2] > 1024) {
	ip[0] = 0xff;
	ip[1] = 0x3f;
	ip[2] = (disk->capacity + 1) / (ip[0] * ip[1]);
	if (ip[2] > 1023)
	    ip[2] = 1023;
    }
    return 0;
}

int ppa_abort(Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;
    /*
     * There is no method for aborting commands since Iomega
     * have tied the SCSI_MESSAGE line high in the interface
     */

    switch (cmd->SCp.phase) {
    case 0:			/* Do not have access to parport */
    case 1:			/* Have not connected to interface */
	ppa_hosts[host_no].cur_cmd = NULL;	/* Forget the problem */
	return SUCCESS;
	break;
    default:			/* SCSI command sent, can not abort */
	return FAILED;
	break;
    }
}

static void ppa_reset_pulse(unsigned int base)
{
    w_dtr(base, 0x40);
    w_ctr(base, 0x8);
    udelay(30);
    w_ctr(base, 0xc);
}

int ppa_reset(Scsi_Cmnd * cmd)
{
    int host_no = cmd->host->unique_id;

    if (cmd->SCp.phase)
	ppa_disconnect(host_no);
    ppa_hosts[host_no].cur_cmd = NULL;	/* Forget the problem */

    ppa_connect(host_no, CONNECT_NORMAL);
    ppa_reset_pulse(PPA_BASE(host_no));
    udelay(1000);		/* device settle delay */
    ppa_disconnect(host_no);
    udelay(1000);		/* device settle delay */
    return SUCCESS;
}

static int device_check(int host_no)
{
    /* This routine looks for a device and then attempts to use EPP
       to send a command. If all goes as planned then EPP is available. */

    static char cmd[6] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int loop, old_mode, status, k, ppb = PPA_BASE(host_no);
    unsigned char l;

    old_mode = ppa_hosts[host_no].mode;
    for (loop = 0; loop < 8; loop++) {
	/* Attempt to use EPP for Test Unit Ready */
	if ((ppb & 0x0007) == 0x0000)
	    ppa_hosts[host_no].mode = PPA_EPP_32;

      second_pass:
	ppa_connect(host_no, CONNECT_EPP_MAYBE);
	/* Select SCSI device */
	if (!ppa_select(host_no, loop)) {
	    ppa_disconnect(host_no);
	    continue;
	}
	printk("ppa: Found device at ID %i, Attempting to use %s\n", loop,
	       PPA_MODE_STRING[ppa_hosts[host_no].mode]);

	/* Send SCSI command */
	status = 1;
	w_ctr(ppb, 0x0c);
	for (l = 0; (l < 6) && (status); l++)
	    status = ppa_out(host_no, cmd, 1);

	if (!status) {
	    ppa_disconnect(host_no);
	    ppa_connect(host_no, CONNECT_EPP_MAYBE);
	    w_dtr(ppb, 0x40);
	    w_ctr(ppb, 0x08);
	    udelay(30);
	    w_ctr(ppb, 0x0c);
	    udelay(1000);
	    ppa_disconnect(host_no);
	    udelay(1000);
	    if (ppa_hosts[host_no].mode == PPA_EPP_32) {
		ppa_hosts[host_no].mode = old_mode;
		goto second_pass;
	    }
	    printk("ppa: Unable to establish communication, aborting driver load.\n");
	    return 1;
	}
	w_ctr(ppb, 0x0c);
	k = 1000000;		/* 1 Second */
	do {
	    l = r_str(ppb);
	    k--;
	    udelay(1);
	} while (!(l & 0x80) && (k));

	l &= 0xf0;

	if (l != 0xf0) {
	    ppa_disconnect(host_no);
	    ppa_connect(host_no, CONNECT_EPP_MAYBE);
	    ppa_reset_pulse(ppb);
	    udelay(1000);
	    ppa_disconnect(host_no);
	    udelay(1000);
	    if (ppa_hosts[host_no].mode == PPA_EPP_32) {
		ppa_hosts[host_no].mode = old_mode;
		goto second_pass;
	    }
	    printk("ppa: Unable to establish communication, aborting driver load.\n");
	    return 1;
	}
	ppa_disconnect(host_no);
	printk("ppa: Communication established with ID %i using %s\n", loop,
	       PPA_MODE_STRING[ppa_hosts[host_no].mode]);
	ppa_connect(host_no, CONNECT_EPP_MAYBE);
	ppa_reset_pulse(ppb);
	udelay(1000);
	ppa_disconnect(host_no);
	udelay(1000);
	return 0;
    }
    printk("ppa: No devices found, aborting driver load.\n");
    return 1;
}
MODULE_LICENSE("GPL");
