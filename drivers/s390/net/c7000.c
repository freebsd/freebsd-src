/*
	Cisco/7000 driver -- Copyright (C) 2000 UTS Global LLC.
	Author: Bob Scardapane (UTS Global LLC).
	Version: 3.

	To use this driver, run the LINUX command:

	insmod c7000 base0=0xYYYY lhost0=s1 uhost0=s2 lappl0=s3 uappl0=s4 dbg=x

	base0=0xYYYY defines the base unit address of the interface.
	lhost0=s1 defines the local host name.
	uhost0=s2 defines the unit host name.
	lappl0=s3 defines the local application name.
	uappl0=s4 defines the unit application name.
	dbg=x defines the message level.  Higher values will result in
	additional diagnostic messages.

	Additional interfaces are defined on insmod by using the variable
	name groups "base1,lhost1,lappl1,uhost1,uappl1", etc... up to three
	additional groups.

	In addition, the module will automatically detect the unit base
	addresses by scanning all active irq's for a control unit type
	of 0x3088 and a model of 0x61 (CLAW mode). The noauto parameter
	can be used to suppress automatic detection.

	The values of lhostx, lapplx, uhostx and uapplx default to:

	lapplx - TCPIP
	lhostx - UTS
	uapplx - TCPIP
	uhostx - C7011

	Note that the values passed in the insmod command will always
	override the automatic detection of the unit base addreeses and
	the default values of lapplx, lhostx, uapplx and uhostx.

	The parameter noauto can be used to disable automatic detection of
	devices:

	noauto=1 (disable automatic detection)
	noauto=0 (Enable automatic detectio.  This is the default value.)

	The values in base0 - base3 will be copied to the bases array when
	the module is loaded. 

	To configure the interface(s), run the LINUX command(s):

	ifconfig ci0 ...
	ifconfig ci1 ...
	ifconfig ci2 ...
	ifconfig ci3 ...

	There is one device structure for each controller in the c7000_devices
	array.  The base address of each controller is in the bases array.
	These arrays parallel each other.  There is also one c7000_controller
	structure for each controller.  This structure is pointed to by field
	priv in the individual device structure. 

	In each c7000_controller, there are embedded c7000_unit structures.
	There is one c7000_unit structure per device number that makes up
	a controller (currently 2 units per controller).
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/irq.h>
 
/*
	Global defines
*/

/*
	Maximum number of controllers.
*/

#define	MAX_C7000	4

/*
	Number of units per controller.
*/

#define	NUNITS		2

/*
	Define indexes of read and write units in the cunits array.
*/

#define	C7000_RD	0
#define	C7000_WR	1

/*
	Number of device buffers.
*/

#define	C7000_MAXBUF	40

/*
	Transmission queue length.
*/

#define	C7000_TXQUEUE_LEN	100
/*
	Size of the IP packet data.
*/

#define	C7000_DATAL	4096

/*
	Size of the read header data.
*/

#define	C7000_READHDRL	4

/*
	Size of read flag byte.
*/

#define	C7000_READFFL	1

/*
	Size of a device buffer.  This is how it is arranged in memory:
	4096 (IP packet data) + 4 (read header) + 1 (read flag) = 4101.
*/

#define	C7000_BUFSIZE	C7000_DATAL + C7000_READHDRL + C7000_READFFL

/*
	Size of sigsmod data.
*/

#define	C7000_SIGSMODL	1

/*
	Flag value that indicates a read was completed in the flag
	field of a c7000_rd_header.
*/

#define	FLAG_FF		0xff
/*
	Size of C7000 sense id data.
*/

#define SIDL		32

/*
	Maximum number of read and write retries.
*/

#define	C7000_MAX_RETRIES	3

/*
	Define sense byte0 value for a box reset.
*/

#define	C7000_BOX_RESET		0x41

/*
	CCW commands.
*/

#define	C7000_WRITE_CCW		0x01	/* normal write */
#define	C7000_READ_CCW		0x02	/* normal read */
#define	C7000_NOOP_CCW		0x03	/* no operation */
#define	C7000_SIGSMOD_CCW	0x05	/* signal status modifier */
#define	C7000_TIC_CCW		0x08	/* transfer in channel */
#define	C7000_READHDR_CCW	0x12	/* read header  */
#define	C7000_READFF_CCW	0x22	/* read FF flag */
#define	C7000_SID_CCW		0xe4	/* sense identification */

/*
	Control commands.
*/

#define	C7000_SYS_VALIDATE		1
#define C7000_SYS_VALIDATE_RESP		2
#define	C7000_CONN_REQ			33
#define C7000_CONN_RESP			34
#define C7000_CONN_CONFRM		35
#define C7000_DISCONN			36
#define C7000_BOXERROR			65

/*
	State machine values.
*/

#define	C7000_INIT	1
#define C7000_HALT	2
#define C7000_SID	3
#define C7000_SYSVAL	4
#define	C7000_CONNECT	5
#define C7000_READY	6
#define C7000_READ	7
#define C7000_WRITE	8
#define	C7000_DISC	9
#define C7000_STOP	10
#define	C7000_STOPPED	11
#define C7000_ERROR	12

/*
	The lower subchannel is used for read operations and the one that is
	one higher is used for write operations.

	Both subchannels are initially in state C7000_INIT.  A transition to
	state C7000_HALT occurs when halt_IO is issued on each.  When the
	halts completes a transition to state C7000_SID occurs and a channel
	program is issued to do a sense identification on both subchannels.

	When the sense identification completes, the state C7000_SYSVAL is
	entered on the read subchannel.  A read channel program is issued.

	When the sense identification completes, the write subchannel enters
	state C7000_SYSVAL and a system validation request is written.  The
	read subchannel is also put into this state.
	
	When both the system validation response is read and an inbound system
	validation request is read, the inbound system validation request is
	responded to and both subchannels enter the C7000_CONNECT state.

	A read channel program is posted to look for the inbound connect
	request.  When that is received a connection confirmation is written.
	The state of both subchannels is then changed to C7000_READY.  A
	read channel program is then posted and the state is changed to
	C7000_READ.  When a read completes, the packet is sent to the higher
	layers and the read channel program is restarted.

	When there is a packet to be written, state C7000_WRITE is entered
	and a channel program is issued to write the data.  The subchannel
	is in state C7000_READY when there is nothing to be written.

	When the stop method is executed, a disconnect message is sent and
	the state is changed to C7000_DISC in both subchannels.  A halt_IO
	will be issued to both subchannels and state C7000_STOP will be entered.
	When the halt IO completes, state C7000_STOPPED will be set. 

	State C7000_ERROR is set when an error occurs in the interrupt
	routine.  Recycle the interface (ifconfig down / ifconfig up)
	to reset this state.
*/

/*
	Results from c7000_check_csw.
*/

enum	c7000_rupt {
	C7000_NORMAL,
	C7000_CHANERR,
	C7000_UCK,
	C7000_UCK_RESET,
	C7000_UE,
	C7000_ATTN,
	C7000_BUSY,
	C7000_OTHER
};

/*
	Bits set in device structure tbusy field.
*/

#define	TB_TX		0	/* sk buffer handling in progress */
#define	TB_STOP		1	/* network device stop in progress */
#define	TB_RETRY	2	/* retry in progress */
#define	TB_NOBUFFER	3	/* no buffer on free queue */

/*
	Bit in c7000_unit.flag_a that indicates the bh routine is busy.
*/

#define	C7000_BH_ACTIVE	0

#define CPrintk(level, args...) \
	if (level <= dbg) \
		printk(args)

/*
	Maximum length of a system validation string.
*/

#define	NAMLEN	8

#define	Err_Conn_Confirm	1
#define	Err_Names_not_Matched	166
#define	Err_C7000_NOT_READY	167
#define	Err_Duplicate		170
#define	Err_Closing		171
#define	Err_No_Such_App		172
#define	Err_Host_Not_Ready	173
#define	Err_CLOSING		174
#define	Err_Dup_Link		175
#define	Err_Wrong_Version	179
#define	Err_Wrong_Frame_Size	180

/*
	Define a macro to extract the logical link identifier from
	the c7000 read header command field.
*/

#define	C7000_LINKID(cmd)	((unsigned char)cmd >> 3)

/*
	Define the control unit type for a Cisco 7000.
*/

#define	C7000_CU_TYPE		0x3088

/*
	Define the control unit model for a Cisco 7000.
*/

#define	C7000_CU_MODEL		0x61

/*
	Define the default system validate parameters (lapplx,
	lhostx, uapplx, uhostx).
*/

#define	C7000_DFLT_LAPPL	"TCPIP"
#define	C7000_DFLT_LHOST	"UTS"
#define	C7000_DFLT_UAPPL	"TCPIP"
#define	C7000_DFLT_UHOST	"C7011"

/*
	Global variables.
*/

/*
	Base device addresses of the controllers.
*/

static int	base0 = -1;
static int	base1 = -1;
static int	base2 = -1;
static int	base3 = -1;

static int	bases[MAX_C7000];

/*
	Local application names.
*/

static char	*lappl0;
static char	*lappl1;
static char	*lappl2;
static char	*lappl3;

/*
	Local host names.
*/

static char	*lhost0;
static char	*lhost1;
static char	*lhost2;
static char	*lhost3;

/*
	Unit application names.
*/

static char	*uappl0;
static char	*uappl1;
static char	*uappl2;
static char	*uappl3;

/*
	Unit hosts names.
*/

static char	*uhost0;
static char	*uhost1;
static char	*uhost2;
static char	*uhost3;

/*
	Debugging level (higher numbers emit lower priority messages).
*/

static unsigned int	dbg = 0;

/*
	Parameter that controls auto detection.
*/

static int	noauto = 0;

/*
	Interface names.
*/

static char	ifnames[MAX_C7000][8] = {"ci0", "ci1", "ci2", "ci3"};

/*
	One device structure per controller.
*/

/* RBH Try out the new code for 2.4.0 */
#define NEWSTUFF

#ifdef NEWSTUFF
#define STRUCT_NET_DEVICE struct net_device
#else
#define STRUCT_NET_DEVICE struct device
#endif

STRUCT_NET_DEVICE	c7000_devices[MAX_C7000];

/*
	Scratch variable filled in with controller name.
*/

static char	*controller;

/*
	Identify parameters that can be passed on the LINUX insmod command.
*/

MODULE_AUTHOR("Robert Scardapane (UTS Global)");
MODULE_DESCRIPTION("Network module for Cisco 7000 box.");

MODULE_PARM(base0, "1i");
MODULE_PARM_DESC(base0, "Base unit address for 1st C7000 box.");
MODULE_PARM(base1, "1i");
MODULE_PARM_DESC(base1, "Base unit address for 2nd C7000 box.");
MODULE_PARM(base2, "1i");
MODULE_PARM_DESC(base2, "Base unit address for 3rd C7000 box.");
MODULE_PARM(base3, "1i");
MODULE_PARM_DESC(base3, "Base unit address for 4th C7000 box.");

MODULE_PARM(lappl0, "s");
MODULE_PARM_DESC(lappl0, "Application name for 1st C7000 box.");
MODULE_PARM(lappl1, "s");
MODULE_PARM_DESC(lappl1, "Application name for 2nd C7000 box.");
MODULE_PARM(lappl2, "s");
MODULE_PARM_DESC(lappl2, "Application name for 3rd C7000 box.");
MODULE_PARM(lappl3, "s");
MODULE_PARM_DESC(lappl3, "Application name for 4th C7000 box.");

