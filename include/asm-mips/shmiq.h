/*
 * Please note that the comments on this file may be out of date
 * and that they represent what I have figured about the shmiq device
 * so far in IRIX.
 *
 * This also contains some streams and idev bits.
 *
 * They may contain errors, please, refer to the source code of the Linux
 * kernel for a definitive answer on what we have implemented
 *
 * Miguel.
 */

/* STREAMs ioctls */
#define STRIOC    ('S' << 8)
#define I_STR     (STRIOC | 010)
#define I_PUSH    (STRIOC | 02)
#define I_LINK    (STRIOC | 014)
#define I_UNLINK  (STRIOC | 015)

/* Data structure passed on I_STR ioctls */
struct strioctl {
        int     ic_cmd;                 /* streams ioctl command */
        int     ic_timout;              /* timeout */
        int     ic_len;                 /* lenght of data */
        void    *ic_dp;                 /* data */
};

/*
 * For mapping the shared memory input queue, you have to:
 *
 * 1. Map /dev/zero for the number of bytes you want to use
 *    for your shared memory input queue plus the size of the
 *    sharedMemoryInputQueue structure + 4 (I still have not figured
 *    what this one is for
 *
 * 2. Open /dev/shmiq
 *
 * 3. Open /dev/qcntlN N is [0..Nshmiqs]
 *
 * 4. Fill a shmiqreq structure.  user_vaddr should point to the return
 *    address from the /dev/zero mmap.  Arg is the number of shmqevents
 *    that fit into the /dev/zero region (remember that at the beginning there
 *    is a sharedMemoryInputQueue header).
 *
 * 5. Issue the ioctl (qcntlfd, QIOCATTACH, &your_shmiqreq);
 */

struct shmiqreq {
	char *user_vaddr;
	int  arg;
};

/* map the shmiq into the process address space */
#define QIOCATTACH       _IOW('Q',1,struct shmiqreq)

/* remove mappings */
#define QIOCDETACH       _IO('Q',2)

/*
 * A shared memory input queue event.
 */
struct shmqdata {
	unsigned char device;          /* device major */
        unsigned char which;           /* device minor */
        unsigned char type;            /* event type */
        unsigned char flags;           /* little event data */
        union {
            int pos;                   /* big event data */
            short ptraxis [2];         /* event data for PTR events */
        } un;
};

/* indetifies the shmiq and the device */
struct shmiqlinkid {
        short int devminor;
        short int index;
};

struct shmqevent {
	union {
                int time;
                struct shmiqlinkid id;
        } un ;
        struct shmqdata data ;
};

/*
 * sharedMemoryInputQueue: this describes the shared memory input queue.
 *
 * head   is the user index into the events, user can modify this one.
 * tail   is managed by the kernel.
 * flags  is one of SHMIQ_OVERFLOW or SHMIQ_CORRUPTED
 *        if OVERFLOW is set it seems ioctl QUIOCSERVICED should be called
 *        to notify the kernel.
 * events where the kernel sticks the events.
 */
struct sharedMemoryInputQueue {
	volatile int head;	     /* user's index into events */
        volatile int tail;	     /* kernel's index into events */
        volatile unsigned int flags; /* place for out-of-band data */
#define SHMIQ_OVERFLOW  1
#define SHMIQ_CORRUPTED 2
        struct shmqevent events[1];  /* input event buffer */
};

/* have to figure this one out */
#define QIOCGETINDX      _IOWR('Q', 8, int)


/* acknowledge shmiq overflow */
#define QIOCSERVICED     _IO('Q', 3)

/* Double indirect I_STR ioctl, yeah, fun fun fun */

struct muxioctl {
        int index;		/* lower stream index */
        int realcmd;		/* the actual command for the subdevice */
};
/* Double indirect ioctl */
#define QIOCIISTR        _IOW('Q', 7, struct muxioctl)

/* Cursor ioclts: */

/* set cursor tracking mode */
#define QIOCURSTRK      _IOW('Q', 4, int)

/* set cursor filter box */
#define QIOCURSIGN      _IOW('Q', 5, int [4])

/* set cursor axes */
struct shmiqsetcurs {
        short index;
        short axes;
};

#define QIOCSETCURS     _IOWR('Q',  9, struct shmiqsetcurs)

/* set cursor position */
struct shmiqsetcpos {
        short   x;
        short   y;
};
#define QIOCSETCPOS     _IOWR('Q', 10, struct shmiqsetcpos)

/* get time since last event */
#define QIOCGETITIME    _IOR('Q', 11, time_t)

/* set current screen */
#define QIOCSETSCRN     _IOW('Q',6,int)


/* -------------------- iDev stuff -------------------- */

#define IDEV_MAX_NAME_LEN 15
#define IDEV_MAX_TYPE_LEN 15

typedef struct {
        char            devName[IDEV_MAX_NAME_LEN+1];
        char            devType[IDEV_MAX_TYPE_LEN+1];
        unsigned short  nButtons;
        unsigned short  nValuators;
        unsigned short  nLEDs;
        unsigned short  nStrDpys;
        unsigned short  nIntDpys;
        unsigned char   nBells;
        unsigned char   flags;
#define IDEV_HAS_KEYMAP		0x01
#define IDEV_HAS_PROXIMITY 	0x02
#define IDEV_HAS_PCKBD 		0x04
} idevDesc;

typedef struct {
	char *nothing_for_now;
} idevInfo;

#define IDEV_KEYMAP_NAME_LEN 15

typedef struct {
        char name[IDEV_KEYMAP_NAME_LEN+1];
} idevKeymapDesc;

/* The valuator definition */
typedef struct {
        unsigned        hwMinRes;
        unsigned        hwMaxRes;
        int             hwMinVal;
        int             hwMaxVal;

        unsigned char   possibleModes;
#define IDEV_ABSOLUTE           0x0
#define IDEV_RELATIVE           0x1
#define IDEV_EITHER             0x2

        unsigned char   mode;	/* One of: IDEV_ABSOLUTE, IDEV_RELATIVE */

        unsigned short  resolution;
        int             minVal;
        int             maxVal;
} idevValuatorDesc;

/* This is used to query a specific valuator with the IDEVGETVALUATORDESC ioctl */
typedef struct {
        short                   valNum;
        unsigned short          flags;
        idevValuatorDesc        desc;
} idevGetSetValDesc;

#define IDEVGETDEVICEDESC	_IOWR('i', 0,  idevDesc)
#define IDEVGETVALUATORDESC     _IOWR('i', 1,  idevGetSetValDesc)
#define IDEVGETKEYMAPDESC	_IOWR('i', 2,  idevKeymapDesc)
#define IDEVINITDEVICE		_IOW ('i', 51, unsigned int)


#ifdef __KERNEL__

/* These are only interpreted by SHMIQ-attacheable devices and are internal
 * to the kernel
 */
#define SHMIQ_OFF        _IO('Q',1)
#define SHMIQ_ON         _IO('Q',2)

void shmiq_push_event (struct shmqevent *e);
int get_sioc (struct strioctl *sioc, unsigned long arg);
#endif
