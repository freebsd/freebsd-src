#ifndef __LINUX_UHCI_H
#define __LINUX_UHCI_H

#include <linux/list.h>
#include <linux/usb.h>

/*
 * Universal Host Controller Interface data structures and defines
 */

/* Command register */
#define USBCMD		0
#define   USBCMD_RS		0x0001	/* Run/Stop */
#define   USBCMD_HCRESET	0x0002	/* Host reset */
#define   USBCMD_GRESET		0x0004	/* Global reset */
#define   USBCMD_EGSM		0x0008	/* Global Suspend Mode */
#define   USBCMD_FGR		0x0010	/* Force Global Resume */
#define   USBCMD_SWDBG		0x0020	/* SW Debug mode */
#define   USBCMD_CF		0x0040	/* Config Flag (sw only) */
#define   USBCMD_MAXP		0x0080	/* Max Packet (0 = 32, 1 = 64) */

/* Status register */
#define USBSTS		2
#define   USBSTS_USBINT		0x0001	/* Interrupt due to IOC */
#define   USBSTS_ERROR		0x0002	/* Interrupt due to error */
#define   USBSTS_RD		0x0004	/* Resume Detect */
#define   USBSTS_HSE		0x0008	/* Host System Error - basically PCI problems */
#define   USBSTS_HCPE		0x0010	/* Host Controller Process Error - the scripts were buggy */
#define   USBSTS_HCH		0x0020	/* HC Halted */

/* Interrupt enable register */
#define USBINTR		4
#define   USBINTR_TIMEOUT	0x0001	/* Timeout/CRC error enable */
#define   USBINTR_RESUME	0x0002	/* Resume interrupt enable */
#define   USBINTR_IOC		0x0004	/* Interrupt On Complete enable */
#define   USBINTR_SP		0x0008	/* Short packet interrupt enable */

#define USBFRNUM	6
#define USBFLBASEADD	8
#define USBSOF		12

/* USB port status and control registers */
#define USBPORTSC1	16
#define USBPORTSC2	18
#define   USBPORTSC_CCS		0x0001	/* Current Connect Status ("device present") */
#define   USBPORTSC_CSC		0x0002	/* Connect Status Change */
#define   USBPORTSC_PE		0x0004	/* Port Enable */
#define   USBPORTSC_PEC		0x0008	/* Port Enable Change */
#define   USBPORTSC_LS		0x0030	/* Line Status */
#define   USBPORTSC_RD		0x0040	/* Resume Detect */
#define   USBPORTSC_LSDA	0x0100	/* Low Speed Device Attached */
#define   USBPORTSC_PR		0x0200	/* Port Reset */
#define   USBPORTSC_SUSP	0x1000	/* Suspend */

/* Legacy support register */
#define USBLEGSUP		0xc0
#define   USBLEGSUP_DEFAULT	0x2000	/* only PIRQ enable set */

#define UHCI_NULL_DATA_SIZE	0x7FF	/* for UHCI controller TD */

#define UHCI_PTR_BITS		0x000F
#define UHCI_PTR_TERM		0x0001
#define UHCI_PTR_QH		0x0002
#define UHCI_PTR_DEPTH		0x0004

#define UHCI_NUMFRAMES		1024	/* in the frame list [array] */
#define UHCI_MAX_SOF_NUMBER	2047	/* in an SOF packet */
#define CAN_SCHEDULE_FRAMES	1000	/* how far future frames can be scheduled */

struct uhci_frame_list {
	__u32 frame[UHCI_NUMFRAMES];

	void *frame_cpu[UHCI_NUMFRAMES];

	dma_addr_t dma_handle;
};

struct urb_priv;

struct uhci_qh {
	/* Hardware fields */
	__u32 link;			/* Next queue */
	__u32 element;			/* Queue element pointer */

	/* Software fields */
	dma_addr_t dma_handle;

	struct usb_device *dev;
	struct urb_priv *urbp;

	struct list_head list;		/* P: uhci->frame_list_lock */
	struct list_head remove_list;	/* P: uhci->remove_list_lock */
} __attribute__((aligned(16)));

/*
 * for TD <status>:
 */
