/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 *	$Id: scsiconf.h,v 1.42 1996/06/14 11:02:18 asami Exp $
 */
#ifndef	SCSI_SCSICONF_H
#define SCSI_SCSICONF_H 1
typedef	int			boolean;
typedef	int			errval;

#include <scsi/scsi_debug.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_driver.h>

/*
 * The following documentation tries to describe the relationship between the
 * various structures defined in this file:
 *
 * each adapter type has a scsi_adapter struct. This describes the adapter and
 *    identifies routines that can be called to use the adapter.
 * each device type has a scsi_device struct. This describes the device and
 *    identifies routines that can be called to use the device.
 * each existing device position (scsibus + target + lun)
 *    can be described by a scsi_link struct.
 *    Only scsi positions that actually have devices, have a scsi_link
 *    structure assigned. so in effect each device has scsi_link struct.
 *    The scsi_link structure contains information identifying both the
 *    device driver and the adapter driver for that position on that scsi bus,
 *    and can be said to 'link' the two.
 * each individual scsi bus has an array that points to all the scsi_link
 *    structs associated with that scsi bus. Slots with no device have
 *    a NULL pointer.
 * each individual device also knows the address of it's own scsi_link
 *    structure.
 *
 *				-------------
 *
 * The key to all this is the scsi_link structure which associates all the
 * other structures with each other in the correct configuration.  The
 * scsi_link is the connecting information that allows each part of the
 * scsi system to find the associated other parts.
 */

struct buf;
struct scsi_xfer;
#ifdef PC98
struct cfdata;
#endif

/*
 * These entrypoints are called by the high-end drivers to get services from
 * whatever low-end drivers they are attached to each adapter type has one of
 * these statically allocated.
 */
struct scsi_adapter
{
/* 04*/	int32_t		(*scsi_cmd) __P((struct scsi_xfer *xs));
/* 08*/	void		(*scsi_minphys) __P((struct buf *bp));
#ifdef PC98
/* 12*/	int32_t		(*open_target_lu) __P((struct scsi_link *sc_link,
										   struct cfdata *cf));
#else
/* 12*/	int32_t		(*open_target_lu) __P((void));
#endif
/* 16*/	int32_t		(*close_target_lu) __P((void));
/* 20*/	u_int32_t		(*adapter_info) __P((int unit)); /* see definitions below */
/* 24*/	char		*name; /* name of scsi bus controller */
/* 32*/	u_long	spare[2];
};

/*
 * return values for scsi_cmd()
 */
#define SUCCESSFULLY_QUEUED	0
#define TRY_AGAIN_LATER		1
#define	COMPLETE		2
#define	HAD_ERROR		3 /* do not use this, use COMPLETE */
#define	ESCAPE_NOT_SUPPORTED	4

/*
 * Return value from sense handler.  IMHO, These ought to be merged
 * in with the return codes above, all made negative to distinguish
 * from valid errno values, and replace "try again later" with "do retry"
 */
#define SCSIRET_CONTINUE -1	/* Continue with standard sense processing */
#define SCSIRET_DO_RETRY -2	/* Retry the command that got this sense */

/*
 * Format of adapter_info() response data
 * e.g. maximum number of entries queuable to a device by the adapter
 */

/* Don't poke around inside of "scsi_data".  Each low level
 * driver has its own definition for it.
 */
struct scsi_data;
struct scsi_link;	/* scsi_link refers to scsi_device and vice-versa */

struct proc;

/*
 * These entry points are called by the low-end drivers to get services from
 * whatever high-end drivers they are attached to.  Each device type has one
 * of these statically allocated.
 *
 * XXX dufault@hda.com: Each adapter driver has a scsi_device structure
 *     that I don't think should be there.
 *     This structure should be rearranged and cleaned up once the
 *     instance down in the adapter drivers is removed.
 */

/*
 * XXX <devconf.h> already includes too much; avoid including <conf.h>.
 */
typedef int yet_another_d_open_t __P((dev_t, int, int, struct proc *));