MODULE_PARM(lhost0, "s");
MODULE_PARM_DESC(lhost0, "Host name for 1st C7000 box.");
MODULE_PARM(lhost1, "s");
MODULE_PARM_DESC(lhost1, "Host name for 2nd C7000 box.");
MODULE_PARM(lhost2, "s");
MODULE_PARM_DESC(lhost2, "Host name for 3rd C7000 box.");
MODULE_PARM(lhost3, "s");
MODULE_PARM_DESC(lhost3, "Host name for 4th C7000 box.");

MODULE_PARM(uhost0, "s");
MODULE_PARM_DESC(uhost0, "Unit name for 1st C7000 box.");
MODULE_PARM(uhost1, "s");
MODULE_PARM_DESC(uhost1, "Unit name for 2nd C7000 box.");
MODULE_PARM(uhost2, "s");
MODULE_PARM_DESC(uhost2, "Unit name for 3rd C7000 box.");
MODULE_PARM(uhost3, "s");
MODULE_PARM_DESC(uhost3, "Unit name for 4th C7000 box.");

MODULE_PARM(uappl0, "s");
MODULE_PARM_DESC(uappl0, "Unit application name for 1st C7000 box.");
MODULE_PARM(uappl1, "s");
MODULE_PARM_DESC(uappl1, "Unit application name for 2nd C7000 box.");
MODULE_PARM(uappl2, "s");
MODULE_PARM_DESC(uappl2, "Unit application name for 3rd C7000 box.");
MODULE_PARM(uappl3, "s");
MODULE_PARM_DESC(uappl3, "Unit application name for 4th C7000 box.");

MODULE_PARM(dbg, "1i");
MODULE_PARM_DESC(dbg, "Message level for debugging.");

MODULE_PARM(noauto, "1i");
MODULE_PARM_DESC(noauto, "Control automatic detection of unit base addresses.");

/*
	Structure used to manage unit buffers.
*/

struct	c7000_buffer {
	ccw1_t			ccws[7];	/* channel program */
	struct	c7000_buffer	*next;		/* list pointer */
	char			*data;		/* pointer to actual data */
	int			len;		/* length of the data */
};

/*
	C7000 Control Block.
 */

struct	c7000_control_blk {
	unsigned char	cmd;
	unsigned char	ver;
	unsigned char	link_id;
	unsigned char	correlator;
	unsigned char	ret_code;
	unsigned char	resvd1[3];
	unsigned char	unitname[NAMLEN];
	unsigned char	hostname[NAMLEN];
	unsigned short	rdsize;		/* read frame size   */
	unsigned short	wrtsize;	/* write frame size  */
	unsigned char	resvd2[4];
};

/*
	Per unit structure contained within the c7000_controller structure.
*/

struct	c7000_unit {
	ccw1_t				ccws[5];	/* control ccws */
	int				devno;		/* device number */
	int				irq;		/* subchannel number */
	int				IO_active;	/* IO activity flag */
	int				state;		/* fsm state */
	int				retries;	/* retry counter */
	unsigned long			flag_a;		/* bh activity flag */
	devstat_t			devstat;	/* device status */
#ifdef NEWSTUFF
	wait_queue_head_t		wait;		/* sleep q head */
#else
	struct wait_queue		*wait;		/* sleep q pointer */
#endif
	struct c7000_controller		*cntlp;		/* controller pointer */
	struct c7000_buffer		*free;		/* free buffer anchor */
	struct c7000_buffer		*proc_head;	/* proc head */
	struct c7000_buffer		*proc_tail;	/* proc tail */
	struct c7000_buffer		*bh_head;	/* bh head */
	struct c7000_buffer		*bh_tail;	/* bh tail */
	struct tq_struct		tq;		/* bh scheduling */
	char				senseid[SIDL];	/* sense id data */
	struct c7000_control_blk	control_blk;	/* control block */
	unsigned char			sigsmod;	/* sigsmod flag */
	unsigned char			readhdr[4];	/* read header */
	unsigned char			readff;		/* readff flag */
};

/*
	Private structure pointed to by dev->priv.
*/

struct c7000_controller {
	struct	net_device_stats	stats;		/* statistics */
	STRUCT_NET_DEVICE		*dev;		/* -> device struct */
	unsigned int			base_addr;	/* base address */
	char				lappl[NAMLEN];	/* local appl */
	char				lhost[NAMLEN];	/* local host */
	char				uappl[NAMLEN];	/* unit appl */
	char				uhost[NAMLEN];	/* unit host */
	unsigned char			version;	/* version = 2 */
	unsigned char			linkid;		/* link id */
	struct	c7000_unit		cunits[NUNITS];	/* embedded units */
#ifdef NEWSTUFF
	int				tbusy;
#endif
};

/*
	This is the structure returned by the C7000_READHDR_CCW.
*/

struct	c7000_rd_header {
	unsigned short	len;	/* packet length */
	unsigned char	cmd;	/* command code */
	unsigned char	flag;	/* flag */
};

/*
	Set the device structure transmission busy flag.
*/

#ifdef NEWSTUFF
#define c7000_set_busy(dev) netif_stop_queue(dev)
#else
static __inline__ void
c7000_set_busy(STRUCT_NET_DEVICE *dev)
{
	dev->tbusy = 1;
	eieio();
	return;
}
#endif
	
/*
	Clear the device structure transmission busy flag.
*/

#ifdef NEWSTUFF
#define c7000_clear_busy(dev) netif_wake_queue(dev)
#else
static __inline__ void
c7000_clear_busy(STRUCT_NET_DEVICE *dev)
{
	dev->tbusy = 0;
	eieio();
	return;
}
#endif

/*
	Extract the device structure transmission busy flag.
*/

#ifdef NEWSTUFF
#define c7000_check_busy(dev) netif_queue_stopped(dev)
#else
static __inline__ int
c7000_check_busy(STRUCT_NET_DEVICE *dev)
{
	eieio();
	return(dev->tbusy);
}
#endif

/*
	Set a bit in the device structure transmission busy flag.
*/

static __inline__ void
c7000_setbit_busy(int nr, STRUCT_NET_DEVICE *dev)
{
#ifdef NEWSTUFF
	netif_stop_queue(dev);
	test_and_set_bit(nr, &((struct c7000_controller *)dev->priv)->tbusy);
#else
	set_bit(nr, (void *)&dev->tbusy);
#endif
	return;
}

/*
	Clear a bit in the device structure transmission busy flag.
*/

static __inline__ void
c7000_clearbit_busy(int nr, STRUCT_NET_DEVICE *dev)
{
#ifdef NEWSTUFF
	clear_bit(nr, &((struct c7000_controller *)dev->priv)->tbusy);
	netif_wake_queue(dev);
#else
	clear_bit(nr, (void *)&dev->tbusy);
#endif
	return;
}

/*
	Test and set a bit in the device structure transmission busy flag.
*/

static __inline__ int
c7000_ts_busy(int nr, STRUCT_NET_DEVICE *dev)
{
#ifdef NEWSTUFF
	netif_stop_queue(dev);
	return test_and_set_bit(nr, &((struct c7000_controller *)dev->priv)->tbusy);
#else
	return(test_and_set_bit(nr, (void *)&dev->tbusy));
#endif
}

/*
	Set the C7000 controller in the error state.
*/

static void
c7000_error(struct c7000_controller *ccp)
{
	int			i;
	struct	c7000_unit	*cup;
	STRUCT_NET_DEVICE	*dev = ccp->dev;

	for (i = 0; i < NUNITS; i++) {
		cup = &ccp->cunits[i];
		cup->state = C7000_ERROR;
	}

	if (dev != NULL)
#ifdef NEWSTUFF
		/* RBH XXX Should we be doing this? */
		dev->state &= ~__LINK_STATE_START;
#else
		dev->flags &= ~IFF_RUNNING;
#endif

	CPrintk(0, "c7000: c7000_error: base unit 0x%x is down\n", ccp->base_addr);
	return;
}

/*
	Based on the SENSE ID information, fill in the
	controller name.  Note that this is the SENSE ID
	information saved by LINUX/390 at boot time.
*/

static int
c7000_check_type(senseid_t *id)
{

	switch (id->cu_type) {

		case C7000_CU_TYPE:

			if (id->cu_model == C7000_CU_MODEL) {
				controller = "C7000  ";
 				return(0);
			}

			break;

                default:
			break;
	}

	return(-1);
}

/*
	Check the device information for the controller.
*/

static int
c7000_check_devices(int devno)
{
	int		i;
	s390_dev_info_t	temp;

	/*
		Get the SENSE ID information for each device.
	*/

	for (i = devno; i < (devno + NUNITS); i++) {

		if (get_dev_info_by_devno(devno, &temp) != 0)
			return(-1);
		
		if (c7000_check_type(&temp.sid_data) == -1)
			return(-1);
	}

	CPrintk(1, "c7000: c7000_check_devices: device type is %s\n", controller);
	return(0);
}

/*
	Issue a halt I/O to device pointed to by cup.
*/

static int
c7000_haltio(struct c7000_unit *cup)
{
	__u32			parm;
	__u8			flags = 0x00;
	__u32			saveflags;
	DECLARE_WAITQUEUE(wait, current);
	int			rc;

	s390irq_spin_lock_irqsave(cup->irq, saveflags);
	parm = (unsigned long)cup;

	if ((rc = halt_IO(cup->irq, parm, flags)) != 0) {
		s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
		return(rc);
	}

	/*
		Wait for the halt I/O to finish.
	*/

	add_wait_queue(&cup->wait, &wait);
	current->state = TASK_UNINTERRUPTIBLE;
	s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
	schedule();
	remove_wait_queue(&cup->wait, &wait);
	return(0);
}

/*
	Issue a start I/O to device pointed to by cup.
*/

static int
c7000_doio(struct c7000_unit *cup)
{
	__u32			parm;
	__u8			flags = 0x00;
	__u32			saveflags;
	DECLARE_WAITQUEUE(wait, current);
	int			rc;

	/*
		Do no further I/O while the device is in the ERROR, STOP
		or STOPPED state.
	*/

	if (cup->state == C7000_ERROR || cup->state == C7000_STOP || cup->state == C7000_STOPPED)
		return(-1);

	s390irq_spin_lock_irqsave(cup->irq, saveflags);
	parm = (unsigned long)cup;

	if ((rc = do_IO(cup->irq, &cup->ccws[0], parm, 0xff, flags)) != 0) {
		s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
		return(rc);
	}
	
	/*
		Wait for the I/O to complete.
	*/

	add_wait_queue(&cup->wait, &wait);
	current->state = TASK_UNINTERRUPTIBLE;
	s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
	schedule();
	remove_wait_queue(&cup->wait, &wait);

	/*
		Interrupt handling may have marked the device in ERROR.
	*/

	if (cup->state == C7000_ERROR)
		return(-1);

	return(0);
}

/*
	Build a channel program to do a sense id channel program.
*/

static void
c7000_bld_senseid_chpgm(struct c7000_unit *cup)
{
	ccw1_t	*ccwp;

	ccwp = &cup->ccws[0];
	ccwp->cmd_code = C7000_SID_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(&cup->senseid);
	ccwp->count = SIDL;
	ccwp++;
	ccwp->cmd_code = C7000_NOOP_CCW;
	ccwp->flags = CCW_FLAG_SLI;
	ccwp->cda = (__u32)NULL;
	ccwp->count = 1;
	return;
}