#define TD_CTRL_SPD		(1 << 29)	/* Short Packet Detect */
#define TD_CTRL_C_ERR_MASK	(3 << 27)	/* Error Counter bits */
#define TD_CTRL_C_ERR_SHIFT	27
#define TD_CTRL_LS		(1 << 26)	/* Low Speed Device */
#define TD_CTRL_IOS		(1 << 25)	/* Isochronous Select */
#define TD_CTRL_IOC		(1 << 24)	/* Interrupt on Complete */
#define TD_CTRL_ACTIVE		(1 << 23)	/* TD Active */
#define TD_CTRL_STALLED		(1 << 22)	/* TD Stalled */
#define TD_CTRL_DBUFERR		(1 << 21)	/* Data Buffer Error */
#define TD_CTRL_BABBLE		(1 << 20)	/* Babble Detected */
#define TD_CTRL_NAK		(1 << 19)	/* NAK Received */
#define TD_CTRL_CRCTIMEO	(1 << 18)	/* CRC/Time Out Error */
#define TD_CTRL_BITSTUFF	(1 << 17)	/* Bit Stuff Error */
#define TD_CTRL_ACTLEN_MASK	0x7FF		/* actual length, encoded as n - 1 */

#define TD_CTRL_ANY_ERROR	(TD_CTRL_STALLED | TD_CTRL_DBUFERR | \
				 TD_CTRL_BABBLE | TD_CTRL_CRCTIME | TD_CTRL_BITSTUFF)

#define uhci_status_bits(ctrl_sts)	(ctrl_sts & 0xFE0000)
#define uhci_actual_length(ctrl_sts)	((ctrl_sts + 1) & TD_CTRL_ACTLEN_MASK) /* 1-based */

/*
 * for TD <info>: (a.k.a. Token)
 */
#define TD_TOKEN_TOGGLE_SHIFT	19
#define TD_TOKEN_TOGGLE		(1 << 19)
#define TD_TOKEN_PID_MASK	0xFF
#define TD_TOKEN_EXPLEN_MASK	0x7FF		/* expected length, encoded as n - 1 */

#define uhci_maxlen(token)	((token) >> 21)
#define uhci_expected_length(info) (((info >> 21) + 1) & TD_TOKEN_EXPLEN_MASK) /* 1-based */
#define uhci_toggle(token)	(((token) >> TD_TOKEN_TOGGLE_SHIFT) & 1)
#define uhci_endpoint(token)	(((token) >> 15) & 0xf)
#define uhci_devaddr(token)	(((token) >> 8) & 0x7f)
#define uhci_devep(token)	(((token) >> 8) & 0x7ff)
#define uhci_packetid(token)	((token) & TD_TOKEN_PID_MASK)
#define uhci_packetout(token)	(uhci_packetid(token) != USB_PID_IN)
#define uhci_packetin(token)	(uhci_packetid(token) == USB_PID_IN)

/*
 * The documentation says "4 words for hardware, 4 words for software".
 *
 * That's silly, the hardware doesn't care. The hardware only cares that
 * the hardware words are 16-byte aligned, and we can have any amount of
 * sw space after the TD entry as far as I can tell.
 *
 * But let's just go with the documentation, at least for 32-bit machines.
 * On 64-bit machines we probably want to take advantage of the fact that
 * hw doesn't really care about the size of the sw-only area.
 *
 * Alas, not anymore, we have more than 4 words for software, woops.
 * Everything still works tho, surprise! -jerdfelt
 */
struct uhci_td {
	/* Hardware fields */
	__u32 link;
	__u32 status;
	__u32 info;
	__u32 buffer;

	/* Software fields */
	dma_addr_t dma_handle;

	struct usb_device *dev;
	struct urb *urb;

	struct list_head list;		/* P: urb->lock */

	int frame;
	struct list_head fl_list;	/* P: uhci->frame_list_lock */
} __attribute__((aligned(16)));

/*
 * There are various standard queues. We set up several different
 * queues for each of the three basic queue types: interrupt,
 * control, and bulk.
 *
 *  - There are various different interrupt latencies: ranging from
 *    every other USB frame (2 ms apart) to every 256 USB frames (ie
 *    256 ms apart). Make your choice according to how obnoxious you
 *    want to be on the wire, vs how critical latency is for you.
 *  - The control list is done every frame.
 *  - There are 4 bulk lists, so that up to four devices can have a
 *    bulk list of their own and when run concurrently all four lists
 *    will be be serviced.
 *
 * This is a bit misleading, there are various interrupt latencies, but they
 * vary a bit, interrupt2 isn't exactly 2ms, it can vary up to 4ms since the
 * other queues can "override" it. interrupt4 can vary up to 8ms, etc. Minor
 * problem
 *
 * In the case of the root hub, these QH's are just head's of qh's. Don't
 * be scared, it kinda makes sense. Look at this wonderful picture care of
 * Linus:
 *
 *  generic-  ->  dev1-  ->  generic-  ->  dev1-  ->  control-  ->  bulk- -> ...
 *   iso-QH      iso-QH       irq-QH      irq-QH        QH           QH
 *      |           |            |           |           |            |
 *     End     dev1-iso-TD1     End     dev1-irq-TD1    ...          ... 
 *                  |
 *             dev1-iso-TD2
 *                  |
 *                ....
 *
 * This may vary a bit (the UHCI docs don't explicitly say you can put iso
 * transfers in QH's and all of their pictures don't have that either) but
 * other than that, that is what we're doing now
 *
 * And now we don't put Iso transfers in QH's, so we don't waste one on it
 * --jerdfelt
 *
 * To keep with Linus' nomenclature, this is called the QH skeleton. These
 * labels (below) are only signficant to the root hub's QH's
 */