struct scsi_device
{
/*  4*/	errval (*err_handler)(struct scsi_xfer *xs);	/* return -1 to say
							 * err processing complete */
/*  8*/	void	(*start)(u_int32_t unit, u_int32_t flags);
/* 12*/	int32_t	(*async) __P((void));
/* 16*/	int32_t	(*done) __P((struct scsi_xfer *xs));	/* returns -1 to say done processing complete */
/* 20*/	char	*name;		/* name of device type */
/* 24*/	u_int32_t flags;		/* device type dependent flags */
/* 32*/	int32_t	spare[2];

/* 36*/ int32_t	link_flags;	/* Flags OR'd into sc_link at attach time */
/* 40*/ errval  (*attach)(struct scsi_link *sc_link);
/* 44*/ char	*desc;		/* Description of device */
/* 48*/ yet_another_d_open_t *open;
/* 52*/ int sizeof_scsi_data;
/* 56*/ int type;		/* Type of device this supports */
/* 60*/ int	(*getunit)(dev_t dev);
/* 64*/ dev_t  (*setunit)(dev_t dev, int unit);

/* 68*/ int (*dev_open)(dev_t dev, int flags, int fmt, struct proc *p,
         struct scsi_link *sc_link);
/* 72*/ int (*dev_ioctl)(dev_t dev, int cmd, caddr_t arg, int mode,
         struct proc *p, struct scsi_link *sc_link);
/* 76*/ int (*dev_close)(dev_t dev, int flag, int fmt, struct proc *p,
         struct scsi_link *sc_link);
/* 80*/ void (*dev_strategy)(struct buf *bp, struct scsi_link *sc_link);

	/* Not initialized after this */

#define SCSI_LINK(DEV, UNIT) ( \
	(struct scsi_link *)(extend_get((DEV)->links, (UNIT))) \
	)

#define SCSI_DATA(DEV, UNIT) ( \
	(SCSI_LINK((DEV), (UNIT)) ? \
	(SCSI_LINK((DEV), (UNIT))->sd) : \
	(struct scsi_data *)0) \
	)

/* 80*/ struct extend_array *links;

/* 84*/ int free_unit;
/* 88*/ struct scsi_device *next;	/* Next in list in the registry. */
};

/* SCSI_DEVICE_ENTRIES: A macro to generate all the entry points from the
 * name.
 */
#define SCSI_DEVICE_ENTRIES(NAME) \
static errval NAME##attach(struct scsi_link *sc_link); \
extern struct scsi_device NAME##_switch;	/* XXX actually static */ \
void NAME##init(void) { \
	scsi_device_register(&NAME##_switch); \
} \
static int NAME##open(dev_t dev, int flags, int fmt, struct proc *p) { \
	return scsi_open(dev, flags, fmt, p, &NAME##_switch); \
} \
static int NAME##ioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p) { \
	return scsi_ioctl(dev, cmd, addr, flag, p, &NAME##_switch); \
} \
static int NAME##close(dev_t dev, int flag, int fmt, struct proc *p) { \
	return scsi_close(dev, flag, fmt, p, &NAME##_switch); \
} \
static void NAME##minphys(struct buf *bp) { \
	scsi_minphys(bp, &NAME##_switch); \
}  \
static void NAME##strategy(struct buf *bp) { \
	scsi_strategy(bp, &NAME##_switch); \
}

#ifdef KERNEL
/* Configuration tables for config.
 */
/* A unit, type, etc can be SCCONF_ANY to indicate it is a '?'
 *  in the config.
 */
#define SCCONF_UNSPEC 255
#define SCCONF_ANY 254

struct isa_driver;
struct scsi_ctlr_config
{
	int scbus;
	char *driver;
	int unit;
	int bus;
};

struct scsi_device_config
{
	char *name;		/* SCSI device name (sd, st, etc) */
	int unit;		/* desired device unit */
	int cunit;		/* Controller unit */
	int target;		/* SCSI ID (target) */
	int lun;		/* SCSI lun */
	int flags;		/* Flags from config */
};

extern void (*scsi_tinit[])(void);
extern struct scsi_ctlr_config scsi_cinit[];
extern struct scsi_device_config scsi_dinit[];

#endif

/*
 * Define various devices that we know mis-behave in some way,
 * and note how they are bad, so we can correct for them
 */
struct st_mode {
/*  4*/	u_int32_t blksiz;
/*  6*/	u_int16_t quirks;		/* same definitions as in XXX */
/*  7*/	char    density;
/*  8*/	char    spare[1];
};

typedef struct st_mode st_modes[4];

/* define behaviour codes (quirks) */
/* common to all SCSI devices */
#define SCSI_Q_NO_SYNC		0x8000
#define SCSI_Q_NO_FAST		0x4000
#define SCSI_Q_NO_WIDE		0x2000