/*
	Build a channel program to write a control message.
*/

static void
c7000_bld_wrtctl_chpgm(struct c7000_unit *cup)
{
	ccw1_t	*ccwp;

	ccwp = &cup->ccws[0];
	ccwp->cmd_code = C7000_WRITE_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(&cup->control_blk);
	ccwp->count = sizeof(struct c7000_control_blk);
	ccwp++;
	ccwp->cmd_code = C7000_READFF_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(&cup->readff);
	ccwp->count = C7000_READFFL;
	ccwp++;
	ccwp->cmd_code = C7000_TIC_CCW;
	ccwp->flags = 0;
	ccwp->cda = (__u32)virt_to_phys(ccwp + 1);
	ccwp->count = 0;
	ccwp++;
	ccwp->cmd_code = C7000_NOOP_CCW;
	ccwp->flags = CCW_FLAG_SLI;
	ccwp->cda = (__u32)NULL;
	ccwp->count = 1;
	return;
}

/*
	Build a write channel program to write the indicated buffer.
*/

static void
c7000_bld_wrt_chpgm(struct c7000_unit *cup, struct c7000_buffer *buf)
{
	ccw1_t				*ccwp;
	struct	c7000_controller	*ccp = cup->cntlp;

	ccwp = &buf->ccws[0];
	ccwp->cmd_code = C7000_WRITE_CCW | (ccp->linkid << 3);
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(buf->data);
	ccwp->count = buf->len;
	ccwp++;
	ccwp->cmd_code = C7000_READFF_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(buf->data + C7000_DATAL + C7000_READHDRL);
	ccwp->count = C7000_READFFL;
	ccwp++;
	ccwp->cmd_code = C7000_TIC_CCW;
	ccwp->flags = 0;
	ccwp->cda = (__u32)virt_to_phys(ccwp + 1);
	ccwp->count = 0;
	ccwp++;
	ccwp->cmd_code = C7000_NOOP_CCW;
	ccwp->flags = (CCW_FLAG_SLI);
	ccwp->cda = (__u32)NULL;
	ccwp->count = 1;
	return;
}

/*
	Build a channel program to read a control message.
*/

static void
c7000_bld_readctl_chpgm(struct c7000_unit *cup)
{
	ccw1_t	*ccwp;

	ccwp = &cup->ccws[0];
	ccwp->cmd_code = C7000_READ_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(&cup->control_blk);
	ccwp->count = sizeof(struct c7000_control_blk);
	ccwp++;
	ccwp->cmd_code = C7000_READHDR_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(&cup->readhdr);
	ccwp->count = C7000_READHDRL;
	ccwp++;
	ccwp->cmd_code = C7000_SIGSMOD_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(&cup->sigsmod);
	ccwp->count = C7000_SIGSMODL;
	ccwp++;
	ccwp->cmd_code = C7000_TIC_CCW;
	ccwp->flags = 0;
	ccwp->cda = (__u32)virt_to_phys(ccwp + 1);
	ccwp->count = 0;
	ccwp++;
	ccwp->cmd_code = C7000_NOOP_CCW;
	ccwp->flags = (CCW_FLAG_SLI);
	ccwp->cda = (__u32)NULL;
	ccwp->count = 1;
	return;
}

/*
	Build a channel program to read the indicated buffer.
*/

static void
c7000_bld_read_chpgm(struct c7000_unit *cup, struct c7000_buffer *buf)
{
	ccw1_t	*ccwp;

	ccwp = &buf->ccws[0];
	ccwp->cmd_code = C7000_READ_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(buf->data);
	ccwp->count = C7000_DATAL;
	ccwp++;
	ccwp->cmd_code = C7000_READHDR_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(buf->data + C7000_DATAL);
	ccwp->count = C7000_READHDRL;
	ccwp++;
	ccwp->cmd_code = C7000_SIGSMOD_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC);
	ccwp->cda = (__u32)virt_to_phys(&cup->sigsmod);
	ccwp->count = C7000_SIGSMODL;
	ccwp++;
	ccwp->cmd_code = C7000_TIC_CCW;
	ccwp->flags = 0;
	ccwp->cda = (__u32)virt_to_phys(ccwp + 3);
	ccwp->count = 0;
	ccwp++;
	ccwp->cmd_code = C7000_READFF_CCW;
	ccwp->flags = (CCW_FLAG_SLI | CCW_FLAG_CC | CCW_FLAG_PCI);
	ccwp->cda = (__u32)virt_to_phys(&cup->readff);
	ccwp->count = C7000_READFFL;
	ccwp++;
	ccwp->cmd_code = C7000_TIC_CCW;
	ccwp->flags = 0;
	ccwp->cda = (__u32)virt_to_phys(ccwp + 1);
	ccwp->count = 0;
	ccwp++;
	ccwp->cmd_code = C7000_NOOP_CCW;
	ccwp->flags = (CCW_FLAG_SLI);
	ccwp->cda = (__u32)NULL;
	ccwp->count = 1;
	return;
}

/*
	Allocate buffer structure headers and buffers for all units
	A return value of 0 means that all allocations worked.  A -1
	means that an allocation failed.  It is expected that the caller
	will call c7000_free_buffers when -1 is returned.
*/

static int
c7000_alloc_buffers(STRUCT_NET_DEVICE *dev)
{
	int				i;
	int				j;
	char				*data;
	struct	c7000_buffer		*bufptr;
	struct	c7000_controller	*ccp = (struct c7000_controller *) dev->priv;
	struct	c7000_unit		*cup;
	
	for (i = 0; i < NUNITS; i++) {
		cup = &ccp->cunits[i];
		cup->free = NULL;

		for (j = 0; j < C7000_MAXBUF; j++) {
			bufptr = kmalloc(sizeof(struct c7000_buffer), GFP_KERNEL);
			data = kmalloc(C7000_BUFSIZE, GFP_KERNEL);

			if (bufptr == NULL)
			{
				if(data)
					kfree(data);
				return(-1);
			}

			/*
				Place filled in buffer header on free anchor.
			*/

			bufptr->next = cup->free;
			bufptr->data = data;
			bufptr->len = 0;
			cup->free = bufptr;

			if (data == NULL)
				return(-1);

			memset(data, '\0', C7000_BUFSIZE);
		}

	}

	CPrintk(1, "c7000: c7000_alloc_buffers: allocated buffers for base unit 0x%lx\n", dev->base_addr);
	return(0);
}

/*
	Free buffers on a chain.
*/

static void
c7000_free_chain(struct c7000_buffer *buf)
{
	char			*data;
	struct	c7000_buffer	*bufptr = buf;
	struct	c7000_buffer	*tmp;

	while (bufptr != NULL) {
		data = bufptr->data;

		if (data != NULL)
			kfree(data);

		tmp = bufptr;
		bufptr = bufptr->next;
		kfree(tmp);
	}

	return;
}

/*
	Free buffers on all possible chains for all units.
*/

static void
c7000_free_buffers(STRUCT_NET_DEVICE *dev)
{
	int				i;
	struct	c7000_controller	*ccp = (struct c7000_controller *) dev->priv;
	struct	c7000_unit	*cup;
	
	for (i = 0; i < NUNITS; i++) {
		cup = &ccp->cunits[i];
		c7000_free_chain(cup->free);
		cup->free = NULL;
		c7000_free_chain(cup->proc_head);
		cup->proc_head = cup->proc_tail = NULL;
		c7000_free_chain(cup->bh_head);
		cup->bh_head = cup->bh_tail = NULL;
	}

	CPrintk(1, "c7000: c7000_free_buffers: freed buffers for base unit 0x%lx\n", dev->base_addr);
	return;
}

/*
	Obtain a free buffer.  Return a pointer to the c7000_buffer
	structure OR NULL.
*/

struct c7000_buffer *
c7000_get_buffer(struct c7000_unit *cup) 
{
	struct	c7000_buffer	*buf;

	buf = cup->free;

	if (buf == NULL)
		return(NULL);

	cup->free = buf->next;
	buf->next = NULL;
	return(buf);
}

/*
	Release a buffer to the free list.
*/

void
c7000_release_buffer(struct c7000_unit *cup, struct c7000_buffer *buf)
{
	struct	c7000_buffer	*tmp;

	tmp = cup->free;
	cup->free = buf;
	buf->next = tmp;
	return;
}

/*
	Queue a buffer on the end of the processing (proc) chain.
*/

void
c7000_queue_buffer(struct c7000_unit *cup, struct c7000_buffer *buf)
{
	buf->next = NULL;

	if (cup->proc_head == NULL) {
		cup->proc_head = cup->proc_tail = buf;
		return;
	}

	cup->proc_tail->next = buf;
	cup->proc_tail = buf;
	return;
}

/*
	Dequeue a buffer from the start of the processing (proc) chain.
*/

struct c7000_buffer *
c7000_dequeue_buffer(struct c7000_unit *cup)
{
	struct	c7000_buffer	*buf = cup->proc_head;

	if (buf == NULL)
		return(NULL);

	cup->proc_head = buf->next;

	if (cup->proc_head == NULL)
		cup->proc_tail = NULL;

	buf->next = NULL;
	return(buf);
}

/*
	Queue a buffer on the end of the bh routine chain.
*/

void
c7000_queue_bh_buffer(struct c7000_unit *cup, struct c7000_buffer *buf)
{
	buf->next = NULL;

	if (cup->bh_head == NULL) {
		cup->bh_head = cup->bh_tail = buf;
		return;
	}

	cup->bh_tail->next = buf;
	cup->bh_tail = buf;
	return;
}

/*
	Dequeue a buffer from the start of the bh routine chain.
*/

struct c7000_buffer *
c7000_dequeue_bh_buffer(struct c7000_unit *cup)
{
	struct	c7000_buffer	*buf = cup->bh_head;

	if (buf == NULL)
		return(NULL);

	cup->bh_head = buf->next;

	if (cup->bh_head == NULL)
		cup->bh_tail = NULL;

	buf->next = NULL;
	return(buf);
}

/*
	Build up a list of buffers to read.  Each buffer is described
	by one c7000_buffer structure.  The c7000_buffer structure
	contains a channel segment that will read that one buffer.
	The channel program segments are chained together via TIC
	CCWS.
*/

static int
c7000_bld_read_chain(struct c7000_unit *cup)
{
	struct	c7000_buffer	*buf, *pbuf = NULL;
	struct	c7000_rd_header	*head;
	int			num = 0;

	while (cup->free != NULL) {

		/*
			Obtain a buffer for a read channel segment.
		*/

		if ((buf = c7000_get_buffer(cup)) == NULL) {
			CPrintk(0, "c7000: c7000_bld_read_chain: can not obtain a read buffer for unit 0x%x\n", cup->devno);
			return(-ENOMEM);
		}

		num++;
		buf->len = 0;

		/*
			Clear out the read header flag.
		*/

		head = (struct c7000_rd_header *)(buf->data + C7000_DATAL);
		head->flag = 0x00;
		c7000_queue_buffer(cup, buf);

		/*
			Build the read channel program segment.
		*/

		c7000_bld_read_chpgm(cup, buf);

		/*
			Chain the prior (if any) channel program segment to
			this one.
		*/

		if (pbuf != NULL)
			pbuf->ccws[3].cda = pbuf->ccws[5].cda = (__u32)virt_to_phys(&buf->ccws[0]);

		pbuf = buf;
	}

	CPrintk(1, "c7000: c7000_bld_read_chain: chained %d buffers for unit 0x%x\n", num, cup->devno);
	return(0);
}

