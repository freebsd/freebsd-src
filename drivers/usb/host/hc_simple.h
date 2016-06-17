/*-------------------------------------------------------------------------*/
/* list of all controllers using this driver 
 * */

static LIST_HEAD (hci_hcd_list);

/* URB states (urb_state) */
/* isoc, interrupt single state */

/* bulk transfer main state and 0-length packet */
#define US_BULK		0
#define US_BULK0	1
/* three setup states */
#define US_CTRL_SETUP	2
#define US_CTRL_DATA	1
#define US_CTRL_ACK	0

/*-------------------------------------------------------------------------*/
/* HC private part of a device descriptor
 * */

#define NUM_EDS 32

typedef struct epd {
	struct urb *pipe_head;
	struct list_head urb_queue;
//	int urb_state;
	struct timer_list timeout;
	int last_iso;		/* timestamp of last queued ISOC transfer */

} epd_t;

struct hci_device {
	epd_t ed[NUM_EDS];
};

/*-------------------------------------------------------------------------*/
/* Virtual Root HUB 
 */

#define usb_to_hci(usb)	((struct hci_device *)(usb)->hcpriv)

struct virt_root_hub {
	int devnum;		/* Address of Root Hub endpoint */
	void *urb;		/* interrupt URB of root hub */
	int send;		/* active flag */
	int interval;		/* intervall of roothub interrupt transfers */
	struct timer_list rh_int_timer;	/* intervall timer for rh interrupt EP */
};

#if 1
/* USB HUB CONSTANTS (not OHCI-specific; see hub.h and USB spec) */

/* destination of request */
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

#endif

/*-------------------------------------------------------------------------*/
/* struct for each HC 
 */

#define MAX_TRANS	32

typedef struct td {
	struct urb *urb;
	__u16 len;
	__u16 iso_index;
} td_t;

typedef struct td_array {
	int len;
	td_t td[MAX_TRANS];
} td_array_t;

typedef struct hci {
	struct virt_root_hub rh;	/* roothub */
	wait_queue_head_t waitq;	/* deletion of URBs and devices needs a waitqueue */
	int active;			/* HC is operating */

	struct list_head ctrl_list;	/* set of ctrl endpoints */
	struct list_head bulk_list;	/* set of bulk endpoints */
	struct list_head iso_list;	/* set of isoc endpoints */
	struct list_head intr_list;	/* ordered (tree) set of int endpoints */
	struct list_head del_list;	/* set of entpoints to be deleted */

	td_array_t *td_array;
	td_array_t a_td_array;
	td_array_t i_td_array[2];

	struct list_head hci_hcd_list;	/* list of all hci_hcd */
	struct usb_bus *bus;		/* our bus */

//	int trans;			/* number of transactions pending */
	int active_urbs;
	int active_trans;
	int frame_number;		/* frame number */
	hcipriv_t hp;			/* individual part of hc type */
	int nakCnt;
	int last_packet_nak;

} hci_t;

/*-------------------------------------------------------------------------*/
/* condition (error) CC codes and mapping OHCI like
 */

#define TD_CC_NOERROR		0x00
#define TD_CC_CRC		0x01
#define TD_CC_BITSTUFFING	0x02
#define TD_CC_DATATOGGLEM	0x03
#define TD_CC_STALL		0x04
#define TD_DEVNOTRESP		0x05
#define TD_PIDCHECKFAIL		0x06
#define TD_UNEXPECTEDPID	0x07
#define TD_DATAOVERRUN		0x08
#define TD_DATAUNDERRUN		0x09
#define TD_BUFFEROVERRUN	0x0C
#define TD_BUFFERUNDERRUN	0x0D
#define TD_NOTACCESSED		0x0F


/* urb interface functions */
static int hci_get_current_frame_number (struct usb_device *usb_dev);
static int hci_unlink_urb (struct urb * urb);

static int qu_queue_urb (hci_t * hci, struct urb * urb);

/* root hub */
static int rh_init_int_timer (struct urb * urb);
static int rh_submit_urb (struct urb * urb);
static int rh_unlink_urb (struct urb * urb);

/* schedule functions */
static int sh_add_packet (hci_t * hci, struct urb * urb);

/* hc specific functions */
static inline void hc_flush_data_cache (hci_t * hci, void *data, int len);
static inline int hc_parse_trans (hci_t * hci, int *actbytes, __u8 * data,
				  int *cc, int *toggle, int length, int pid,
				  int urb_state);
static inline int hc_add_trans (hci_t * hci, int len, void *data, int toggle,
				int maxps, int slow, int endpoint, int address,
				int pid, int format, int urb_state);

static void hc_start_int (hci_t * hci);
static void hc_stop_int (hci_t * hci);

/* debug| print the main components of an URB     
 * small: 0) header + data packets 1) just header */

static void urb_print (struct urb * urb, char *str, int small)
{
	unsigned int pipe = urb->pipe;
	int i, len;

	if (!urb->dev || !urb->dev->bus) {
		dbg ("%s URB: no dev", str);
		return;
	}

	printk ("%s URB:[%4x] dev:%2d,ep:%2d-%c,type:%s,flags:%4x,len:%d/%d,stat:%d(%x)\n",
		str, hci_get_current_frame_number (urb->dev),
		usb_pipedevice (pipe), usb_pipeendpoint (pipe),
		usb_pipeout (pipe) ? 'O' : 'I',
		usb_pipetype (pipe) < 2 ? (usb_pipeint (pipe) ? "INTR" : "ISOC")
		: (usb_pipecontrol (pipe) ? "CTRL" : "BULK"), urb->transfer_flags,
		urb->actual_length, urb->transfer_buffer_length, urb->status,
		urb->status);
	if (!small) {
		if (usb_pipecontrol (pipe)) {
			printk (__FILE__ ": cmd(8):");
			for (i = 0; i < 8; i++)
				printk (" %02x", ((__u8 *) urb->setup_packet)[i]);
			printk ("\n");
		}
		if (urb->transfer_buffer_length > 0 && urb->transfer_buffer) {
			printk (__FILE__ ": data(%d/%d):", urb->actual_length,
				urb->transfer_buffer_length);
			len = usb_pipeout (pipe) ? urb-> transfer_buffer_length : urb->actual_length;
			for (i = 0; i < 2096 && i < len; i++)
				printk (" %02x", ((__u8 *) urb->transfer_buffer)[i]);
			printk ("%s stat:%d\n", i < len ? "..." : "",
				urb->status);
		}
	}
}