/* tape specific ST_Q_* */
#define ST_Q_NEEDS_PAGE_0	0x0001
#define ST_Q_FORCE_FIXED_MODE	0x0002
#define ST_Q_FORCE_VAR_MODE	0x0004
#define ST_Q_SNS_HLP		0x0008	/* must do READ for good MODE SENSE */
#define ST_Q_IGNORE_LOADS	0x0010
#define ST_Q_BLKSIZ		0x0020	/* variable-block media_blksiz > 0 */
#define ST_Q_CC_NOMSG		0x0040	/* no messages accepted in CC state */
#define ST_Q_NO_1024		0x0080	/* never ever use 1024-byte fix blk */

#define ST_Q_NO_SYNC		SCSI_Q_NO_SYNC
#define ST_Q_NO_FAST		SCSI_Q_NO_FAST
#define ST_Q_NO_WIDE		SCSI_Q_NO_WIDE

/* disk specific SD_Q_* */
#define SD_Q_NO_TAGS		0x0001

#define SD_Q_NO_SYNC		SCSI_Q_NO_SYNC
#define SD_Q_NO_FAST		SCSI_Q_NO_FAST
#define SD_Q_NO_WIDE		SCSI_Q_NO_WIDE

/* cd specific CD_Q_* */
#define CD_Q_NO_TOUCH		0x0001


/*
 * This structure describes the connection between an adapter driver and
 * a device driver, and is used by each to call services provided by
 * the other, and to allow generic scsi glue code to call these services
 * as well.
 */
struct scsi_link
{
	u_int8_t	target;			/* targ of this dev */
	u_int8_t	lun;			/* lun of this dev */
	u_int8_t	adapter_targ;		/* what are we on the scsi bus */
	u_int8_t	adapter_unit;		/* e.g. the 0 in aha0 */
	u_int8_t	adapter_bus;		/* e.g. the 0 in bus0 */
	u_int8_t	scsibus;		/* the Nth scsibus	*/
	u_int8_t	dev_unit;		/* e.g. the 0 in sd0 */
	u_int8_t	opennings;		/* available operations */
	u_int8_t	active;			/* operations in progress */
	u_int16_t	flags;			/* flags that all devices have */
	u_int16_t	quirks;			/* device specific quirks */
	struct	scsi_adapter *adapter;	/* adapter entry points etc. */
	struct	scsi_device *device;	/* device entry points etc. */
	struct	scsi_xfer *active_xs;	/* operations under way */
	void *	fordriver;		/* for private use by the driver */
	void *  devmodes;		/* device specific mode tables */
	dev_t	dev;			/* Device major number (character) */
	struct	scsi_data *sd;	/* Device data structure */
	struct	scsi_inquiry_data inqbuf;	/* Inquiry data */
	void	*adapter_softc;		/* needed for call to foo_scsi_cmd */
};

/* XXX-HA: dufault@hda.com: SDEV_BOUNCE is set down in the adapter drivers
 * in an sc_link structure to indicate that this host adapter requires
 * ISA DMA bounce buffers.  I think the link structure should
 * be associated only with the type drive and not the adapter driver,
 * and the bounce flag should be in something associated with the
 * adapter driver.
 * XXX-HA And I added the "supports residuals properly" flag that ALSO goes
 * in an adapter structure.  I figure I'll fix both at once.
 *
 * XXX SDEV_OPEN is used for two things: To prevent more than one
 * open and to make unit attentions errors be logged on the console.
 * These should be split up; I'm adding SDEV_IS_OPEN to enforce one
 * open only.
 *
 * XXX SDEV_UK is used to mark the "uk" device.
 */

#define	SDEV_MEDIA_LOADED 	0x0001	/* device figures are still valid */
#define	SDEV_WAITING	 	0x0002	/* a process is waiting for this */
#define	SDEV_OPEN	 		0x0004	/* at least 1 open session */
#define SDEV_BOUNCE			0x0008	/* XXX-HA: unit needs DMA bounce buffer */
#define	SDEV_DBX			0x00F0	/* debugging flags (scsi_debug.h) */
#define SDEV_ONCE_ONLY		0x0100	/* unit can only be opened once */
#define SDEV_BOOTVERBOSE	0x0200	/* be noisy during boot */
#define SDEV_RESIDS_WORK	0x0400	/* XXX-HA: Residuals work */
#define SDEV_TARGET_OPS 	0x0800	/* XXX-HA: Supports target ops  */
#define	SDEV_IS_OPEN 		0x1000	/* at least 1 open session */
#define SDEV_UK			0x2000	/* this is the "uk" device */