/*
	Build up a list of buffers to write.  Each buffer is described
	by one c7000_buffer structure.  The c7000_buffer structure
	contains a channel segment that will write that one buffer.
	The channel program segments are chained together via TIC
	CCWS.
*/

static void
c7000_bld_wrt_chain(struct c7000_unit *cup)
{
	struct	c7000_buffer	*buf = cup->proc_head, *pbuf = NULL;
	int			num = 0;

	while (buf != NULL) {
		c7000_bld_wrt_chpgm(cup, buf);

		/*
			Chain the channel program segments together.
		*/

		if (pbuf != NULL)
			pbuf->ccws[2].cda = (__u32)virt_to_phys(&buf->ccws[0]);

		pbuf = buf;
		buf = buf->next;
		num++;
	}

	CPrintk(1, "c7000: c7000_bld_wrt_chain: chained %d buffers for unit 0x%x\n", num, cup->devno);
	return;
}

/*
	Interrupt handler bottom half (bh) routine.
	Process all of the buffers on the c7000_unit bh chain.
	The bh chain is populated by the interrupt routine when
	a READ channel program completes on a buffer.
*/

static void
c7000_irq_bh(struct c7000_unit *cup)
{
	struct	c7000_buffer		*buf, *pbuf;
	struct	c7000_rd_header		*head;	
	struct	sk_buff			*skb;
	struct	c7000_controller	*ccp;
	STRUCT_NET_DEVICE		*dev;
	int				rc;
	__u16				data_length;
	__u32				parm;
	__u8				flags = 0x00;
	__u32				saveflags;

	ccp = cup->cntlp;
	dev = ccp->dev;

	s390irq_spin_lock_irqsave(cup->irq, saveflags);

	/*
		Process all buffers sent to bh by the interrupt routine.
	*/

	while (cup->bh_head != NULL) {
		buf = c7000_dequeue_bh_buffer(cup);

		/*
			Deference the data as a c7000 header.
		*/

		head = (struct c7000_rd_header *)(buf->data + C7000_DATAL);

		/*
			If it is a control message, release the buffer and 
			continue the loop.
		*/

		if (C7000_LINKID(head->cmd) == 0) {
			CPrintk(0, "c7000: c7000_irq_bh: unexpected control command %d on unit 0x%x\n", head->cmd, cup->devno);
			c7000_release_buffer(cup, buf);
			continue;
		}
			
		/*
			Allocate a socket buffer.
		*/

		data_length = head->len;
		skb = dev_alloc_skb(data_length);

		/*
			Copy the data to the skb.
			Send it to the upper layers.
		*/

		if (skb != NULL) {
			memcpy(skb_put(skb, data_length), buf->data, data_length);
			skb->dev = dev;
			skb->protocol = htons(ETH_P_IP);
			skb->pkt_type = PACKET_HOST;
			skb->mac.raw = skb->data;
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			netif_rx(skb);
			ccp->stats.rx_packets++;
		} else {
			CPrintk(0, "c7000: c7000_irq_bh: can not allocate a skb for unit 0x%x\n", cup->devno);
			ccp->stats.rx_dropped++;
		}

		/*
			Rechain the buffer on the processing list.
		*/

		head->flag = 0x00;
		buf->len = 0;
		pbuf = cup->proc_tail;
		c7000_queue_buffer(cup, buf);

		/*
			Rechain the buffer on the running channel program.
		*/

		if (pbuf != NULL)
			pbuf->ccws[3].cda = pbuf->ccws[5].cda = (__u32)virt_to_phys(&buf->ccws[0]);

	}

	/*
		Restart the READ channel program if IO_active is 0.
	*/

	if (test_and_set_bit(0, (void *)&cup->IO_active) == 0) {

		if ((rc = c7000_bld_read_chain(cup)) != 0) {
			CPrintk(0, "c7000: c7000_irq_bh: can not build read chain for unit 0x%x, return code %d\n", cup->devno, rc);
			c7000_error(cup->cntlp);
			clear_bit(C7000_BH_ACTIVE, (void *)&cup->flag_a);
			s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
			return;
		}

		parm = (__u32)cup;
		cup->state = C7000_READ;

		if ((rc = do_IO(cup->irq, &cup->proc_head->ccws[0], parm, 0xff, flags)) != 0) {
			CPrintk(0, "c7000: c7000_irq_bh: can not start READ IO to unit 0x%x, return code %d\n", cup->devno, rc);
			c7000_error(cup->cntlp);
			clear_bit(C7000_BH_ACTIVE, (void *)&cup->flag_a);
			s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
			return;
		}

		CPrintk(1, "c7000: c7000_irq_bh: started READ IO to unit 0x%x\n", cup->devno);
	}
			
	/*
		Clear the bh active indication.
	*/

	clear_bit(C7000_BH_ACTIVE, (void *)&cup->flag_a);
	s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
	return;
}

/*
	Send a system validate control command to a unit.
*/

static int
c7000_send_sysval(struct c7000_unit *cup)
{
	int				rc;
	struct	c7000_controller	*ccp = cup->cntlp;
	struct	c7000_control_blk	*ctlblkp = &(cup->control_blk);

	CPrintk(1, "c7000: c7000_send_sysval: send sysval for device 0x%x\n", cup->devno);

	/*
		Build the system validate control message.
	*/

	memset(ctlblkp, '\0', sizeof(struct c7000_control_blk));
	ctlblkp->cmd = C7000_SYS_VALIDATE;
	ctlblkp->correlator = 0;
	ctlblkp->link_id = ccp->linkid;
	ctlblkp->ver = ccp->version; 
	memcpy(ctlblkp->hostname, ccp->lhost, NAMLEN);
	memcpy(ctlblkp->unitname, ccp->uhost, NAMLEN);
	ctlblkp->rdsize = C7000_DATAL;
        ctlblkp->wrtsize = C7000_DATAL;

	/*
		Build the channel program.
	*/

	c7000_bld_wrtctl_chpgm(cup);

	/*
		Do the IO and wait for write to complete.
	*/

	if ((rc = c7000_doio(cup)) != 0) {
		CPrintk(0, "c7000: c7000_send_sysval failed with rc = %d for unit 0x%x\n", rc, cup->devno);
		return(-1);
	}

	return(0);
}

/*
	Send a system validate response control command to a unit.
*/

static int
c7000_send_sysval_resp(struct c7000_unit *cup, unsigned char correlator, int ret_code)
{
	int				rc;
	struct	c7000_controller	*ccp = cup->cntlp;
	struct	c7000_control_blk	*ctlblkp = &(cup->control_blk);

	CPrintk(1, "c7000: c7000_send_sysval_resp: send sysval response for device 0x%x\n", cup->devno);

	/*
		Build the system validate response control message.
	*/

	memset(ctlblkp, '\0', sizeof(struct c7000_control_blk));
	ctlblkp->cmd = C7000_SYS_VALIDATE_RESP;
	ctlblkp->correlator = correlator;
	ctlblkp->ret_code = ret_code;
	ctlblkp->link_id = ccp->linkid;
	ctlblkp->ver = ccp->version; 
	memcpy(ctlblkp->hostname, ccp->lhost, NAMLEN);
	memcpy(ctlblkp->unitname, ccp->uhost, NAMLEN);
	ctlblkp->rdsize = C7000_DATAL;
        ctlblkp->wrtsize = C7000_DATAL;

	/*
		Build the channel program.
	*/

	c7000_bld_wrtctl_chpgm(cup);

	/*
		Do the IO and wait for write to complete.
	*/

	if ((rc = c7000_doio(cup)) != 0) {
		CPrintk(0, "c7000: c7000_send_sysval_resp failed with rc = %d for unit 0x%x\n", rc, cup->devno);
		return(-1);
	}

	return(0);
}

/*
	Check the information read in a SYS_VALIDATE control message.
*/

static int
c7000_checkinfo(struct c7000_unit *cup)
{
	struct	c7000_controller	*ccp = cup->cntlp;
 	struct	c7000_control_blk	*ctlblkp = &cup->control_blk;
	int				ret_code = 0;

	if (memcmp(ccp->lhost, ctlblkp->hostname, NAMLEN) ||
		memcmp(ccp->uhost, ctlblkp->unitname, NAMLEN))
		ret_code = Err_Names_not_Matched;

	if (ctlblkp->ver != ccp->version)
		ret_code = Err_Wrong_Version;

        if ((ctlblkp->rdsize < C7000_DATAL) || (ctlblkp->wrtsize < C7000_DATAL))
		ret_code = Err_Wrong_Frame_Size;

	if (ret_code != 0)
		CPrintk(0, "c7000: c7000_checkinfo: ret_code %d for device 0x%x\n", ret_code, cup->devno);

	return(ret_code);
}

/*
	Keep reading until a sysval response comes in or an error.
*/

static int
c7000_get_sysval_resp(struct c7000_unit *cup)
{
	struct	c7000_controller	*ccp = cup->cntlp;
	int				resp = 1;
	int				req = 1;
	int				rc;
	int				ret_code = 0;

	CPrintk(1, "c7000: c7000_get_sysval_resp: get sysval response for unit 0x%x\n", cup->devno);

	/*
		Wait for the response to C7000_SYS_VALIDATE and for an
		inbound C7000_SYS_VALIDATE.
	*/

	while (resp || req) {

		/*
			Build the read channel program.
		*/

		c7000_bld_readctl_chpgm(cup);

		if ((rc = c7000_doio(cup)) != 0) {
			CPrintk(0, "c7000: c7000_get_sysval_resp: failed with rc = %d for unit 0x%x\n", rc, cup->devno);
			return(-1);
		}

		/*
			Process the control message.
		*/

		switch (cup->control_blk.cmd) {

			/*
				Check that response is positive and return
				with success. Otherwise, return with an
				error.
			*/

			case C7000_SYS_VALIDATE_RESP:

				if (cup->control_blk.ret_code == 0)
					resp = 0;
				else {
					CPrintk(0, "c7000: c7000_get_sysval_resp: receive sysval response for device 0x%x, return code %d\n",
						cup->devno,
						cup->control_blk.ret_code);
					return(-1);
				}

				break;

			/*
				Check that the request is reasonable and
				send a SYS_VALIDATE_RESP.  Otherwise,
				return with an error.
			*/

			case C7000_SYS_VALIDATE:
				CPrintk(1, "c7000: c7000_get_sysval_resp: receive sysval for device 0x%x\n", cup->devno);
				req = 0;
				ret_code = c7000_checkinfo(cup);

				if (c7000_send_sysval_resp(&ccp->cunits[C7000_WR], cup->control_blk.correlator, ret_code) != 0)
					return(-1);

				if (ret_code != 0)
					return(-1);

				break;

			/*
				Anything else is unexpected and will result
				in a return with an error.
			*/

			default:
				CPrintk(0, "c7000: c7000_get_sysval_resp: receive unexpected command for device 0x%x, command %d\n", cup->devno, cup->control_blk.cmd);
				return(-1);
				break;
		}

	}

	return(0);
}

/*
	Send a connection confirm control message.
*/

