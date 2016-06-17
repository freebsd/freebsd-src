#ifndef _LINUX_XD_H
#define _LINUX_XD_H

/*
 * This file contains the definitions for the IO ports and errors etc. for XT hard disk controllers (at least the DTC 5150X).
 *
 * Author: Pat Mackinlay, pat@it.com.au
 * Date: 29/09/92
 *
 * Revised: 01/01/93, ...
 *
 * Ref: DTC 5150X Controller Specification (thanks to Kevin Fowler, kevinf@agora.rain.com)
 * Also thanks to: Salvador Abreu, Dave Thaler, Risto Kankkunen and Wim Van Dorst.
 */

/* XT hard disk controller registers */
#define XD_DATA		(xd_iobase + 0x00)	/* data RW register */
#define XD_RESET	(xd_iobase + 0x01)	/* reset WO register */
#define XD_STATUS	(xd_iobase + 0x01)	/* status RO register */
#define XD_SELECT	(xd_iobase + 0x02)	/* select WO register */
#define XD_JUMPER	(xd_iobase + 0x02)	/* jumper RO register */
#define XD_CONTROL	(xd_iobase + 0x03)	/* DMAE/INTE WO register */
#define XD_RESERVED	(xd_iobase + 0x03)	/* reserved */

/* XT hard disk controller commands (incomplete list) */
#define CMD_TESTREADY	0x00	/* test drive ready */
#define CMD_RECALIBRATE	0x01	/* recalibrate drive */
#define CMD_SENSE	0x03	/* request sense */
#define CMD_FORMATDRV	0x04	/* format drive */
#define CMD_VERIFY	0x05	/* read verify */
#define CMD_FORMATTRK	0x06	/* format track */
#define CMD_FORMATBAD	0x07	/* format bad track */
#define CMD_READ	0x08	/* read */
#define CMD_WRITE	0x0A	/* write */
#define CMD_SEEK	0x0B	/* seek */

/* Controller specific commands */
#define CMD_DTCSETPARAM	0x0C	/* set drive parameters (DTC 5150X & CX only?) */
#define CMD_DTCGETECC	0x0D	/* get ecc error length (DTC 5150X only?) */
#define CMD_DTCREADBUF	0x0E	/* read sector buffer (DTC 5150X only?) */
#define CMD_DTCWRITEBUF 0x0F	/* write sector buffer (DTC 5150X only?) */
#define CMD_DTCREMAPTRK	0x11	/* assign alternate track (DTC 5150X only?) */
#define CMD_DTCGETPARAM	0xFB	/* get drive parameters (DTC 5150X only?) */
#define CMD_DTCSETSTEP	0xFC	/* set step rate (DTC 5150X only?) */
#define CMD_DTCSETGEOM	0xFE	/* set geometry data (DTC 5150X only?) */
#define CMD_DTCGETGEOM	0xFF	/* get geometry data (DTC 5150X only?) */
#define CMD_ST11GETGEOM 0xF8	/* get geometry data (Seagate ST11R/M only?) */
#define CMD_WDSETPARAM	0x0C	/* set drive parameters (WD 1004A27X only?) */
#define CMD_XBSETPARAM	0x0C	/* set drive parameters (XEBEC only?) */

/* Bits for command status byte */
#define CSB_ERROR	0x02	/* error */
#define CSB_LUN		0x20	/* logical Unit Number */

/* XT hard disk controller status bits */
#define STAT_READY	0x01	/* controller is ready */
#define STAT_INPUT	0x02	/* data flowing from controller to host */
#define STAT_COMMAND	0x04	/* controller in command phase */
#define STAT_SELECT	0x08	/* controller is selected */
#define STAT_REQUEST	0x10	/* controller requesting data */
#define STAT_INTERRUPT	0x20	/* controller requesting interrupt */

/* XT hard disk controller control bits */
#define PIO_MODE	0x00	/* control bits to set for PIO */
#define DMA_MODE	0x03	/* control bits to set for DMA & interrupt */

#define XD_MAXDRIVES	2	/* maximum 2 drives */
#define XD_TIMEOUT	HZ	/* 1 second timeout */
#define XD_RETRIES	4	/* maximum 4 retries */

#undef DEBUG			/* define for debugging output */

#ifdef DEBUG
	#define DEBUG_STARTUP	/* debug driver initialisation */
	#define DEBUG_OVERRIDE	/* debug override geometry detection */
	#define DEBUG_READWRITE	/* debug each read/write command */
	#define DEBUG_OTHER	/* debug misc. interrupt/DMA stuff */
	#define DEBUG_COMMAND	/* debug each controller command */
#endif /* DEBUG */

/* this structure defines the XT drives and their types */
typedef struct {
	u8 heads;
	u16 cylinders;
	u8 sectors;
	u8 control;
} XD_INFO;

/* this structure is returned to the HDIO_GETGEO ioctl */
typedef struct {
	__u8 heads;
	__u8 sectors;
	__u8 cylinders;
	__u32 start;
} XD_GEOMETRY;

/* this structure defines a ROM BIOS signature */
typedef struct {
	unsigned int offset;
	const char *string;
	void (*init_controller)(unsigned int address);
	void (*init_drive)(u8 drive);
	const char *name;
} XD_SIGNATURE;

#ifndef MODULE
static int xd_manual_geo_init (char *command);
#endif /* MODULE */
static u8 xd_detect (u8 *controller, unsigned int *address);
static u8 xd_initdrives (void (*init_drive)(u8 drive));
static void xd_geninit (void);

static int xd_open (struct inode *inode,struct file *file);
static void do_xd_request (request_queue_t * q);
static int xd_ioctl (struct inode *inode,struct file *file,unsigned int cmd,unsigned long arg);
static int xd_release (struct inode *inode,struct file *file);
static int xd_reread_partitions (kdev_t dev);
static int xd_readwrite (u8 operation,u8 drive,char *buffer,u_int block,u_int count);
static void xd_recalibrate (u8 drive);

static void xd_interrupt_handler (int irq, void *dev_id, struct pt_regs *regs);
static u8 xd_setup_dma (u8 opcode,u8 *buffer,u_int count);
static u8 *xd_build (u8 *cmdblk,u8 command,u8 drive,u8 head,u16 cylinder,u8 sector,u8 count,u8 control);
static void xd_watchdog (unsigned long unused);
static inline u8 xd_waitport (u16 port,u8 flags,u8 mask,unsigned long timeout);
static u_int xd_command (u8 *command,u8 mode,u8 *indata,u8 *outdata,u8 *sense,unsigned long timeout);

/* card specific setup and geometry gathering code */
static void xd_dtc_init_controller (unsigned int address);
static void xd_dtc5150cx_init_drive (u8 drive);
static void xd_dtc_init_drive (u8 drive);
static void xd_wd_init_controller (unsigned int address);
static void xd_wd_init_drive (u8 drive);
static void xd_seagate_init_controller (unsigned int address);
static void xd_seagate_init_drive (u8 drive);
static void xd_omti_init_controller (unsigned int address);
static void xd_omti_init_drive (u8 drive);
static void xd_xebec_init_controller (unsigned int address);
static void xd_xebec_init_drive (u8 drive);
static void xd_setparam (u8 command,u8 drive,u8 heads,u16 cylinders,u16 rwrite,u16 wprecomp,u8 ecc);
static void xd_override_init_drive (u8 drive);

#endif /* _LINUX_XD_H */