/*
 * One of these is allocated and filled in for each scsi bus.
 * it holds pointers to allow the scsi bus to get to the driver
 * That is running each LUN on the bus
 * it also has a template entry which is the prototype struct
 * supplied by the adapter driver, this is used to initialise
 * the others, before they have the rest of the fields filled in
 */
struct scsibus_data {
	u_char		 maxtarg;
	u_char		 maxlun;
	struct scsi_link *adapter_link;	/* prototype supplied by adapter */
	struct scsi_link *(*sc_link)[][8]; /* dynamically allocated */
};

/*
 * Each scsi transaction is fully described by one of these structures
 * It includes information about the source of the command and also the
 * device and adapter for which the command is destined.
 * (via the scsi_link structure)						*
 */
struct scsi_xfer
{
/*04*/	struct	scsi_xfer *next;	/* when free */
/*08*/	u_int32_t	flags;
/*12*/	struct	scsi_link *sc_link;	/* all about our device and adapter */
/*13*/	u_int8_t	retries;		/* the number of times to retry */
/*16*/	u_int8_t	spare[3];
/*20*/	int32_t	timeout;		/* in milliseconds */
/*24*/	struct	scsi_generic *cmd;	/* The scsi command to execute */
/*28*/	int32_t	cmdlen;			/* how long it is */
/*32*/	u_char	*data;			/* dma address OR a uio address */
/*36*/	int32_t	datalen;		/* data len (blank if uio)    */
/*40*/	int32_t	resid;			/* how much buffer was not touched */
/*44*/	int32_t	error;			/* an error value	*/
/*48*/	struct	buf *bp;		/* If we need to associate with a buf */
/*80*/	struct	scsi_sense_data	sense; /* 32 bytes*/
	/*
	 * Believe it or not, Some targets fall on the ground with
	 * anything but a certain sense length.
	 */
/*84*/	int32_t req_sense_length;	/* Explicit request sense length */
/*88*/	int32_t status;			/* SCSI status */
/*100*/	struct	scsi_generic cmdstore;	/* stash the command in here */
};

/*
 * Per-request Flag values
 */
#define	SCSI_NOSLEEP	0x01	/* Not a user... don't sleep		*/
#define	SCSI_NOMASK	0x02	/* dont allow interrupts.. booting	*/
#define	SCSI_NOSTART	0x04	/* left over from ancient history	*/
#define	SCSI_USER	0x08	/* Is a user cmd, call scsi_user_done	*/
#define	SCSI_ITSDONE	0x10	/* the transfer is as done as it gets	*/
#define	ITSDONE		0x10	/* the transfer is as done as it gets	*/
#define	SCSI_INUSE	0x20	/* The scsi_xfer block is in use	*/
#define	INUSE		0x20	/* The scsi_xfer block is in use	*/
#define	SCSI_SILENT	0x40	/* Don't report errors to console	*/
#define SCSI_ERR_OK	0x80	/* An error on this operation is OK.	*/
#define	SCSI_RESET	0x100	/* Reset the device in question		*/
#define	SCSI_DATA_UIO	0x200	/* The data address refers to a UIO	*/
#define	SCSI_DATA_IN	0x400	/* expect data to come INTO memory	*/
#define	SCSI_DATA_OUT	0x800	/* expect data to flow OUT of memory	*/
#define	SCSI_TARGET	0x1000	/* This defines a TARGET mode op.	*/
#define	SCSI_ESCAPE	0x2000	/* Escape operation			*/
#define	SCSI_EOF	0x4000	/* The operation should return EOF	*/
#define	SCSI_RESID_VALID 0x8000	/* The resid field contains valid data	*/

/*
 * Escape op codes.  This provides an extensible setup for operations
 * that are not scsi commands.  They are intended for modal operations.
 */

#define SCSI_OP_TARGET	0x0001
#define	SCSI_OP_RESET	0x0002
#define	SCSI_OP_BDINFO	0x0003

/*
 * Error values an adapter driver may return
 */
