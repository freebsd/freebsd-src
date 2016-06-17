/* Header file for the hpusbscsi driver */
/* (C) Copyright 2001 Oliver Neukum */
/* sponsored by the Linux Usb Project */
/* large parts based on or taken from code by John Fremlin and Matt Dharm */
/* this file is licensed under the GPL */

/* A big thanks to Jose for untiring testing */

typedef void (*usb_urb_callback) (struct urb *);
typedef void (*scsi_callback)(Scsi_Cmnd *);

#define SENSE_COMMAND_SIZE 6
#define HPUSBSCSI_SENSE_LENGTH 0x16

struct hpusbscsi
{
        struct list_head lh;
        struct usb_device *dev; /* NULL indicates unplugged device */
        int ep_out;
        int ep_in;
        int ep_int;
        int interrupt_interval;
	int need_short_workaround;

        struct Scsi_Host *host;
        Scsi_Host_Template ctempl;
        int number;
       scsi_callback scallback;
       Scsi_Cmnd *srb;
	u8 sense_command[SENSE_COMMAND_SIZE];

        int use_count;
	struct semaphore lock;
        wait_queue_head_t pending;
        wait_queue_head_t deathrow;

        struct urb dataurb;
        struct urb controlurb;
        int fragment;

        int state;
        int current_data_pipe;

        u8 scsi_state_byte;
};

#define SCSI_ERR_MASK ~0x3fu

static const unsigned char scsi_command_direction[256/8] = {
	0x28, 0x81, 0x14, 0x14, 0x20, 0x01, 0x90, 0x77,
	0x0C, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define DIRECTION_IS_IN(x) ((scsi_command_direction[x>>3] >> (x & 7)) & 1)

static int hpusbscsi_scsi_detect (struct SHT * sht);
static void simple_command_callback(struct urb *u);
static void scatter_gather_callback(struct urb *u);
static void simple_payload_callback (struct urb *u);
static void control_interrupt_callback (struct urb *u);
static void request_sense_callback (struct urb *u);
static void simple_done (struct urb *u);
static int hpusbscsi_scsi_queuecommand (Scsi_Cmnd *srb, scsi_callback callback);
static int hpusbscsi_scsi_host_reset (Scsi_Cmnd *srb);
static int hpusbscsi_scsi_abort (Scsi_Cmnd *srb);
static void issue_request_sense (struct hpusbscsi *hpusbscsi);

static Scsi_Host_Template hpusbscsi_scsi_host_template = {
	name:           "hpusbscsi",
	detect:		hpusbscsi_scsi_detect,
//	release:	hpusbscsi_scsi_release,
	queuecommand:	hpusbscsi_scsi_queuecommand,

	eh_abort_handler:	hpusbscsi_scsi_abort,
	eh_host_reset_handler:	hpusbscsi_scsi_host_reset,

	sg_tablesize:		SG_ALL,
	can_queue:		1,
	this_id:		-1,
	cmd_per_lun:		1,
	present:		0,
	unchecked_isa_dma:	FALSE,
	use_clustering:		TRUE,
	use_new_eh_code:	TRUE,
	emulated:		TRUE
};

/* defines for internal driver state */
#define HP_STATE_FREE                 0  /*ready for next request */
#define HP_STATE_BEGINNING      1  /*command being transfered */
#define HP_STATE_WORKING         2  /* data transfer stage */
#define HP_STATE_ERROR             3  /* error has been reported */
#define HP_STATE_WAIT                 4  /* waiting for status transfer */
#define HP_STATE_PREMATURE              5 /* status prematurely reported */