#define UHCI_NUM_SKELTD		10
#define skel_int1_td		skeltd[0]
#define skel_int2_td		skeltd[1]
#define skel_int4_td		skeltd[2]
#define skel_int8_td		skeltd[3]
#define skel_int16_td		skeltd[4]
#define skel_int32_td		skeltd[5]
#define skel_int64_td		skeltd[6]
#define skel_int128_td		skeltd[7]
#define skel_int256_td		skeltd[8]
#define skel_term_td		skeltd[9]	/* To work around PIIX UHCI bug */

#define UHCI_NUM_SKELQH		4
#define skel_ls_control_qh	skelqh[0]
#define skel_hs_control_qh	skelqh[1]
#define skel_bulk_qh		skelqh[2]
#define skel_term_qh		skelqh[3]

/*
 * Search tree for determining where <interval> fits in the
 * skelqh[] skeleton.
 *
 * An interrupt request should be placed into the slowest skelqh[]
 * which meets the interval/period/frequency requirement.
 * An interrupt request is allowed to be faster than <interval> but not slower.
 *
 * For a given <interval>, this function returns the appropriate/matching
 * skelqh[] index value.
 *
 * NOTE: For UHCI, we don't really need int256_qh since the maximum interval
 * is 255 ms.  However, we do need an int1_qh since 1 is a valid interval
 * and we should meet that frequency when requested to do so.
 * This will require some change(s) to the UHCI skeleton.
 */
static inline int __interval_to_skel(int interval)
{
	if (interval < 16) {
		if (interval < 4) {
			if (interval < 2)
				return 0;	/* int1 for 0-1 ms */
			return 1;		/* int2 for 2-3 ms */
		}
		if (interval < 8)
			return 2;		/* int4 for 4-7 ms */
		return 3;			/* int8 for 8-15 ms */
	}
	if (interval < 64) {
		if (interval < 32)
			return 4;		/* int16 for 16-31 ms */
		return 5;			/* int32 for 32-63 ms */
	}
	if (interval < 128)
		return 6;			/* int64 for 64-127 ms */
	return 7;				/* int128 for 128-255 ms (Max.) */
}

struct virt_root_hub {
	struct usb_device *dev;
	int devnum;		/* Address of Root Hub endpoint */
	struct urb *urb;
	void *int_addr;
	int send;
	int interval;
	int numports;
	int c_p_r[8];
	struct timer_list rh_int_timer;
};

/*
 * This describes the full uhci information.
 *
 * Note how the "proper" USB information is just
 * a subset of what the full implementation needs.
 */
struct uhci {
	struct pci_dev *dev;

#ifdef CONFIG_PROC_FS
	/* procfs */
	int num;
	struct proc_dir_entry *proc_entry;
#endif

	/* Grabbed from PCI */
	int irq;
	unsigned int io_addr;
	unsigned int io_size;

	struct pci_pool *qh_pool;
	struct pci_pool *td_pool;

	struct usb_bus *bus;

	struct uhci_td *skeltd[UHCI_NUM_SKELTD];	/* Skeleton TD's */
	struct uhci_qh *skelqh[UHCI_NUM_SKELQH];	/* Skeleton QH's */

	spinlock_t frame_list_lock;
	struct uhci_frame_list *fl;		/* P: uhci->frame_list_lock */
	int fsbr;				/* Full speed bandwidth reclamation */
	unsigned long fsbrtimeout;		/* FSBR delay */
	int is_suspended;

	/* Main list of URB's currently controlled by this HC */
	spinlock_t urb_list_lock;
	struct list_head urb_list;		/* P: uhci->urb_list_lock */

	/* List of QH's that are done, but waiting to be unlinked (race) */
	spinlock_t qh_remove_list_lock;
	struct list_head qh_remove_list;	/* P: uhci->qh_remove_list_lock */