#define XS_NOERROR	0x0	/* there is no error, (sense is invalid)  */
#define XS_SENSE	0x1	/* Check the returned sense for the error */
#define	XS_DRIVER_STUFFUP 0x2	/* Driver failed to perform operation	  */
#define XS_TIMEOUT	0x03	/* The device timed out.. turned off?	  */
#define XS_SWTIMEOUT	0x04	/* The Timeout reported was caught by SW  */
#define XS_BUSY		0x08	/* The device busy, try again later?	  */
#define XS_LENGTH	0x09	/* Illegal length (over/under run)	  */
#define XS_SELTIMEOUT	0x10	/* Device failed to respond to selection  */

#ifdef KERNEL
void *extend_get(struct extend_array *ea, int index);
void scsi_attachdevs __P((struct scsibus_data *scbus));
u_int32_t scsi_read_capacity __P(( struct scsi_link *sc_link,
	u_int32_t *blk_size, u_int32_t flags));
errval scsi_test_unit_ready __P(( struct scsi_link *sc_link, u_int32_t flags));
errval scsi_reset_target __P((struct scsi_link *));
errval scsi_target_mode __P((struct scsi_link *, int));
errval scsi_inquire( struct scsi_link *sc_link,
			struct scsi_inquiry_data *inqbuf, u_int32_t flags);
errval scsi_prevent( struct scsi_link *sc_link, u_int32_t type,u_int32_t flags);
struct scsibus_data *scsi_alloc_bus __P((void));
errval scsi_probe_bus __P((int, int, int));
errval scsi_probe_busses __P(( int, int, int));
errval scsi_start_unit( struct scsi_link *sc_link, u_int32_t flags);
errval scsi_stop_unit(struct scsi_link *sc_link, u_int32_t eject, u_int32_t flags);
void scsi_done(struct scsi_xfer *xs);
void scsi_user_done(struct scsi_xfer *xs);
errval scsi_scsi_cmd __P(( struct scsi_link *, struct scsi_generic *,
			u_int32_t, u_char *,
			u_int32_t, u_int32_t,
			u_int32_t, struct buf *,
			u_int32_t));
int	scsi_do_ioctl __P((dev_t dev, int cmd, caddr_t addr, int mode,
        struct proc *p, struct scsi_link *sc_link));

struct scsi_link *scsi_link_get __P((int bus, int targ, int lun));
dev_t scsi_dev_lookup __P((int (*opener)(dev_t dev, int flags, int fmt,
struct proc *p)));

int scsi_opened_ok __P((dev_t dev, int flag, int type, struct scsi_link *sc_link));
errval scsi_set_bus __P((int, struct scsi_link *));

char	*scsi_sense_desc	__P((int, int));
void	scsi_sense_print	__P((struct scsi_xfer *));
void	show_scsi_cmd		__P((struct scsi_xfer *));

void	scsi_uto3b __P((u_int32_t , u_char *));
u_int32_t	scsi_3btou __P((u_char *));
int32_t	scsi_3btoi __P((u_char *));
void	scsi_uto4b __P((u_int32_t, u_char *));
u_int32_t	scsi_4btou __P((u_char *));
void	scsi_uto2b __P((u_int32_t, u_char *));
u_int32_t	scsi_2btou __P((u_char *));

void sc_print_addr __P((struct scsi_link *));
void sc_print_start __P((struct scsi_link *));
void sc_print_finish __P((void));

struct sysctl_req;
int	scsi_externalize __P((struct scsi_link *, struct sysctl_req *));

void	scsi_device_register __P((struct scsi_device *sd));

extern struct kern_devconf kdc_scbus0; /* XXX should go away */

void scsi_configure_start __P((void));
void scsi_configure_finish __P((void));

void ukinit __P((void));

#ifdef SCSI_2_DEF
errval scsi_change_def( struct scsi_link *sc_link, u_int32_t flags);
#endif
#endif	/* KERNEL */

#define SCSI_EXTERNALLEN (sizeof(struct scsi_link))


/* XXX This belongs in a tape file.
 */