static int
c7000_conn_confrm(struct c7000_unit *cup, unsigned char correlator, int linkid)
{
	int				rc;
	struct	c7000_controller	*ccp = cup->cntlp;
	struct	c7000_control_blk	*ctlblkp = &(cup->control_blk);

	CPrintk(1, "c7000: c7000_conn_confrm: send the connection confirmation message for unit 0x%x\n", cup->devno);

	/*
		Build the connection confirm control message.
	*/

	memset(ctlblkp, '\0', sizeof(struct c7000_control_blk));
	ctlblkp->cmd = C7000_CONN_CONFRM;
	ctlblkp->ver = ccp->version;
	ctlblkp->link_id = linkid;
	ctlblkp->correlator = correlator;
	ctlblkp->rdsize = 0;
	ctlblkp->wrtsize = 0;
	memcpy(ctlblkp->hostname, ccp->lappl, NAMLEN);
	memcpy(ctlblkp->unitname, ccp->uappl, NAMLEN);

	/*
		Build the channel program.
	*/

	c7000_bld_wrtctl_chpgm(cup);

	/*
		Do the IO and wait for write to complete.
	*/

	if ((rc = c7000_doio(cup)) != 0) {
		CPrintk(0, "c7000: c7000_conn_confrm: failed with rc = %d for unit 0x%x\n", rc, cup->devno);
		return(-1);
	}

	return(0);
}

/*
	Send a connection request control message.
*/

static int
c7000_send_conn(struct c7000_unit *cup)
{
	int				rc;
	struct	c7000_controller	*ccp = cup->cntlp;
	struct	c7000_control_blk	*ctlblkp = &(cup->control_blk);

	CPrintk(1, "c7000: c7000_send_conn: send the connection request message for unit 0x%x\n", cup->devno);

	/*
		Build the connection request control message.
	*/

	memset(ctlblkp, '\0', sizeof(struct c7000_control_blk));
	ctlblkp->cmd = C7000_CONN_REQ;
	ctlblkp->ver = ccp->version;
	ctlblkp->link_id = 0;
	ctlblkp->correlator = 0;
	ctlblkp->rdsize = C7000_DATAL;
	ctlblkp->wrtsize = C7000_DATAL;
	memcpy(ctlblkp->hostname, ccp->lappl, NAMLEN);
	memcpy(ctlblkp->unitname, ccp->uappl, NAMLEN);

	/*
		Build the channel program.
	*/

	c7000_bld_wrtctl_chpgm(cup);

	/*
		Do the IO and wait for write to complete.
	*/

	if ((rc = c7000_doio(cup)) != 0) {
		CPrintk(0, "c7000: c7000_send_conn: failed with rc = %d for unit 0x%x\n", rc, cup->devno);
		return(-1);
	}

	return(0);
}

/*
	Send a disconnect control message to the link with the value of
	linkid.
*/

static int
c7000_send_disc(struct c7000_unit *cup, int linkid)
{
	int				rc;
	struct	c7000_controller	*ccp = cup->cntlp;
	struct	c7000_control_blk	*ctlblkp = &(cup->control_blk);

	CPrintk(1, "c7000: c7000_send_disc: send disconnect message for unit 0x%x\n", cup->devno);

	/*
		Build the disconnect control message.
	*/

	memset(ctlblkp, '\0', sizeof(struct c7000_control_blk));
	ctlblkp->cmd = C7000_DISCONN;
	ctlblkp->ver = ccp->version;
	ctlblkp->link_id = linkid;
	ctlblkp->correlator = 0;
	ctlblkp->rdsize = C7000_DATAL;
	ctlblkp->wrtsize = C7000_DATAL;
	memcpy(ctlblkp->hostname, ccp->lappl, NAMLEN);
	memcpy(ctlblkp->unitname, ccp->uappl, NAMLEN);

	/*
		Build the channel program.
	*/

	c7000_bld_wrtctl_chpgm(cup);

	/*
		Do the IO and wait for write to complete.
	*/

	if ((rc = c7000_doio(cup)) != 0) {
		CPrintk(0, "c7000: c7000_send_disc: failed with rc = %d for unit 0x%x\n", rc, cup->devno);
		return(-1);
	}

	return(0);
}

/*
	Resolve the race condition based on the link identifier value.
	The adapter microcode assigns the identifiers.  A higher value implies
	that the race was lost. A side effect of this function is that
	ccp->linkid is set to the link identifier to be used for this
	connection (provided that 0 is returned).
*/

static int
c7000_resolve_race(struct c7000_unit *cup, int local_linkid, int remote_linkid)
{
	struct c7000_controller 	*ccp = cup->cntlp;

	CPrintk(1, "c7000: c7000_resolve_race: for unit 0x%x, local linkid %d, remote linkid %d\n", cup->devno, local_linkid, remote_linkid);

	/*
		This local link identifier should not be zero..
	*/

	if (local_linkid == 0) {
		CPrintk(0, "c7000: c7000_resolve_race: error for unit 0x%x, local linkid is null\n", cup->devno);
		return(-1);
	}

	/*
		This indicates that there is no race.  Just use our
		local link identifier.
	*/

	if (remote_linkid == 0) {
		ccp->linkid = local_linkid;
		return(0);
	}

	/*
		Send a connection confirm message if we lost the race to
		the winning link identifier.

		Either way, save the winning link identifier.
	*/

	if (local_linkid > remote_linkid) {

		if (c7000_conn_confrm(&ccp->cunits[C7000_WR], cup->control_blk.correlator, remote_linkid) != 0) {
			CPrintk(0, "c7000: c7000_resolve_race: failed for unit 0x%x\n", cup->devno);
			return(-1);
		}

		ccp->linkid = remote_linkid;
	} else {
		ccp->linkid = local_linkid;
	}

	return(0);
}

/*
	Get connected by processing the connection request/response/confirm
	control messages.  A connection request has already been sent by
	calling function c7000_send_conn.
*/

static int
c7000_get_conn(struct c7000_unit *cup)
{
	struct c7000_controller 	*ccp = cup->cntlp;
	int				rc;
	int				cont = 1;
	int				remote_linkid = 0;
	int				local_linkid = 0;

	CPrintk(1, "c7000: c7000_get_conn: read the connected message for unit 0x%x\n", cup->devno);
	ccp->linkid = 0;

	while (cont == 1) {

		/*
			Build the read channel program.
		*/

		c7000_bld_readctl_chpgm(cup);

		/*
			Start the channel program to read a control message.
		*/

		if ((rc = c7000_doio(cup)) != 0) {
			CPrintk(0, "c7000: c7000_get_conn: failed with rc = %d for unit 0x%x\n", rc, cup->devno);
			return(-1);
		}

		/*
			Process the control message that was received based
			on the command code.
		*/

		CPrintk(1, "c7000: c7000_get_conn: received command %d for unit 0x%x\n", cup->control_blk.cmd, cup->devno);

		switch(cup->control_blk.cmd) {

			/*
				Save the remote link_id in the message for
				a check in c7000_resolve_race.
			*/

			case C7000_CONN_REQ:
				remote_linkid = cup->control_blk.link_id;
				break;

			/*
				A connection response received.  Resolve
				the network race condition (if any) by
				comparing the link identifier values.
			*/

			case C7000_CONN_RESP:

				if (cup->control_blk.ret_code != 0) {
					CPrintk(0, "c7000: c7000_get_conn: failed for unit 0x%x , connection response return code %d\n",
						cup->devno, cup->control_blk.ret_code);
					return(-1);
				}

				local_linkid = cup->control_blk.link_id;

				if (c7000_resolve_race(cup, local_linkid, remote_linkid) != 0)
					return(-1);

				break;

			/*
				Got a confirmation to our connection request.
				Disconnect the remote link identifier (if any).
				Break out of the loop.
			*/

			case C7000_CONN_CONFRM:

				if (remote_linkid != 0) {

					if (c7000_send_disc(&ccp->cunits[C7000_WR], remote_linkid) != 0) {
						CPrintk(0, "c7000: c7000_get_conn: send disconnect failed for unit 0x%x\n", cup->devno);
						return(-1);
					}

				}

				cont = 0;
				break;

			/*
				Got a disconnect to our connection request.
				Break out of the loop.
			*/

			case C7000_DISCONN:
				cont = 0;
				break;

			/*
				Anything else must be an error.
				Return with an error immediately.
			*/

			default:
				CPrintk(0, "c7000: c7000_get_conn: failed for unit 0x%x unexpected command %d\n",
					cup->devno, cup->control_blk.cmd);
				return(-1);
		}

	}

	/*
		Be sure that we now have a link identifier.
	*/

	if (ccp->linkid == 0)
		return(-1);

	return(0);
}

/*
	Get statistics method.
*/

struct net_device_stats *
c7000_stats(STRUCT_NET_DEVICE *dev)
{
	struct	c7000_controller	*ccp = (struct c7000_controller *)dev->priv;

	return(&ccp->stats);
}

/*
	Open method.
*/

static int
c7000_open(STRUCT_NET_DEVICE *dev)
{
	int				i;
	struct	c7000_controller	*ccp = (struct c7000_controller *)dev->priv;
	struct	c7000_unit		*cup;
	int				rc;
	__u32				parm;
	__u8				flags = 0x00;

	c7000_set_busy(dev);

	/*
		Allocate space for the unit buffers.
	*/

	if (c7000_alloc_buffers(dev) == -1) {
		CPrintk(0, "c7000: c7000_open: can not allocate buffer space for base unit 0x%lx\n", dev->base_addr);
		c7000_free_buffers(dev);  /* free partially allocated buffers */
		c7000_clear_busy(dev);
		return(-ENOMEM);
	}
	
	/*
		Perform the initialization for all units.
	*/

	for (i = 0; i < NUNITS; i++) {
		cup = &ccp->cunits[i];

		/*
			Initialize task queue structure used for the bottom
			half routine.
		*/

#ifndef NEWSTUFF
		cup->tq.next = NULL;
#endif
		cup->tq.sync = 0;
		cup->tq.routine = (void *)(void *)c7000_irq_bh;
		cup->tq.data = cup;
		cup->state = C7000_HALT;
#ifdef NEWSTUFF
		init_waitqueue_head(&cup->wait);
#endif
		CPrintk(1, "c7000: c7000_open: issuing halt to unit 0x%x\n", cup->devno);

		/*
			Issue a halt I/O to the unit
		*/

		if ((rc = c7000_haltio(cup)) != 0) {
			CPrintk(0, "c7000: c7000_open: halt_IO failed with rc = %d for unit 0x%x\n", rc, cup->devno);
			continue;
		}

		cup->IO_active = 0;
		cup->flag_a = 0;
		cup->sigsmod = 0x00;

		CPrintk(1, "c7000: c7000_open: halt complete for unit 0x%x\n", cup->devno);
	}

	/*
		On each subchannel send a sense id.
	*/

	for (i = 0; i < NUNITS; i++) {
		cup = &ccp->cunits[i];

		/*
			Build SENSE ID channel program.
		*/

		c7000_bld_senseid_chpgm(cup);
	
		/*
			Issue the start I/O for SENSE ID channel program.
		*/

		CPrintk(1, "c7000: c7000_open: issuing SENSEID to unit 0x%x\n", cup->devno);

		if ((rc = c7000_doio(cup)) != 0) {
			CPrintk(0, "c7000: c7000_open: SENSEID failed with rc = %d for unit 0x%x\n", rc, cup->devno);
			c7000_clear_busy(dev);
			return(-EIO);
		}
	
		CPrintk(1, "c7000: c7000_open: SENSEID complete for unit 0x%x\n", cup->devno);
	}

	/*
		Send the system validation control message.
	*/

	cup = &ccp->cunits[C7000_WR];

	if (c7000_send_sysval(cup) != 0) {
		CPrintk(0, "c7000: c7000_open: can not send sysval for unit 0x%x\n", cup->devno);
		c7000_clear_busy(dev);
		return(-EIO);
	}

	CPrintk(1, "c7000: c7000_open: successfully sent sysval for unit 0x%x\n", cup->devno);
	/*
		Get the system validation response message.
	*/

	cup = &ccp->cunits[C7000_RD];

	if (c7000_get_sysval_resp(cup) != 0) {
		CPrintk(0, "c7000: c7000_open: can not read sysval response for unit 0x%x\n", cup->devno);
		c7000_clear_busy(dev);
		return(-EIO);
	}

	CPrintk(1, "c7000: c7000_open: successfully received sysval reply for unit 0x%x\n", cup->devno);
	ccp->cunits[C7000_RD].state = ccp->cunits[C7000_WR].state = C7000_CONNECT;

	cup = &ccp->cunits[C7000_WR];

	/*
		Send a connection request.
	*/

	if (c7000_send_conn(cup) != 0) {
		CPrintk(0, "c7000: c7000_open: connection failed for unit 0x%x\n", cup->devno);
		c7000_clear_busy(dev);
		return(-EIO);
	}

	cup = &ccp->cunits[C7000_RD];

	/*
		Get the response to our connection request Note that a
		network race may occur.  This is handled in c7000_get_conn.
	*/

	if (c7000_get_conn(cup) != 0) {
		CPrintk(0, "c7000: c7000_open: unit 0x%x has connected\n", cup->devno);
		c7000_clear_busy(dev);
		return(-EIO);
	}

	CPrintk(1, "c7000: c7000_open: successfully received connection request for unit 0x%x\n", cup->devno);
	ccp->cunits[C7000_RD].state = ccp->cunits[C7000_WR].state = C7000_READY;

	/*
		Clear the interface statistics.
	*/

	memset(&ccp->stats, '\0', sizeof(struct net_device_stats));

	if ((rc = c7000_bld_read_chain(cup)) != 0) {
		c7000_clear_busy(dev);
		return(rc);
	}

	/*
		Start the C7000_READ channel program but do not wait for it's
		completion.
	*/

	cup->state = C7000_READ;
	parm = (__u32) cup;
	set_bit(0, (void *)&cup->IO_active);

	if ((rc = do_IO(cup->irq, &cup->proc_head->ccws[0], parm, 0xff, flags)) != 0) {
		CPrintk(0, "c7000: c7000_open: READ failed with return code %d for unit 0x%x\n", rc, cup->devno);
		c7000_error(cup->cntlp);
		clear_bit(0, (void *)&cup->IO_active);
		c7000_clear_busy(dev);
		return(-EIO);
	}

#ifdef NEWSTUFF
	netif_start_queue(dev);
#else
	dev->start = 1;
#endif
	CPrintk(0, "c7000: c7000_open: base unit 0x%lx is opened\n", dev->base_addr);
	c7000_clear_busy(dev);
	MOD_INC_USE_COUNT;	/* increment module usage count */
	return(0);
}