	/* List of asynchronously unlinked URB's */
	spinlock_t urb_remove_list_lock;
	struct list_head urb_remove_list;	/* P: uhci->urb_remove_list_lock */

	/* List of URB's awaiting completion callback */
	spinlock_t complete_list_lock;
	struct list_head complete_list;		/* P: uhci->complete_list_lock */

	struct virt_root_hub rh;	/* private data of the virtual root hub */
};

struct urb_priv {
	struct urb *urb;
	struct usb_device *dev;

	dma_addr_t setup_packet_dma_handle;
	dma_addr_t transfer_buffer_dma_handle;

	struct uhci_qh *qh;		/* QH for this URB */
	struct list_head td_list;	/* P: urb->lock */

	int fsbr : 1;			/* URB turned on FSBR */
	int fsbr_timeout : 1;		/* URB timed out on FSBR */
	int queued : 1;			/* QH was queued (not linked in) */
	int short_control_packet : 1;	/* If we get a short packet during */
					/*  a control transfer, retrigger */
					/*  the status phase */

	int status;			/* Final status */

	unsigned long inserttime;	/* In jiffies */
	unsigned long fsbrtime;		/* In jiffies */

	struct list_head queue_list;	/* P: uhci->frame_list_lock */
	struct list_head complete_list;	/* P: uhci->complete_list_lock */
};

/*
 * Locking in uhci.c
 *
 * spinlocks are used extensively to protect the many lists and data
 * structures we have. It's not that pretty, but it's necessary. We
 * need to be done with all of the locks (except complete_list_lock) when
 * we call urb->complete. I've tried to make it simple enough so I don't
 * have to spend hours racking my brain trying to figure out if the
 * locking is safe.
 *
 * Here's the safe locking order to prevent deadlocks:
 *
 * #1 uhci->urb_list_lock
 * #2 urb->lock
 * #3 uhci->urb_remove_list_lock, uhci->frame_list_lock, 
 *   uhci->qh_remove_list_lock
 * #4 uhci->complete_list_lock
 *
 * If you're going to grab 2 or more locks at once, ALWAYS grab the lock
 * at the lowest level FIRST and NEVER grab locks at the same level at the
 * same time.
 * 
 * So, if you need uhci->urb_list_lock, grab it before you grab urb->lock
 */

/* -------------------------------------------------------------------------
   Virtual Root HUB
   ------------------------------------------------------------------------- */
/* destination of request */
#define RH_DEVICE		0x00
#define RH_INTERFACE		0x01
#define RH_ENDPOINT		0x02
#define RH_OTHER		0x03

#define RH_CLASS		0x20
#define RH_VENDOR		0x40

/* Requests: bRequest << 8 | bmRequestType */
#define RH_GET_STATUS		0x0080
#define RH_CLEAR_FEATURE	0x0100
#define RH_SET_FEATURE		0x0300
#define RH_SET_ADDRESS		0x0500
#define RH_GET_DESCRIPTOR	0x0680
#define RH_SET_DESCRIPTOR	0x0700
#define RH_GET_CONFIGURATION	0x0880
#define RH_SET_CONFIGURATION	0x0900
#define RH_GET_STATE		0x0280
#define RH_GET_INTERFACE	0x0A80
#define RH_SET_INTERFACE	0x0B00
#define RH_SYNC_FRAME		0x0C80
/* Our Vendor Specific Request */
#define RH_SET_EP		0x2000

/* Hub port features */
#define RH_PORT_CONNECTION	0x00
#define RH_PORT_ENABLE		0x01
#define RH_PORT_SUSPEND		0x02
#define RH_PORT_OVER_CURRENT	0x03
#define RH_PORT_RESET		0x04
#define RH_PORT_POWER		0x08
#define RH_PORT_LOW_SPEED	0x09
#define RH_C_PORT_CONNECTION	0x10
#define RH_C_PORT_ENABLE	0x11
#define RH_C_PORT_SUSPEND	0x12
#define RH_C_PORT_OVER_CURRENT	0x13
#define RH_C_PORT_RESET		0x14

/* Hub features */
#define RH_C_HUB_LOCAL_POWER	0x00
#define RH_C_HUB_OVER_CURRENT	0x01
#define RH_DEVICE_REMOTE_WAKEUP	0x00
#define RH_ENDPOINT_STALL	0x01

/* Our Vendor Specific feature */
#define RH_REMOVE_EP		0x00

#define RH_ACK			0x01
#define RH_REQ_ERR		-1
#define RH_NACK			0x00

#endif