/**********************************************************************
			from the scsi2 spec
                Value Tracks Density(bpi) Code Type  Reference     Note
                0x1     9       800       NRZI  R    X3.22-1983    2
                0x2     9      1600       PE    R    X3.39-1986    2
                0x3     9      6250       GCR   R    X3.54-1986    2
                0x5    4/9     8000       GCR   C    X3.136-1986   1
                0x6     9      3200       PE    R    X3.157-1987   2
                0x7     4      6400       IMFM  C    X3.116-1986   1
                0x8     4      8000       GCR   CS   X3.158-1986   1
                0x9    18     37871       GCR   C    X3B5/87-099   2
                0xA    22      6667       MFM   C    X3B5/86-199   1
                0xB     4      1600       PE    C    X3.56-1986    1
                0xC    24     12690       GCR   C    HI-TC1        1,5
                0xD    24     25380       GCR   C    HI-TC2        1,5
                0xF    15     10000       GCR   C    QIC-120       1,5
                0x10   18     10000       GCR   C    QIC-150       1,5
                0x11   26     16000       GCR   C    QIC-320(525?) 1,5
                0x12   30     51667       RLL   C    QIC-1350      1,5
                0x13    1     61000       DDS   CS    X3B5/88-185A 4
                0x14    1     43245       RLL   CS    X3.202-1991  4
                0x15    1     45434       RLL   CS    ECMA TC17    4
                0x16   48     10000       MFM   C     X3.193-1990  1
                0x17   48     42500       MFM   C     X3B5/91-174  1

                where Code means:
                NRZI Non Return to Zero, change on ones
                GCR  Group Code Recording
                PE   Phase Encoded
                IMFM Inverted Modified Frequency Modulation
                MFM  Modified Frequency Modulation
                DDS  Dat Data Storage
                RLL  Run Length Encoding

                where Type means:
                R    Real-to-Real
                C    Cartridge
                CS   cassette

                where Notes means:
                1    Serial Recorded
                2    Parallel Recorded
                3    Old format know as QIC-11
                4    Helical Scan
                5    Not ANSI standard, rather industry standard.

********************************************************************/

#define	HALFINCH_800	0x01
#define	HALFINCH_1600	0x02
#define	HALFINCH_6250	0x03
#define	QIC_11		0x04	/* from Archive 150S Theory of Op. XXX	*/
#define QIC_24		0x05	/* may be bad, works for CIPHER ST150S XXX */
#define QIC_120		0x0f
#define QIC_150		0x10
#define QIC_320		0x11
#define QIC_525		0x11
#define QIC_1320	0x12
#define DDS		0x13
#define DAT_1		0x13
#define	QIC_3080	0x29


/* XXX (dufault@hda.com) This is used only by "su" and "sctarg".
 * The minor number field conflicts with the disk slice code,
 * and so it is tough to access the disks through the "su" device.
 */

/* Device number fields:
 *
 * NON-FIXED SCSI devices:
 *
 * ?FC? ???? ???? ???? MMMMMMMM mmmmmmmm
 *
 * F: Fixed device (nexus in number): must be 0.
 * C: Control device; only user mode ioctl is supported.
 * ?: Don't know; those bits didn't use to exist, currently always 0.
 * M: Major device number.
 * m: Old style minor device number.
 *
 * FIXED SCSI devices:
 *
 * XXX Conflicts with the slice code.  Maybe the slice code can be
 * changed to respect the F bit?
 *
 * ?FC? ?BBB TTTT ?LLL MMMMMMMM mmmmmmmm
 *
 * F: Fixed device (nexus in number); must be 1.
 * C: Control device; only user mode ioctl is supported.
 * B: SCSI bus
 * T: SCSI target ID
 * L: Logical unit
 * M: Major device number
 * m: Old style minor device number.
 */

#define SCSI_FIXED_MASK              0x40000000
#define SCSI_FIXED(DEV)    (((DEV) & SCSI_FIXED_MASK))
#define SCSI_CONTROL_MASK  0x20000000
#define SCSI_CONTROL(DEV)  (((DEV) & SCSI_CONTROL_MASK))

#define SCSI_BUS(DEV)      (((DEV) & 0x07000000) >> 24)
#define SCSI_ID(DEV)       (((DEV) & 0x00F00000) >> 20)
#define SCSI_LUN(DEV)      (((DEV) & 0x00070000) >> 16)

#define SCSI_MKFIXED(B, T, L) ( \
         ((B) << 24) | \
         ((T) << 20) | \
         ((L) << 16) | \
         SCSI_FIXED_MASK )

#endif /*SCSI_SCSICONF_H*/
/* END OF FILE */