/*
	Stop method.
*/

static int
c7000_stop(STRUCT_NET_DEVICE *dev)
{
	int				i;
	struct	c7000_controller	*ccp = (struct c7000_controller *)dev->priv;
	struct	c7000_unit		*cup;
	int				rc;

#ifdef NEWSTUFF
/* nothing? */
#else
	dev->start = 0;
#endif
	c7000_set_busy(dev);

	/*
		Send a disconnect message.
	*/

	ccp->cunits[C7000_RD].state = ccp->cunits[C7000_WR].state = C7000_DISC;
	cup = &ccp->cunits[C7000_WR];

	if (c7000_send_disc(cup, ccp->linkid) != 0) {
		CPrintk(0, "c7000: c7000_stop: send of disconnect message failed for unit 0x%x\n", cup->devno);
	}

	CPrintk(1, "c7000: c7000_stop: successfully sent disconnect message to unit 0x%x\n", cup->devno);

	/*
		Issue a halt I/O to all units.
	*/

	for (i = 0; i < NUNITS; i++) {
		cup = &ccp->cunits[i];
		cup->state = C7000_STOP;
		CPrintk(1, "c7000: c7000_stop: issuing halt to unit 0x%x\n", cup->devno);

		if ((rc = c7000_haltio(cup)) != 0) {
			CPrintk(0, "c7000: c7000_stop: halt_IO failed with rc = %d for unit 0x%x\n", rc, cup->devno);
			continue;
		}

		CPrintk(1, "c7000: c7000_stop: halt complete for unit 0x%x\n", cup->devno);
	}

	c7000_free_buffers(dev);
	CPrintk(0, "c7000: c7000_stop: base unit 0x%lx is stopped\n", dev->base_addr);
	MOD_DEC_USE_COUNT;	/* Decrement module usage count */
	return(0);
}

/*
	Configure the interface.
*/

static int
c7000_config(STRUCT_NET_DEVICE *dev, struct ifmap *map)
{
	CPrintk(1, "c7000: c7000_config: entered for base unit 0x%lx\n", dev->base_addr);
	return(0);
}

/*
	Transmit a packet.
*/

static int
c7000_xmit(struct sk_buff *skb, STRUCT_NET_DEVICE *dev)
{
	struct	c7000_controller	*ccp = (struct c7000_controller *)dev->priv;
	struct	c7000_unit		*cup;
	__u32				saveflags;
	__u32				parm;
	__u8				flags = 0x00;
	struct	c7000_buffer		*buf, *pbuf;
	int				rc;

	CPrintk(1, "c7000: c7000_xmit: entered for base unit 0x%lx\n", dev->base_addr);

	/*
		When the skb pointer is NULL return.
	*/

	if (skb == NULL) {
		CPrintk(0, "c7000: c7000_xmit: skb pointer is null for base unit 0x%lx\n", dev->base_addr);
		ccp->stats.tx_dropped++;
		return(-EIO);
	}

	cup = &ccp->cunits[C7000_WR];

	/*
		Lock the irq.
	*/

	s390irq_spin_lock_irqsave(cup->irq, saveflags);
	
	/*
		When the device transmission busy flag is on , no data
		can be sent.  Unlock the irq and return EBUSY.
	*/

	if (c7000_check_busy(dev)) {
		CPrintk(1, "c7000: c7000_xmit: c7000_check_busy returns true for base unit 0x%lx\n", dev->base_addr);
		s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
		return(-EBUSY);
	}

	/*
		Set the device transmission busy flag on atomically.
	*/

	if (c7000_ts_busy(TB_TX, dev)) {
		CPrintk(1, "c7000: c7000_xmit: c7000_ts_busy returns true for base unit 0x%lx\n", dev->base_addr);
		s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
		return(-EBUSY);
	}

	CPrintk(1, "c7000: c7000_xmit: set TB_TX for unit 0x%x\n", cup->devno);

	/*
		Obtain a free buffer.  If none are free then mark tbusy
		with TB_NOBUFFER and return EBUSY.
	*/

	if ((buf = c7000_get_buffer(cup)) == NULL) {
		CPrintk(1, "c7000: c7000_xmit: setting TB_NOBUFFER for base unit 0x%lx\n", dev->base_addr);
		c7000_setbit_busy(TB_NOBUFFER, dev);
		c7000_clearbit_busy(TB_TX, dev);
		s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
		return(-EBUSY);
	}

	CPrintk(1, "c7000: c7000_xmit: Got buffer for unit 0x%x\n", cup->devno);

	/*
		Save the length of the skb data and then copy it to the
		buffer.  Queue the buffer on the processing list.
	*/

	buf->len = skb->len;
	memcpy(buf->data, skb->data, skb->len);
	memset(buf->data + C7000_DATAL + C7000_READHDRL, '\0', C7000_READFFL);
	pbuf = cup->proc_tail;
	c7000_queue_buffer(cup, buf);

	/*
		Chain the buffer to the running channel program.
	*/

	if (test_bit(0, (void *)&cup->IO_active) && pbuf != NULL) {
		c7000_bld_wrt_chpgm(cup, buf);
		pbuf->ccws[2].cda = (__u32)virt_to_phys(&buf->ccws[0]);
	}

	/*
		Free the socket buffer.
	*/

	dev_kfree_skb(skb);

	/*
		If the unit is not currently doing IO, build a channel
		program and start the IO for the buffers on the processing
		chain.
	*/

	if (test_and_set_bit(0, (void *)&cup->IO_active) == 0) {
		CPrintk(1, "c7000: c7000_xmit: start IO for unit 0x%x\n", cup->devno);
		c7000_bld_wrt_chain(cup);
		parm = (__u32) cup;
		cup->state = C7000_WRITE;

		if ((rc = do_IO(cup->irq, &cup->proc_head->ccws[0], parm, 0xff, flags)) != 0) {
			CPrintk(0, "c7000: c7000_xmit: do_IO failed with return code %d for unit 0x%x\n", rc, cup->devno);
			c7000_error(cup->cntlp);
			c7000_clearbit_busy(TB_TX, dev);
			s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
			return(-EIO);
		}

		dev->trans_start = jiffies;
		CPrintk(1, "c7000: c7000_xmit: do_IO succeeds for unit 0x%x\n", cup->devno);
	}
		
	/*
		If the free chain is now NULL, set the TB_NOBUFFER flag.
	*/

	if (cup->free == NULL) {
		CPrintk(1, "c7000: c7000_xmit: setting TB_NOBUFFER for base unit 0x%lx\n", dev->base_addr);
		c7000_setbit_busy(TB_NOBUFFER, dev);
	}

	c7000_clearbit_busy(TB_TX, dev);
	s390irq_spin_unlock_irqrestore(cup->irq, saveflags);
	CPrintk(1, "c7000: c7000_xmit: exits for unit 0x%x\n", cup->devno);
	return(0);
}

/*
	Handle an ioctl from a user process.
*/

static int
c7000_ioctl(STRUCT_NET_DEVICE *dev, struct ifreq *ifr, int cmd)
{
	CPrintk(1, "c7000: c7000_ioctl: entered for base unit 0x%lx with cmd %d\n", dev->base_addr, cmd);
	return(0);
}

/*
	Analyze the interrupt status and return a value
	that identifies the type.
*/

static enum c7000_rupt
c7000_check_csw(devstat_t *devstat)
{

	/*
		Check for channel detected conditions (except PCI).
	*/

	if ((devstat->cstat & ~SCHN_STAT_PCI) != 0) {
		CPrintk(0, "c7000: c7000_check_csw: channel status 0x%x for unit 0x%x\n", devstat->cstat, devstat->devno);
		return(C7000_CHANERR);
	}

	/*
		Fast path the normal cases.
	*/

	if (devstat->dstat == (DEV_STAT_CHN_END | DEV_STAT_DEV_END))
		return(C7000_NORMAL);

	if (devstat->cstat == SCHN_STAT_PCI)
		return(C7000_NORMAL);

	/*
		Check for exceptions.
	*/

	if (devstat->dstat & DEV_STAT_UNIT_CHECK) {
		CPrintk(0, "c7000: c7000_check_csw: unit check for unit 0x%x, sense byte0 0x%2.2x\n", devstat->devno, devstat->ii.sense.data[0]);

		if (devstat->ii.sense.data[0] == C7000_BOX_RESET)
			return(C7000_UCK_RESET);
		else
			return(C7000_UCK);

	} else if (devstat->dstat & DEV_STAT_UNIT_EXCEP) {
		CPrintk(0, "c7000: c7000_check_csw: unit exception for unit 0x%x\n", devstat->devno);
		return(C7000_UE);

	} else if (devstat->dstat & DEV_STAT_ATTENTION) {
		CPrintk(0, "c7000: c7000_check_csw: attention for unit 0x%x\n", devstat->devno);
		return(C7000_ATTN);

	} else if (devstat->dstat & DEV_STAT_BUSY) {
		CPrintk(0, "c7000: c7000_check_csw: busy for unit 0x%x\n", devstat->devno);
		return(C7000_BUSY);

	} else {
		CPrintk(0, "c7000: c7000_check_csw: channel status 0x%2.2x , device status 0x%2.2x, devstat flags 0x%8.8x for unit 0x%x\n",
			devstat->cstat, devstat->dstat, devstat->flag, devstat->devno);
		return(C7000_OTHER);
	}

	/* NOT REACHED */

}

