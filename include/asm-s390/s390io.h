/*
 *  arch/s390/kernel/s390io.h
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Ingo Adlung (adlung@de.ibm.com)
 */

#ifndef __s390io_h
#define __s390io_h
#include <linux/tqueue.h>

/*
 * IRQ data structure used by I/O subroutines
 *
 * Note : If bit flags are added, the "unused" value must be
 *        decremented accordingly !
 */
typedef struct _ioinfo {
     unsigned int  irq;           /* aka. subchannel number */
     spinlock_t    irq_lock;      /* irq lock */
     void          *private_data; /* pointer to private data */

     struct _ioinfo *prev;
     struct _ioinfo *next;

     __u8          st;            /* subchannel type */

     union {
        unsigned int info;
        struct {
           unsigned int  busy      : 1;  /* device currently in use */
           unsigned int  oper      : 1;  /* device is operational */
           unsigned int  fast      : 1;  /* post with "channel end", ...    */
                                         /* ... don't wait for "device end" */
                                         /* ... from do_IO() parameters     */
           unsigned int  ready     : 1;  /* interrupt handler registered */
           unsigned int  haltio    : 1;  /* halt_IO in process */
           unsigned int  doio      : 1;  /* do_IO in process */
           unsigned int  w4final   : 1;  /* wait for final status, internally */
                                         /* ... used with 'fast' setting only */
           unsigned int  repall    : 1;  /* report every interrupt status */
           unsigned int  unready   : 1;  /* deregister irq handler in process */
           unsigned int  d_disable : 1;  /* delayed disabling required */
           unsigned int  w4sense   : 1;  /* SENSE status pending */
           unsigned int  syncio    : 1;  /* synchronous I/O requested */
           unsigned int  consns    : 1;  /* concurrent sense is available */
           unsigned int  delsense  : 1;  /* delayed SENSE required */
           unsigned int  s_pend    : 1;  /* status pending condition */
           unsigned int  pgid      : 1;  /* "path group ID" is valid */
           unsigned int  pgid_supp : 1;  /* "path group ID" command is supported */
           unsigned int  esid      : 1;  /* Ext. SenseID supported by HW */
           unsigned int  repnone   : 1;  /* don't call IRQ handler on interrupt */
           unsigned int  newreq    : 1;  /* new register interface */
           unsigned int  dval      : 1;  /* device number valid */
           unsigned int  unknown   : 1;  /* unknown device - if SenseID failed */
	   unsigned int  unfriendly: 1;  /* device is locked by someone else */
	   unsigned int  killio    : 1;  /* currently killing pending io */
	   unsigned int  noio      : 1;  /* don't let drivers start io */
	   unsigned int  notacccap : 1;  /* capable of handling DEVSTAT_NOT_ACC */
           unsigned int  unused    : (sizeof(unsigned int)*8 - 26); /* unused */
              } __attribute__ ((packed)) flags;
        } ui;

     unsigned long u_intparm;     /* user interruption parameter */
     senseid_t     senseid;       /* SenseID info */
     irq_desc_t    irq_desc;      /* irq descriptor */
     not_oper_handler_func_t nopfunc; 	/* not oper handler */		
     __u8          opm;           /* path mask of operational paths */
     __u16         devno;         /* device number */
     pgid_t        pgid;          /* path group ID */
     schib_t       schib;         /* subchannel information block */
     orb_t         orb;           /* operation request block */
     devstat_t     devstat;       /* device status */
     ccw1_t        senseccw;      /* ccw for sense command */
     __u8          sense_data[32];/* buffer for basic sense */
     unsigned int  stctl;         /* accumulated status control from irb */
     ssd_info_t    ssd_info;      /* subchannel description */
     struct tq_struct pver_bh;    /* path verification bottom half task */
     atomic_t	   pver_pending;  /* != 0 if path verification for sch is pending */
} __attribute__ ((aligned(8))) ioinfo_t;

#define IOINFO_FLAGS_BUSY    0x80000000
#define IOINFO_FLAGS_OPER    0x40000000
#define IOINFO_FLAGS_FAST    0x20000000
#define IOINFO_FLAGS_READY   0x10000000
#define IOINFO_FLAGS_HALTIO  0x08000000
#define IOINFO_FLAGS_DOIO    0x04000000
#define IOINFO_FLAGS_DOIO_Q  0x02000000
#define IOINFO_FLAGS_W4FINAL 0x01000000
#define IOINFO_FLAGS_REPALL  0x00800000

extern ioinfo_t *ioinfo[];
int s390_set_private_data(int irq, void * data);
void * s390_get_private_data(int irq);

#define CHSC_SEI_ACC_CHPID        1
#define CHSC_SEI_ACC_LINKADDR     2
#define CHSC_SEI_ACC_FULLLINKADDR 3

#define CIO_PATHGONE_WAIT4INT     0x01
#define CIO_PATHGONE_IOERR        0x02
#define CIO_PATHGONE_DEVGONE      0x04

#endif  /* __s390io_h */