/*
	Retry the last CCW chain to the unit.
*/

static void
c7000_retry_io(struct c7000_unit *cup)
{
	int	rc;
	__u32	parm;
	__u8	flags = 0x00;
	ccw1_t	*ccwp;

	if (++cup->retries > C7000_MAX_RETRIES) {
		c7000_error(cup->cntlp);
		CPrintk(0, "c7000: c7000_retry_io: retry IO for unit 0x%x exceeds maximum retry count\n", cup->devno);
		return;
	}

	set_bit(0, (void *)&cup->IO_active);
	parm = (__u32)cup;

	if (cup->state == C7000_READ || cup->state == C7000_WRITE)
		ccwp = &cup->proc_head->ccws[0];
	else
		ccwp = &cup->ccws[0];

	if ((rc = do_IO(cup->irq, ccwp, parm, 0xff, flags)) != 0) {
		CPrintk(0, "c7000: c7000_retry_io: can not retry IO for unit 0x%x, return code %d\n", cup->devno, rc);
		clear_bit(0, (void *)&cup->IO_active);
		c7000_error(cup->cntlp);
	}

	CPrintk(1, "c7000: c7000_retry_io: retry IO for unit 0x%x, retry count %d\n", cup->devno, cup->retries);
	return;
}

/*
	Process a read interrupt by scanning the list of buffers
	for ones that have completed and queue them for the bottom
	half to process.
*/

static void
c7000_proc_rintr(struct c7000_unit *cup)
{
	struct	c7000_buffer	*buf;
	struct	c7000_rd_header	*head;
	int			num_read = 0;

	while (cup->proc_head != NULL) {
		head = (struct c7000_rd_header *)(cup->proc_head->data + C7000_DATAL);

		/*
			The flag byte in the read header will be set to
			FLAG_FF when the buffer has been read.
		*/

		if (head->flag != FLAG_FF)
			break;
		
		/*
			Dequeue the buffer from the proc chain
			and enqueue it on the bh chain for
			the bh routine to process.
		*/

		buf = c7000_dequeue_buffer(cup);
		c7000_queue_bh_buffer(cup, buf);
		num_read++;
	}

	CPrintk(1, "c7000: c7000_proc_rintr: %d buffers read for unit 0x%x\n", num_read, cup->devno);
	return;
}

/*
	Process all completed buffers on the proc chain.
	A buffer is completed if it's READFF flag is FLAG_FF.
*/

static int
c7000_proc_wintr(struct c7000_unit *cup)
{
	struct	c7000_controller	*ccp = cup->cntlp;
	struct	c7000_buffer		*buf;
	int				num_write = 0;

	if (cup->proc_head == NULL) {
		CPrintk(0, "c7000: c7000_proc_wintr: unexpected NULL processing chain pointer for unit 0x%x\n", cup->devno);
		return(num_write);
	}

	while (cup->proc_head != NULL) {

		/*
			Check if the buffer has completed.
		*/

		if (*(cup->proc_head->data + C7000_DATAL + C7000_READHDRL) != FLAG_FF)
			break;

		/*
			Remove buffer from top of processing chain.
			Place it on free list.
		*/

		buf = c7000_dequeue_buffer(cup);
		c7000_release_buffer(cup, buf);
		num_write++;

		/*
			Update transmitted packets statistic.
		*/

		ccp->stats.tx_packets++;
	}

	CPrintk(1, "c7000: c7000_proc_wintr: %d buffers written for unit 0x%x\n", num_write, cup->devno);
	return(num_write);
}

/*
	Interrupt handler.
*/

static void
c7000_intr(int irq, void *initparm, struct pt_regs *regs)
{
	devstat_t			*devstat = ((devstat_t *) initparm);
	struct	c7000_unit		*cup = NULL;
	struct	c7000_controller	*ccp = NULL;
	STRUCT_NET_DEVICE		*dev = NULL;
	__u32				parm;
	__u8				flags = 0x00;
	int				rc;

	/*
		Discard unsolicited interrupts
	*/

	if (devstat->intparm == 0) {
		CPrintk(0, "c7000: c7000_intr: unsolicited interrupt for device 0x%x, cstat = 0x%2.2x, dstat = 0x%2.2x, flag = 0x%8.8x\n",
			devstat->devno, devstat->cstat, devstat->dstat, devstat->flag);
		return;
	}

	/*
		Obtain the c7000_unit structure pointer.
	*/

	cup = (struct c7000_unit *)(devstat->intparm);

	/*
		Obtain the c7000_controller structure and device structure
		pointers.
	*/

	if (cup == NULL) {
		CPrintk(0, "c7000: c7000_intr: c7000_unit pointer is NULL in devstat\n");
		return;
	}

	ccp = cup->cntlp;

	if (ccp == NULL) {
		CPrintk(0, "c7000: c7000_intr: c7000_cntlp pointer is NULL in c7000_unit structure 0x%x for unit 0x%x\n", (int)cup, cup->devno);
		return;
	}

	dev = ccp->dev;

	if (dev == NULL) {
		CPrintk(0, "c7000: c7000_intr: device pointer is NULL in c7000_controller structure 0x%x for unit 0x%x\n", (int)ccp, cup->devno);
		return;
	}

	/*
		Finite state machine (fsm) handling.
	*/

	CPrintk(1, "c7000: c7000_intr: entered with state %d flag 0x%8.8x for unit 0x%x\n", cup->state, devstat->flag, cup->devno);

	switch(cup->state) {

		/*
			Not expected to be here when in INIT state.
		*/

		case C7000_INIT:

			break;

		/*
			Enter state C7000_SID and wakeup the sleeping
			process in c7000_open.
		*/

		case C7000_HALT:

			if ((devstat->flag & DEVSTAT_FINAL_STATUS) == 0)
				break;

			cup->state = C7000_SID;
			wake_up(&cup->wait);
			break;
		
		/*
			Enter state C7000_SYSVAL and wakeup the sleeping
			process in c7000_open.
		*/
			
		case C7000_SID:

			if ((devstat->flag & DEVSTAT_FINAL_STATUS) == 0)
				break;

			if (c7000_check_csw(devstat) != 0) {
				c7000_retry_io(cup);

				if (cup->state == C7000_ERROR)
					wake_up(&cup->wait);

				break;
			}

			cup->retries = 0;
			cup->state = C7000_SYSVAL;
			wake_up(&cup->wait);
			break;

		/*
			Wakeup the sleeping process in c7000_open.
		*/

		case C7000_SYSVAL:

			if ((devstat->flag & DEVSTAT_FINAL_STATUS) == 0)
				break;

			if (c7000_check_csw(devstat) != 0) {
				c7000_retry_io(cup);

				if (cup->state == C7000_ERROR)
					wake_up(&cup->wait);

				break;
			}

			cup->retries = 0;
			wake_up(&cup->wait);
			break;

		/*
			Wakeup the sleeping process in c7000_open.
		*/

		case C7000_CONNECT:

			if ((devstat->flag & DEVSTAT_FINAL_STATUS) == 0)
				break;

			if (c7000_check_csw(devstat) != 0) {
				c7000_retry_io(cup);

				if (cup->state == C7000_ERROR)
					wake_up(&cup->wait);

				break;
			}

			cup->retries = 0;
			wake_up(&cup->wait);
			break;

		/*
			Not expected to be entered here.
		*/

		case C7000_READY:
			break;

		/*
			Process the data that was just read.
		*/

		case C7000_READ:

			if ((devstat->flag & (DEVSTAT_PCI | DEVSTAT_FINAL_STATUS)) == 0)
				break;

			CPrintk(1, "c7000: c7000_intr: process read interrupt for unit 0x%x , devstat flag = 0x%8.8x\n", cup->devno, devstat->flag);

			/*
				Check for serious errors.
			*/

			if (c7000_check_csw(devstat) != 0) {
				ccp->stats.rx_errors++;
				c7000_error(cup->cntlp);
				break;
			}

			/*
				Build the bottom half buffer list.
			*/

			c7000_proc_rintr(cup);

			/*
				When final status is received clear
				the IO active bit.
			*/

			if (devstat->flag & DEVSTAT_FINAL_STATUS) {
				clear_bit(0, (void *)&cup->IO_active);
			}

			/*
				If there are free buffers redrive the IO.
			*/

			if ((devstat->flag & DEVSTAT_FINAL_STATUS) &&
			    (cup->free != NULL)) {
				c7000_bld_read_chain(cup);
				parm = (__u32)cup;
				set_bit(0, (void *)&cup->IO_active);

				if ((rc = do_IO(cup->irq, &cup->proc_head->ccws[0], parm, 0xff, flags)) != 0) {
					clear_bit(0, (void *)&cup->IO_active);
					CPrintk(0, "c7000: c7000_intr: do_IO failed with return code %d for unit 0x%x\n", rc, cup->devno);
					c7000_error(cup->cntlp);
					break;
				}

				CPrintk(1, "c7000: c7000_intr: started read io for unit 0x%x\n", cup->devno);
			}

			/*
				Initiate bottom half routine to process
				data that was read.
			*/

			if (test_and_set_bit(C7000_BH_ACTIVE, (void *)&cup->flag_a) == 0) {
				queue_task(&cup->tq, &tq_immediate);
				mark_bh(IMMEDIATE_BH);
			}

			break;

		/*
			Free the transmitted buffers and restart the channel
			process (if necessary).
		*/

		case C7000_WRITE:

			if ((devstat->flag & DEVSTAT_FINAL_STATUS) == 0)
				break;

			if (c7000_check_csw(devstat) != 0) {
				ccp->stats.tx_errors++;
				c7000_error(cup->cntlp);
				break;
			}

			/*
				If at least one buffer was freed, clear
				the NOBUFFER indication.
			*/

			if (c7000_proc_wintr(cup) != 0) {
				c7000_clearbit_busy(TB_NOBUFFER, dev);
			}

			/*
				Restart the channel program if there are more
				buffers on the processing chain.
			*/

			if (cup->proc_head != NULL) {
				c7000_bld_wrt_chain(cup);
				parm = (__u32)cup;
				set_bit(0, (void *)&cup->IO_active);

				if ((rc = do_IO(cup->irq, &cup->proc_head->ccws[0], parm, 0xff, flags)) != 0) {
					CPrintk(0, "c7000: c7000_intr: do_IO failed with return code %d for unit 0x%x\n", rc, cup->devno);
					clear_bit(0, (void *)&cup->IO_active);
					c7000_error(cup->cntlp);
					break;
				}

				dev->trans_start = jiffies;
			} else {
				clear_bit(0, (void *)&cup->IO_active);
				cup->state = C7000_READY;
			}

			break;

		/*
			Disconnect message completed.  Wakeup the
			sleeping process in c7000_stop.
		*/

		case C7000_DISC:
			if ((devstat->flag & DEVSTAT_FINAL_STATUS) == 0)
				break;

			if (c7000_check_csw(devstat) != 0) {
				c7000_retry_io(cup);

				if (cup->state == C7000_ERROR)
					wake_up(&cup->wait);

				break;
			}

			cup->retries = 0;
			wake_up(&cup->wait);
			break;

		/*
			Subchannel is now halted.  Wakeup the sleeping
			process in c7000_stop.  Set the state to C7000_STOPPED.
		*/

		case C7000_STOP:

			cup->state = C7000_STOPPED;
			wake_up(&cup->wait);
			break;

		/*
			When in error state, stay there until the
			interface is recycled.
		*/

		case C7000_ERROR:

			break;

		/*
			Should not reach here
		*/

		default:
			CPrintk(0, "c7000: c7000_intr: entered default case for unit 0x%x, state %d\n", cup->devno, cup->state);
			break;

	}

	CPrintk(1, "c7000: c7000_intr: exited with state %d for unit 0x%x\n", cup->state, cup->devno);
	return;
}

/*
	Fill in system validation name padding it with blanks.
*/

static void
c7000_fill_name(char *dst, char *src)
{
	char	*tmp = dst;
	int	i;

	for (i = 0; i < NAMLEN; i++, tmp++) 
		*tmp = ' ';

	for (i = 0; i < NAMLEN && *src != '\0'; i++)
		*dst++ = *src++;

	return;
}

/*
	Initialization routine called when the device is registered.
*/

static int
c7000_init(STRUCT_NET_DEVICE *dev)
{
	struct	c7000_controller	*ccp;
	int				i;
	int				unitaddr;
	int				irq;

	/*
		Find the position of base_addr in the bases array.
	*/

	for (i = 0; i < MAX_C7000; i++) 
		if (bases[i] == dev->base_addr)
			break;

	if (i == MAX_C7000)
		return(-ENODEV);

	/*
		Make sure it is a C7000 type of device.
	*/

	if (c7000_check_devices(dev->base_addr) != 0) {
		CPrintk(0, "c7000: c7000_init: base unit 0x%lx is not the right type\n", dev->base_addr);
		return(-ENODEV);
	}

	/*
		Initialize the device structure functions.
		Note that ARP is not done on the CLAW interface.
		There is no ethernet header.
	*/

	dev->mtu = C7000_DATAL;
	dev->hard_header_len = 0;
        dev->addr_len = 0;
        dev->type = ARPHRD_SLIP;
        dev->tx_queue_len = C7000_TXQUEUE_LEN;
	dev->flags = IFF_NOARP;   
	dev->open = c7000_open;
	dev->stop = c7000_stop;
	dev->set_config = c7000_config;
	dev->hard_start_xmit = c7000_xmit;
	dev->do_ioctl = c7000_ioctl;
	dev->get_stats = c7000_stats;

	/*
		Allocate space for a private data structure.
	*/

	if ((ccp = dev->priv = kmalloc(sizeof(struct c7000_controller), GFP_KERNEL)) == NULL) {
		CPrintk(0, "c7000: c7000_init: base unit 0x%lx can not be initialized\n", dev->base_addr);
		return(-ENOMEM);
	}

	CPrintk(1, "c7000: c7000_init: allocated a c7000_controller structure at address 0x%x\n", (int)ccp);
	memset(ccp, '\0', sizeof(struct c7000_controller));
	ccp->dev = dev;
	ccp->base_addr = dev->base_addr;

	/*
		Populate the system validation name with default values.
	*/

	c7000_fill_name(ccp->lappl, C7000_DFLT_LAPPL);
	c7000_fill_name(ccp->lhost, C7000_DFLT_LHOST);
	c7000_fill_name(ccp->uappl, C7000_DFLT_UAPPL);
	c7000_fill_name(ccp->uhost, C7000_DFLT_UHOST);

	/*
		When values have been supplied, replace the prior defaults.
	*/

	if (i == 0) {

		if (lappl0 != NULL)
			c7000_fill_name(ccp->lappl, lappl0);

		if (lhost0 != NULL)
			c7000_fill_name(ccp->lhost, lhost0);

		if (uappl0 != NULL)
			c7000_fill_name(ccp->uappl, uappl0);

		if (uhost0 != NULL)
			c7000_fill_name(ccp->uhost, uhost0);

	} else if (i == 1) {

		if (lappl1 != NULL)
			c7000_fill_name(ccp->lappl, lappl1);

		if (lhost1 != NULL)
			c7000_fill_name(ccp->lhost, lhost1);

		if (uappl1 != NULL)
			c7000_fill_name(ccp->uappl, uappl1);

		if (uhost1 != NULL)
			c7000_fill_name(ccp->uhost, uhost1);

	} else if (i == 2) {

		if (lappl2 != NULL)
			c7000_fill_name(ccp->lappl, lappl2);

		if (lhost2 != NULL)
			c7000_fill_name(ccp->lhost, lhost2);

		if (uappl2 != NULL)
			c7000_fill_name(ccp->uappl, uappl2);

		if (uhost2 != NULL)
			c7000_fill_name(ccp->uhost, uhost2);

	} else {

		if (lappl3 != NULL)
			c7000_fill_name(ccp->lappl, lappl3);

		if (lhost3 != NULL)
			c7000_fill_name(ccp->lhost, lhost3);

		if (uappl3 != NULL)
			c7000_fill_name(ccp->uappl, uappl3);

		if (uhost3 != NULL)
			c7000_fill_name(ccp->uhost, uhost3);

	}

	CPrintk(1, "c7000: c7000_init: lappl = %8.8s lhost = %8.8s uappl = %8.8s uhost = %8.8s for base unit 0x%x\n", ccp->lappl, ccp->lhost, ccp->uappl, ccp->uhost, ccp->base_addr);
	ccp->version = 2;
	ccp->linkid = 0;

	/*
		Initialize the fields in the embedded cunits
		array.  This type of controller occupies a range
		of three contiguous device numbers.
	*/

	for (i = 0; i < NUNITS; i++) {
		unitaddr = dev->base_addr + i;

		/*
			Get the subchannel number.
		*/

		if ((irq = ccp->cunits[i].irq = get_irq_by_devno(unitaddr)) == -1) {
			CPrintk(0, "c7000: c7000_init: can not get subchannel for unit 0x%x\n", unitaddr);
			return(-ENODEV);
		}

		/*
			Get control of the subchannel.
		*/

		if (request_irq(irq, c7000_intr, SA_INTERRUPT, dev->name, &ccp->cunits[i].devstat) != 0) {
			CPrintk(0, "c7000: c7000_init: can not get control of subchannel 0x%x for unit 0x%x\n", irq, unitaddr);
			return(-EBUSY);
		}

		CPrintk(1, "c7000: c7000_init: obtained control of subchannel 0x%x for unit 0x%x\n", irq, unitaddr);
		ccp->cunits[i].devno = unitaddr;
		ccp->cunits[i].IO_active = 0;
		ccp->cunits[i].state = C7000_INIT;
		ccp->cunits[i].cntlp = ccp;
		CPrintk(1, "c7000: c7000_init: initialized unit 0x%x on subchannel 0x%x\n", unitaddr, irq);
	}
		
	return(0);
}

/*
	Probe for the Cisco 7000 unit base addresses.
*/

static void
c7000_probe(void)
{
	s390_dev_info_t	d;
	int		i;
	int		j;
	int		idx;

	/*
		Probe for up to MAX_C7000 devices.
		Get the first irq into variable idx.
	*/
	
	idx = get_irq_first();

	for (j = 0; j < MAX_C7000; j++) {

		if (idx < 0)
			break;

		/*
			Continue scanning the irq's.  Variable idx
			maintains the location from the prior scan.
		*/

		for (i = idx; i >= 0; i = get_irq_next(i)) {

			/*
				Ignore invalid irq's.
			*/

			if (get_dev_info_by_irq(i, &d) < 0)
				continue;

			/*
				A Cisco 7000 is defined as a 3088 model
				type 0x61.
			*/

			if (d.sid_data.cu_type == C7000_CU_TYPE &&
			    d.sid_data.cu_model == C7000_CU_MODEL) {
				CPrintk(0, "c7000_probe: unit probe found 0x%x\n", d.devno);
				bases[j] = d.devno;

				/*
					Skip the write irq and setup idx
					to probe for the next box.
				*/

				idx = get_irq_next(i + 1);
				break;
			}

		}

	}

	return;
}

/*
	Module loading.  Register each C7000 interface found via probing
	or insmod command parameters.
*/

int
init_module(void)
{
	int		result;
	int		i;

	for (i = 0 ; i < MAX_C7000; i++)
		bases[i] = -1;

	/*
		Perform automatic detection provided it has not been disabled
		by the noauto parameter. 
	*/

	if (noauto == 0)
		c7000_probe();

	/*
		Populate bases array from the module basex parameters replacing
		what probing found above.
	*/

	if (base0 != -1)
		bases[0] = base0;

	if (base1 != -1)
		bases[1] = base1;

	if (base2 != -1)
		bases[2] = base2;

	if (base3 != -1)
		bases[3] = base3;

	for (i = 0; i < MAX_C7000; i++) {

		if (bases[i] == -1)
			continue;
		
		/*
			Initialize the device structure.
		*/

		memset(&c7000_devices[i], '\0', sizeof(STRUCT_NET_DEVICE));
#ifdef NEWSTUFF
		strcpy(c7000_devices[i].name, ifnames[i]);
#else
		c7000_devices[i].name = &ifnames[i][0];
#endif
		c7000_devices[i].base_addr = bases[i];
		c7000_devices[i].init = c7000_init;

		/*
			Register the device.  This creates the interface
			such as ci0.
		*/

		if ((result = register_netdev(&c7000_devices[i])) != 0)  {
			CPrintk(0, "c7000: init_module: error %d registering base unit 0x%x\n",
				result, bases[i]);
			c7000_devices[i].base_addr = -1;
		} else {
			CPrintk(1, "c7000: init_module: registered base unit 0x%x on interface %s\n", bases[i], ifnames[i]);
		}

	}

	CPrintk(0, "c7000: init_module: module loaded\n");
	return(0);
}

/*
	Module unloading.  Unregister the interface and free kernel
	allocated memory.
*/

void
cleanup_module(void)
{
	int				i;
	int				j;
	struct	c7000_controller	*ccp;

	for (i = 0; i < MAX_C7000; i++) {

		if (bases[i] == -1)
			continue;

		/*
			If the device was registered, it must be unregistered
			prior to unloading the module.
		*/

		if (c7000_devices[i].base_addr != -1) {

			ccp = (struct c7000_controller *) c7000_devices[i].priv;

			if (ccp != NULL) {
				
				for (j = 0; j < NUNITS ; j++) {
					CPrintk(1, "c7000: clean_module: free subchannel 0x%x for unit 0x%x\n", ccp->cunits[j].irq, ccp->cunits[j].devno);
					free_irq(ccp->cunits[j].irq, &ccp->cunits[j].devstat);
				}

				CPrintk(1, "c7000: clean_module: free a c7000_controller structure at address 0x%x\n", (int)ccp);
				kfree(ccp);
			}

			unregister_netdev(&c7000_devices[i]);
		}

		bases[i] = -1;
	}

	CPrintk(0, "c7000: clean_module: module unloaded\n");
	return;
}
