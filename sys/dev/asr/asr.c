/*
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000-2001 Adaptec Corporation
 * All rights reserved.
 *
 * TERMS AND CONDITIONS OF USE
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Adaptec and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose, are disclaimed. In no
 * event shall Adaptec be liable for any direct, indirect, incidental, special,
 * exemplary or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruptions) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this driver software, even
 * if advised of the possibility of such damage.
 *
 * SCSI I2O host adapter driver
 *
 *      V1.08 2001/08/21 Mark_Salyzyn@adaptec.com
 *              - The 2000S and 2005S do not initialize on some machines,
 *		  increased timeout to 255ms from 50ms for the StatusGet
 *		  command.
 *      V1.07 2001/05/22 Mark_Salyzyn@adaptec.com
 *              - I knew this one was too good to be true. The error return
 *                on ioctl commands needs to be compared to CAM_REQ_CMP, not
 *                to the bit masked status.
 *      V1.06 2001/05/08 Mark_Salyzyn@adaptec.com
 *              - The 2005S that was supported is affectionately called the
 *                Conjoined BAR Firmware. In order to support RAID-5 in a
 *                16MB low-cost configuration, Firmware was forced to go
 *                to a Split BAR Firmware. This requires a separate IOP and
 *                Messaging base address.
 *      V1.05 2001/04/25 Mark_Salyzyn@adaptec.com
 *              - Handle support for 2005S Zero Channel RAID solution.
 *              - System locked up if the Adapter locked up. Do not try
 *                to send other commands if the resetIOP command fails. The
 *                fail outstanding command discovery loop was flawed as the
 *                removal of the command from the list prevented discovering
 *                all the commands.
 *              - Comment changes to clarify driver.
 *              - SysInfo searched for an EATA SmartROM, not an I2O SmartROM.
 *              - We do not use the AC_FOUND_DEV event because of I2O.
 *                Removed asr_async.
 *      V1.04 2000/09/22 Mark_Salyzyn@adaptec.com, msmith@freebsd.org,
 *                       lampa@fee.vutbr.cz and Scott_Long@adaptec.com.
 *              - Removed support for PM1554, PM2554 and PM2654 in Mode-0
 *                mode as this is confused with competitor adapters in run
 *                mode.
 *              - critical locking needed in ASR_ccbAdd and ASR_ccbRemove
 *                to prevent operating system panic.
 *              - moved default major number to 154 from 97.
 *      V1.03 2000/07/12 Mark_Salyzyn@adaptec.com
 *              - The controller is not actually an ASR (Adaptec SCSI RAID)
 *                series that is visible, it's more of an internal code name.
 *                remove any visible references within reason for now.
 *              - bus_ptr->LUN was not correctly zeroed when initially
 *                allocated causing a possible panic of the operating system
 *                during boot.
 *      V1.02 2000/06/26 Mark_Salyzyn@adaptec.com
 *              - Code always fails for ASR_getTid affecting performance.
 *              - initiated a set of changes that resulted from a formal
 *                code inspection by Mark_Salyzyn@adaptec.com,
 *                George_Dake@adaptec.com, Jeff_Zeak@adaptec.com,
 *                Martin_Wilson@adaptec.com and Vincent_Trandoan@adaptec.com.
 *                Their findings were focussed on the LCT & TID handler, and
 *                all resulting changes were to improve code readability,
 *                consistency or have a positive effect on performance.
 *      V1.01 2000/06/14 Mark_Salyzyn@adaptec.com
 *              - Passthrough returned an incorrect error.
 *              - Passthrough did not migrate the intrinsic scsi layer wakeup
 *                on command completion.
 *              - generate control device nodes using make_dev and delete_dev.
 *              - Performance affected by TID caching reallocing.
 *              - Made suggested changes by Justin_Gibbs@adaptec.com
 *                      - use splcam instead of splbio.
 *                      - use cam_imask instead of bio_imask.
 *                      - use u_int8_t instead of u_char.
 *                      - use u_int16_t instead of u_short.
 *                      - use u_int32_t instead of u_long where appropriate.
 *                      - use 64 bit context handler instead of 32 bit.
 *                      - create_ccb should only allocate the worst case
 *                        requirements for the driver since CAM may evolve
 *                        making union ccb much larger than needed here.
 *                        renamed create_ccb to asr_alloc_ccb.
 *                      - go nutz justifying all debug prints as macros
 *                        defined at the top and remove unsightly ifdefs.
 *                      - INLINE STATIC viewed as confusing. Historically
 *                        utilized to affect code performance and debug
 *                        issues in OS, Compiler or OEM specific situations.
 *      V1.00 2000/05/31 Mark_Salyzyn@adaptec.com
 *              - Ported from FreeBSD 2.2.X DPT I2O driver.
 *                      changed struct scsi_xfer to union ccb/struct ccb_hdr
 *                      changed variable name xs to ccb
 *                      changed struct scsi_link to struct cam_path
 *                      changed struct scsibus_data to struct cam_sim
 *                      stopped using fordriver for holding on to the TID
 *                      use proprietary packet creation instead of scsi_inquire
 *                      CAM layer sends synchronize commands.
 *
 * $FreeBSD$
 */

#define ASR_VERSION     1
#define ASR_REVISION    '0'
#define ASR_SUBREVISION '8'
#define ASR_MONTH       8
#define ASR_DAY         21
#define ASR_YEAR        2001 - 1980

/*
 *      Debug macros to reduce the unsightly ifdefs
 */
#if (defined(DEBUG_ASR) || defined(DEBUG_ASR_USR_CMD) || defined(DEBUG_ASR_CMD))
# define debug_asr_message(message)                                            \
        {                                                                      \
                u_int32_t * pointer = (u_int32_t *)message;                    \
                u_int32_t   length = I2O_MESSAGE_FRAME_getMessageSize(message);\
                u_int32_t   counter = 0;                                       \
                                                                               \
                while (length--) {                                             \
                        printf ("%08lx%c", (u_long)*(pointer++),               \
                          (((++counter & 7) == 0) || (length == 0))            \
                            ? '\n'                                             \
                            : ' ');                                            \
                }                                                              \
        }
#endif /* DEBUG_ASR || DEBUG_ASR_USR_CMD || DEBUG_ASR_CMD */

#if (defined(DEBUG_ASR))
  /* Breaks on none STDC based compilers :-( */
# define debug_asr_printf(fmt,args...)   printf(fmt, ##args)
# define debug_asr_dump_message(message) debug_asr_message(message)
# define debug_asr_print_path(ccb)       xpt_print_path(ccb->ccb_h.path);
  /* None fatal version of the ASSERT macro */
# if (defined(__STDC__))
#  define ASSERT(phrase) if(!(phrase))printf(#phrase " at line %d file %s\n",__LINE__,__FILE__)
# else
#  define ASSERT(phrase) if(!(phrase))printf("phrase" " at line %d file %s\n",__LINE__,__FILE__)
# endif
#else /* DEBUG_ASR */
# define debug_asr_printf(fmt,args...)
# define debug_asr_dump_message(message)
# define debug_asr_print_path(ccb)
# define ASSERT(x)
#endif /* DEBUG_ASR */

/*
 *      If DEBUG_ASR_CMD is defined:
 *              0 - Display incoming SCSI commands
 *              1 - add in a quick character before queueing.
 *              2 - add in outgoing message frames.
 */
#if (defined(DEBUG_ASR_CMD))
# define debug_asr_cmd_printf(fmt,args...)     printf(fmt,##args)
# define debug_asr_dump_ccb(ccb)                                      \
        {                                                             \
                u_int8_t * cp = (unsigned char *)&(ccb->csio.cdb_io); \
                int        len = ccb->csio.cdb_len;                   \
                                                                      \
                while (len) {                                         \
                        debug_asr_cmd_printf (" %02x", *(cp++));      \
                        --len;                                        \
                }                                                     \
        }
# if (DEBUG_ASR_CMD > 0)
#  define debug_asr_cmd1_printf                debug_asr_cmd_printf
# else
#  define debug_asr_cmd1_printf(fmt,args...)
# endif
# if (DEBUG_ASR_CMD > 1)
#  define debug_asr_cmd2_printf                debug_asr_cmd_printf
#  define debug_asr_cmd2_dump_message(message) debug_asr_message(message)
# else
#  define debug_asr_cmd2_printf(fmt,args...)
#  define debug_asr_cmd2_dump_message(message)
# endif
#else /* DEBUG_ASR_CMD */
# define debug_asr_cmd_printf(fmt,args...)
# define debug_asr_cmd_dump_ccb(ccb)
# define debug_asr_cmd1_printf(fmt,args...)
# define debug_asr_cmd2_printf(fmt,args...)
# define debug_asr_cmd2_dump_message(message)
#endif /* DEBUG_ASR_CMD */

#if (defined(DEBUG_ASR_USR_CMD))
# define debug_usr_cmd_printf(fmt,args...)   printf(fmt,##args)
# define debug_usr_cmd_dump_message(message) debug_usr_message(message)
#else /* DEBUG_ASR_USR_CMD */
# define debug_usr_cmd_printf(fmt,args...)
# define debug_usr_cmd_dump_message(message)
#endif /* DEBUG_ASR_USR_CMD */

#define dsDescription_size 46   /* Snug as a bug in a rug */
#include "dev/asr/dptsig.h"

static dpt_sig_S ASR_sig = {
        { 'd', 'P', 't', 'S', 'i', 'G'}, SIG_VERSION, PROC_INTEL,
        PROC_386 | PROC_486 | PROC_PENTIUM | PROC_SEXIUM, FT_HBADRVR, 0,
        OEM_DPT, OS_FREE_BSD, CAP_ABOVE16MB, DEV_ALL,
        ADF_ALL_SC5,
        0, 0, ASR_VERSION, ASR_REVISION, ASR_SUBREVISION,
        ASR_MONTH, ASR_DAY, ASR_YEAR,
/*       01234567890123456789012345678901234567890123456789     < 50 chars */
        "Adaptec FreeBSD 4.0.0 Unix SCSI I2O HBA Driver"
        /*               ^^^^^ asr_attach alters these to match OS */
};

#include <sys/param.h>  /* TRUE=1 and FALSE=0 defined here */
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/stat.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#if defined (__i386__)
#include <i386/include/cputypes.h>
#include <i386/include/vmparam.h>
#elif defined (__alpha__)
#include <alpha/include/pmap.h>
#endif

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#define STATIC static
#define INLINE

#if (defined(DEBUG_ASR) && (DEBUG_ASR > 0))
# undef STATIC
# define STATIC
# undef INLINE
# define INLINE
#endif
#define IN
#define OUT
#define INOUT

#define osdSwap4(x) ((u_long)ntohl((u_long)(x)))
#define KVTOPHYS(x) vtophys(x)
#include        "dev/asr/dptalign.h"
#include        "dev/asr/i2oexec.h"
#include        "dev/asr/i2obscsi.h"
#include        "dev/asr/i2odpt.h"
#include        "dev/asr/i2oadptr.h"
#include        "opt_asr.h"

#include        "dev/asr/sys_info.h"

/* Configuration Definitions */

#define SG_SIZE          58     /* Scatter Gather list Size              */
#define MAX_TARGET_ID    126    /* Maximum Target ID supported           */
#define MAX_LUN          255    /* Maximum LUN Supported                 */
#define MAX_CHANNEL      7      /* Maximum Channel # Supported by driver */
#define MAX_INBOUND      2000   /* Max CCBs, Also Max Queue Size         */
#define MAX_OUTBOUND     256    /* Maximum outbound frames/adapter       */
#define MAX_INBOUND_SIZE 512    /* Maximum inbound frame size            */
#define MAX_MAP          4194304L /* Maximum mapping size of IOP         */
                                /* Also serves as the minimum map for    */
                                /* the 2005S zero channel RAID product   */

/**************************************************************************
** ASR Host Adapter structure - One Structure For Each Host Adapter That **
**  Is Configured Into The System.  The Structure Supplies Configuration **
**  Information, Status Info, Queue Info And An Active CCB List Pointer. **
***************************************************************************/

/* I2O register set */
typedef struct {
        U8           Address[0x30];
        volatile U32 Status;
        volatile U32 Mask;
#               define Mask_InterruptsDisabled 0x08
        U32          x[2];
        volatile U32 ToFIFO;    /* In Bound FIFO  */
        volatile U32 FromFIFO;  /* Out Bound FIFO */
} i2oRegs_t;

/*
 * A MIX of performance and space considerations for TID lookups
 */
typedef u_int16_t tid_t;

typedef struct {
        u_int32_t size;         /* up to MAX_LUN    */
        tid_t     TID[1];
} lun2tid_t;

typedef struct {
        u_int32_t   size;       /* up to MAX_TARGET */
        lun2tid_t * LUN[1];
} target2lun_t;

/*
 *      To ensure that we only allocate and use the worst case ccb here, lets
 *      make our own local ccb union. If asr_alloc_ccb is utilized for another
 *      ccb type, ensure that you add the additional structures into our local
 *      ccb union. To ensure strict type checking, we will utilize the local
 *      ccb definition wherever possible.
 */
union asr_ccb {
        struct ccb_hdr      ccb_h;  /* For convenience */
        struct ccb_scsiio   csio;
        struct ccb_setasync csa;
};

typedef struct Asr_softc {
        u_int16_t               ha_irq;
        void                  * ha_Base;       /* base port for each board */
        u_int8_t     * volatile ha_blinkLED;
        i2oRegs_t             * ha_Virt;       /* Base address of IOP      */
        U8                    * ha_Fvirt;      /* Base address of Frames   */
        I2O_IOP_ENTRY           ha_SystemTable;
        LIST_HEAD(,ccb_hdr)     ha_ccb;        /* ccbs in use              */
        struct cam_path       * ha_path[MAX_CHANNEL+1];
        struct cam_sim        * ha_sim[MAX_CHANNEL+1];
#if __FreeBSD_version >= 400000
        struct resource       * ha_mem_res;
        struct resource       * ha_mes_res;
        struct resource       * ha_irq_res;
        void                  * ha_intr;
#endif
        PI2O_LCT                ha_LCT;        /* Complete list of devices */
#                define le_type   IdentityTag[0]
#                        define I2O_BSA     0x20
#                        define I2O_FCA     0x40
#                        define I2O_SCSI    0x00
#                        define I2O_PORT    0x80
#                        define I2O_UNKNOWN 0x7F
#                define le_bus    IdentityTag[1]
#                define le_target IdentityTag[2]
#                define le_lun    IdentityTag[3]
        target2lun_t          * ha_targets[MAX_CHANNEL+1];
        PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME ha_Msgs;
        u_long                  ha_Msgs_Phys;

        u_int8_t                ha_in_reset;
#               define HA_OPERATIONAL       0
#               define HA_IN_RESET          1
#               define HA_OFF_LINE          2
#               define HA_OFF_LINE_RECOVERY 3
        /* Configuration information */
        /* The target id maximums we take */
        u_int8_t                ha_MaxBus;     /* Maximum bus */
        u_int8_t                ha_MaxId;      /* Maximum target ID */
        u_int8_t                ha_MaxLun;     /* Maximum target LUN */
        u_int8_t                ha_SgSize;     /* Max SG elements */
        u_int8_t                ha_pciBusNum;
        u_int8_t                ha_pciDeviceNum;
        u_int8_t                ha_adapter_target[MAX_CHANNEL+1];
        u_int16_t               ha_QueueSize;  /* Max outstanding commands */
        u_int16_t               ha_Msgs_Count;

        /* Links into other parents and HBAs */
        struct Asr_softc      * ha_next;       /* HBA list */

#ifdef ASR_MEASURE_PERFORMANCE
#define MAX_TIMEQ_SIZE  256	/* assumes MAX 256 scsi commands sent */
        asr_perf_t              ha_performance;
        u_int32_t               ha_submitted_ccbs_count;

        /* Queueing macros for a circular queue */
#define TIMEQ_FREE_LIST_EMPTY(head, tail) (-1 == (head) && -1 == (tail))
#define TIMEQ_FREE_LIST_FULL(head, tail) ((((tail) + 1) % MAX_TIMEQ_SIZE) == (head))
#define ENQ_TIMEQ_FREE_LIST(item, Q, head, tail) \
        if (!TIMEQ_FREE_LIST_FULL((head), (tail))) { \
                if TIMEQ_FREE_LIST_EMPTY((head),(tail)) { \
                        (head) = (tail) = 0; \
                } \
                else (tail) = ((tail) + 1) % MAX_TIMEQ_SIZE; \
                Q[(tail)] = (item); \
        } \
        else { \
                debug_asr_printf("asr: Enqueueing when TimeQ Free List is full... This should not happen!\n"); \
        }
#define DEQ_TIMEQ_FREE_LIST(item, Q, head, tail) \
        if (!TIMEQ_FREE_LIST_EMPTY((head), (tail))) { \
                item  = Q[(head)]; \
                if ((head) == (tail)) { (head) = (tail) = -1; } \
                else (head) = ((head) + 1) % MAX_TIMEQ_SIZE; \
        } \
        else { \
                (item) = -1; \
                debug_asr_printf("asr: Dequeueing when TimeQ Free List is empty... This should not happen!\n"); \
        }

        /* Circular queue of time stamps */
        struct timeval          ha_timeQ[MAX_TIMEQ_SIZE];
        u_int32_t               ha_timeQFreeList[MAX_TIMEQ_SIZE];
        int                     ha_timeQFreeHead;
        int                     ha_timeQFreeTail;
#endif
} Asr_softc_t;

STATIC Asr_softc_t * Asr_softc;

/*
 *      Prototypes of the routines we have in this object.
 */

/* Externally callable routines */
#if __FreeBSD_version >= 400000
#define PROBE_ARGS  IN device_t tag
#define PROBE_RET   int
#define PROBE_SET() u_int32_t id = (pci_get_device(tag)<<16)|pci_get_vendor(tag)
#define PROBE_RETURN(retval) if(retval){device_set_desc(tag,retval);return(0);}else{return(ENXIO);}
#define ATTACH_ARGS IN device_t tag
#define ATTACH_RET  int
#define ATTACH_SET() int unit = device_get_unit(tag)
#define ATTACH_RETURN(retval) return(retval)
#else
#define PROBE_ARGS  IN pcici_t tag, IN pcidi_t id
#define PROBE_RET   const char *
#define PROBE_SET()
#define PROBE_RETURN(retval) return(retval)
#define ATTACH_ARGS IN pcici_t tag, IN int unit
#define ATTACH_RET  void
#define ATTACH_SET()
#define ATTACH_RETURN(retval) return
#endif
/* I2O HDM interface */
STATIC PROBE_RET      asr_probe(PROBE_ARGS);
STATIC ATTACH_RET     asr_attach(ATTACH_ARGS);
/* DOMINO placeholder */
STATIC PROBE_RET      domino_probe(PROBE_ARGS);
STATIC ATTACH_RET     domino_attach(ATTACH_ARGS);
/* MODE0 adapter placeholder */
STATIC PROBE_RET      mode0_probe(PROBE_ARGS);
STATIC ATTACH_RET     mode0_attach(ATTACH_ARGS);

STATIC Asr_softc_t  * ASR_get_sc(
                        IN dev_t dev);
STATIC int            asr_ioctl(
                        IN dev_t      dev,
                        IN u_long     cmd,
                        INOUT caddr_t data,
                        int           flag,
                        struct thread * td);
STATIC int            asr_open(
                        IN dev_t         dev,
                        int32_t          flags,
                        int32_t          ifmt,
                        IN struct thread * td);
STATIC int            asr_close(
                        dev_t         dev,
                        int           flags,
                        int           ifmt,
                        struct thread * td);
STATIC int            asr_intr(
                        IN Asr_softc_t * sc);
STATIC void           asr_timeout(
                        INOUT void * arg);
STATIC int            ASR_init(
                        IN Asr_softc_t * sc);
STATIC INLINE int     ASR_acquireLct(
                        INOUT Asr_softc_t * sc);
STATIC INLINE int     ASR_acquireHrt(
                        INOUT Asr_softc_t * sc);
STATIC void           asr_action(
                        IN struct cam_sim * sim,
                        IN union ccb      * ccb);
STATIC void           asr_poll(
                        IN struct cam_sim * sim);

/*
 *      Here is the auto-probe structure used to nest our tests appropriately
 *      during the startup phase of the operating system.
 */
#if __FreeBSD_version >= 400000
STATIC device_method_t asr_methods[] = {
        DEVMETHOD(device_probe,  asr_probe),
        DEVMETHOD(device_attach, asr_attach),
        { 0, 0 }
};

STATIC driver_t asr_driver = {
        "asr",
        asr_methods,
        sizeof(Asr_softc_t)
};

STATIC devclass_t asr_devclass;

DRIVER_MODULE(asr, pci, asr_driver, asr_devclass, 0, 0);

STATIC device_method_t domino_methods[] = {
        DEVMETHOD(device_probe,  domino_probe),
        DEVMETHOD(device_attach, domino_attach),
        { 0, 0 }
};

STATIC driver_t domino_driver = {
        "domino",
        domino_methods,
        0
};

STATIC devclass_t domino_devclass;

DRIVER_MODULE(domino, pci, domino_driver, domino_devclass, 0, 0);

STATIC device_method_t mode0_methods[] = {
        DEVMETHOD(device_probe,  mode0_probe),
        DEVMETHOD(device_attach, mode0_attach),
        { 0, 0 }
};

STATIC driver_t mode0_driver = {
        "mode0",
        mode0_methods,
        0
};

STATIC devclass_t mode0_devclass;

DRIVER_MODULE(mode0, pci, mode0_driver, mode0_devclass, 0, 0);
#else
STATIC u_long asr_pcicount = 0;
STATIC struct pci_device asr_pcidev = {
        "asr",
        asr_probe,
        asr_attach,
        &asr_pcicount,
        NULL
};
DATA_SET (asr_pciset, asr_pcidev);

STATIC u_long domino_pcicount = 0;
STATIC struct pci_device domino_pcidev = {
        "domino",
        domino_probe,
        domino_attach,
        &domino_pcicount,
        NULL
};
DATA_SET (domino_pciset, domino_pcidev);

STATIC u_long mode0_pcicount = 0;
STATIC struct pci_device mode0_pcidev = {
        "mode0",
        mode0_probe,
        mode0_attach,
        &mode0_pcicount,
        NULL
};
DATA_SET (mode0_pciset, mode0_pcidev);
#endif

/*
 * devsw for asr hba driver
 *
 * only ioctl is used. the sd driver provides all other access.
 */
#define CDEV_MAJOR 154   /* preferred default character major */
STATIC struct cdevsw asr_cdevsw = {
        asr_open,       /* open     */
        asr_close,      /* close    */
        noread,         /* read     */
        nowrite,        /* write    */
        asr_ioctl,      /* ioctl    */
        nopoll,         /* poll     */
        nommap,         /* mmap     */
        nostrategy,     /* strategy */
        "asr",  /* name     */
        CDEV_MAJOR,     /* maj      */
        nodump,         /* dump     */
        nopsize,        /* psize    */
        0,              /* flags    */
};

#ifdef ASR_MEASURE_PERFORMANCE
STATIC u_int32_t         asr_time_delta(IN struct timeval start,
                                             IN struct timeval end);
#endif

#ifdef ASR_VERY_BROKEN
/*
 * Initialize the dynamic cdevsw hooks.
 */
STATIC void
asr_drvinit (
        void * unused)
{
        static int asr_devsw_installed = 0;

        if (asr_devsw_installed) {
                return;
        }
        asr_devsw_installed++;
        /*
         * Find a free spot (the report during driver load used by
         * osd layer in engine to generate the controlling nodes).
         */
        while ((asr_cdevsw.d_maj < NUMCDEVSW)
         && (devsw(makedev(asr_cdevsw.d_maj,0)) != (struct cdevsw *)NULL)) {
                ++asr_cdevsw.d_maj;
        }
        if (asr_cdevsw.d_maj >= NUMCDEVSW) for (
          asr_cdevsw.d_maj = 0;
          (asr_cdevsw.d_maj < CDEV_MAJOR)
           && (devsw(makedev(asr_cdevsw.d_maj,0)) != (struct cdevsw *)NULL);
          ++asr_cdevsw.d_maj);
        /*
         *      Come to papa
         */
        cdevsw_add(&asr_cdevsw);
        /*
         *      delete any nodes that would attach to the primary adapter,
         * let the adapter scans add them.
         */
        destroy_dev(makedev(asr_cdevsw.d_maj,0));
} /* asr_drvinit */

/* Must initialize before CAM layer picks up our HBA driver */
SYSINIT(asrdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,asr_drvinit,NULL)
#endif

/* I2O support routines */
#define defAlignLong(STRUCT,NAME) char NAME[sizeof(STRUCT)]
#define getAlignLong(STRUCT,NAME) ((STRUCT *)(NAME))

/*
 *      Fill message with default.
 */
STATIC PI2O_MESSAGE_FRAME
ASR_fillMessage (
        IN char              * Message,
        IN u_int16_t           size)
{
        OUT PI2O_MESSAGE_FRAME Message_Ptr;

        Message_Ptr = getAlignLong(I2O_MESSAGE_FRAME, Message);
        bzero ((void *)Message_Ptr, size);
        I2O_MESSAGE_FRAME_setVersionOffset(Message_Ptr, I2O_VERSION_11);
        I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
          (size + sizeof(U32) - 1) >> 2);
        I2O_MESSAGE_FRAME_setInitiatorAddress (Message_Ptr, 1);
        return (Message_Ptr);
} /* ASR_fillMessage */

#define EMPTY_QUEUE ((U32)-1L)

STATIC INLINE U32
ASR_getMessage(
        IN i2oRegs_t * virt)
{
        OUT U32        MessageOffset;

        if ((MessageOffset = virt->ToFIFO) == EMPTY_QUEUE) {
                MessageOffset = virt->ToFIFO;
        }
        return (MessageOffset);
} /* ASR_getMessage */

/* Issue a polled command */
STATIC U32
ASR_initiateCp (
        INOUT i2oRegs_t     * virt,
        INOUT U8            * fvirt,
        IN PI2O_MESSAGE_FRAME Message)
{
        OUT U32               Mask = -1L;
        U32                   MessageOffset;
        u_int                 Delay = 1500;

        /*
         * ASR_initiateCp is only used for synchronous commands and will
         * be made more resiliant to adapter delays since commands like
         * resetIOP can cause the adapter to be deaf for a little time.
         */
        while (((MessageOffset = ASR_getMessage(virt)) == EMPTY_QUEUE)
         && (--Delay != 0)) {
                DELAY (10000);
        }
        if (MessageOffset != EMPTY_QUEUE) {
                bcopy (Message, fvirt + MessageOffset,
                  I2O_MESSAGE_FRAME_getMessageSize(Message) << 2);
                /*
                 *      Disable the Interrupts
                 */
                virt->Mask = (Mask = virt->Mask) | Mask_InterruptsDisabled;
                virt->ToFIFO = MessageOffset;
        }
        return (Mask);
} /* ASR_initiateCp */

/*
 *      Reset the adapter.
 */
STATIC U32
ASR_resetIOP (
        INOUT i2oRegs_t                * virt,
        INOUT U8                       * fvirt)
{
        struct resetMessage {
                I2O_EXEC_IOP_RESET_MESSAGE M;
                U32                        R;
        };
        defAlignLong(struct resetMessage,Message);
        PI2O_EXEC_IOP_RESET_MESSAGE      Message_Ptr;
        OUT U32               * volatile Reply_Ptr;
        U32                              Old;

        /*
         *  Build up our copy of the Message.
         */
        Message_Ptr = (PI2O_EXEC_IOP_RESET_MESSAGE)ASR_fillMessage(Message,
          sizeof(I2O_EXEC_IOP_RESET_MESSAGE));
        I2O_EXEC_IOP_RESET_MESSAGE_setFunction(Message_Ptr, I2O_EXEC_IOP_RESET);
        /*
         *  Reset the Reply Status
         */
        *(Reply_Ptr = (U32 *)((char *)Message_Ptr
          + sizeof(I2O_EXEC_IOP_RESET_MESSAGE))) = 0;
        I2O_EXEC_IOP_RESET_MESSAGE_setStatusWordLowAddress(Message_Ptr,
          KVTOPHYS((void *)Reply_Ptr));
        /*
         *      Send the Message out
         */
        if ((Old = ASR_initiateCp (virt, fvirt, (PI2O_MESSAGE_FRAME)Message_Ptr)) != (U32)-1L) {
                /*
                 *      Wait for a response (Poll), timeouts are dangerous if
                 * the card is truly responsive. We assume response in 2s.
                 */
                u_int8_t Delay = 200;

                while ((*Reply_Ptr == 0) && (--Delay != 0)) {
                        DELAY (10000);
                }
                /*
                 *      Re-enable the interrupts.
                 */
                virt->Mask = Old;
                ASSERT (*Reply_Ptr);
                return (*Reply_Ptr);
        }
        ASSERT (Old != (U32)-1L);
        return (0);
} /* ASR_resetIOP */

/*
 *      Get the curent state of the adapter
 */
STATIC INLINE PI2O_EXEC_STATUS_GET_REPLY
ASR_getStatus (
        INOUT i2oRegs_t *                        virt,
        INOUT U8 *                               fvirt,
        OUT PI2O_EXEC_STATUS_GET_REPLY           buffer)
{
        defAlignLong(I2O_EXEC_STATUS_GET_MESSAGE,Message);
        PI2O_EXEC_STATUS_GET_MESSAGE             Message_Ptr;
        U32                                      Old;

        /*
         *  Build up our copy of the Message.
         */
        Message_Ptr = (PI2O_EXEC_STATUS_GET_MESSAGE)ASR_fillMessage(Message,
          sizeof(I2O_EXEC_STATUS_GET_MESSAGE));
        I2O_EXEC_STATUS_GET_MESSAGE_setFunction(Message_Ptr,
          I2O_EXEC_STATUS_GET);
        I2O_EXEC_STATUS_GET_MESSAGE_setReplyBufferAddressLow(Message_Ptr,
          KVTOPHYS((void *)buffer));
        /* This one is a Byte Count */
        I2O_EXEC_STATUS_GET_MESSAGE_setReplyBufferLength(Message_Ptr,
          sizeof(I2O_EXEC_STATUS_GET_REPLY));
        /*
         *  Reset the Reply Status
         */
        bzero ((void *)buffer, sizeof(I2O_EXEC_STATUS_GET_REPLY));
        /*
         *      Send the Message out
         */
        if ((Old = ASR_initiateCp (virt, fvirt, (PI2O_MESSAGE_FRAME)Message_Ptr)) != (U32)-1L) {
                /*
                 *      Wait for a response (Poll), timeouts are dangerous if
                 * the card is truly responsive. We assume response in 50ms.
                 */
                u_int8_t Delay = 255;

                while (*((U8 * volatile)&(buffer->SyncByte)) == 0) {
                        if (--Delay == 0) {
                                buffer = (PI2O_EXEC_STATUS_GET_REPLY)NULL;
                                break;
                        }
                        DELAY (1000);
                }
                /*
                 *      Re-enable the interrupts.
                 */
                virt->Mask = Old;
                return (buffer);
        }
        return ((PI2O_EXEC_STATUS_GET_REPLY)NULL);
} /* ASR_getStatus */

/*
 *      Check if the device is a SCSI I2O HBA, and add it to the list.
 */

/*
 * Probe for ASR controller.  If we find it, we will use it.
 * virtual adapters.
 */
STATIC PROBE_RET
asr_probe(PROBE_ARGS)
{
        PROBE_SET();
        if ((id == 0xA5011044) || (id == 0xA5111044)) {
                PROBE_RETURN ("Adaptec Caching SCSI RAID");
        }
        PROBE_RETURN (NULL);
} /* asr_probe */

/*
 * Probe/Attach for DOMINO chipset.
 */
STATIC PROBE_RET
domino_probe(PROBE_ARGS)
{
        PROBE_SET();
        if (id == 0x10121044) {
                PROBE_RETURN ("Adaptec Caching Memory Controller");
        }
        PROBE_RETURN (NULL);
} /* domino_probe */

STATIC ATTACH_RET
domino_attach (ATTACH_ARGS)
{
        ATTACH_RETURN (0);
} /* domino_attach */

/*
 * Probe/Attach for MODE0 adapters.
 */
STATIC PROBE_RET
mode0_probe(PROBE_ARGS)
{
        PROBE_SET();

        /*
         *      If/When we can get a business case to commit to a
         * Mode0 driver here, we can make all these tests more
         * specific and robust. Mode0 adapters have their processors
         * turned off, this the chips are in a raw state.
         */

        /* This is a PLX9054 */
        if (id == 0x905410B5) {
                PROBE_RETURN ("Adaptec Mode0 PM3757");
        }
        /* This is a PLX9080 */
        if (id == 0x908010B5) {
                PROBE_RETURN ("Adaptec Mode0 PM3754/PM3755");
        }
        /* This is a ZION 80303 */
        if (id == 0x53098086) {
                PROBE_RETURN ("Adaptec Mode0 3010S");
        }
        /* This is an i960RS */
        if (id == 0x39628086) {
                PROBE_RETURN ("Adaptec Mode0 2100S");
        }
        /* This is an i960RN */
        if (id == 0x19648086) {
                PROBE_RETURN ("Adaptec Mode0 PM2865/2400A/3200S/3400S");
        }
#if 0   /* this would match any generic i960 -- mjs */
        /* This is an i960RP (typically also on Motherboards) */
        if (id == 0x19608086) {
                PROBE_RETURN ("Adaptec Mode0 PM2554/PM1554/PM2654");
        }
#endif
        PROBE_RETURN (NULL);
} /* mode0_probe */

STATIC ATTACH_RET
mode0_attach (ATTACH_ARGS)
{
        ATTACH_RETURN (0);
} /* mode0_attach */

STATIC INLINE union asr_ccb *
asr_alloc_ccb (
        IN Asr_softc_t    * sc)
{
        OUT union asr_ccb * new_ccb;

        if ((new_ccb = (union asr_ccb *)malloc(sizeof(*new_ccb),
          M_DEVBUF, M_WAITOK | M_ZERO)) != (union asr_ccb *)NULL) {
                new_ccb->ccb_h.pinfo.priority = 1;
                new_ccb->ccb_h.pinfo.index = CAM_UNQUEUED_INDEX;
                new_ccb->ccb_h.spriv_ptr0 = sc;
        }
        return (new_ccb);
} /* asr_alloc_ccb */

STATIC INLINE void
asr_free_ccb (
        IN union asr_ccb * free_ccb)
{
        free(free_ccb, M_DEVBUF);
} /* asr_free_ccb */

/*
 *      Print inquiry data `carefully'
 */
STATIC void
ASR_prstring (
        u_int8_t * s,
        int        len)
{
        while ((--len >= 0) && (*s) && (*s != ' ') && (*s != '-')) {
                printf ("%c", *(s++));
        }
} /* ASR_prstring */

/*
 * Prototypes
 */
STATIC INLINE int ASR_queue(
        IN Asr_softc_t             * sc,
        IN PI2O_MESSAGE_FRAME Message);
/*
 *      Send a message synchronously and without Interrupt to a ccb.
 */
STATIC int
ASR_queue_s (
        INOUT union asr_ccb * ccb,
        IN PI2O_MESSAGE_FRAME Message)
{
        int                   s;
        U32                   Mask;
        Asr_softc_t         * sc = (Asr_softc_t *)(ccb->ccb_h.spriv_ptr0);

        /*
         * We do not need any (optional byteswapping) method access to
         * the Initiator context field.
         */
        I2O_MESSAGE_FRAME_setInitiatorContext64(Message, (long)ccb);

        /* Prevent interrupt service */
        s = splcam ();
        sc->ha_Virt->Mask = (Mask = sc->ha_Virt->Mask)
          | Mask_InterruptsDisabled;

        if (ASR_queue (sc, Message) == EMPTY_QUEUE) {
                ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                ccb->ccb_h.status |= CAM_REQUEUE_REQ;
        }

        /*
         * Wait for this board to report a finished instruction.
         */
        while ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
                (void)asr_intr (sc);
        }

        /* Re-enable Interrupts */
        sc->ha_Virt->Mask = Mask;
        splx(s);

        return (ccb->ccb_h.status);
} /* ASR_queue_s */

/*
 *      Send a message synchronously to a Asr_softc_t
 */
STATIC int
ASR_queue_c (
        IN Asr_softc_t      * sc,
        IN PI2O_MESSAGE_FRAME Message)
{
        union asr_ccb       * ccb;
        OUT int               status;

        if ((ccb = asr_alloc_ccb (sc)) == (union asr_ccb *)NULL) {
                return (CAM_REQUEUE_REQ);
        }

        status = ASR_queue_s (ccb, Message);

        asr_free_ccb(ccb);

        return (status);
} /* ASR_queue_c */

/*
 *      Add the specified ccb to the active queue
 */
STATIC INLINE void
ASR_ccbAdd (
        IN Asr_softc_t      * sc,
        INOUT union asr_ccb * ccb)
{
        int s;

        s = splcam();
        LIST_INSERT_HEAD(&(sc->ha_ccb), &(ccb->ccb_h), sim_links.le);
        if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
                if (ccb->ccb_h.timeout == CAM_TIME_DEFAULT) {
                        /*
                         * RAID systems can take considerable time to
                         * complete some commands given the large cache
                         * flashes switching from write back to write thru.
                         */
                        ccb->ccb_h.timeout = 6 * 60 * 1000;
                }
                ccb->ccb_h.timeout_ch = timeout(asr_timeout, (caddr_t)ccb,
                  (ccb->ccb_h.timeout * hz) / 1000);
        }
        splx(s);
} /* ASR_ccbAdd */

/*
 *      Remove the specified ccb from the active queue.
 */
STATIC INLINE void
ASR_ccbRemove (
        IN Asr_softc_t      * sc,
        INOUT union asr_ccb * ccb)
{
        int s;

        s = splcam();
        untimeout(asr_timeout, (caddr_t)ccb, ccb->ccb_h.timeout_ch);
        LIST_REMOVE(&(ccb->ccb_h), sim_links.le);
        splx(s);
} /* ASR_ccbRemove */

/*
 *      Fail all the active commands, so they get re-issued by the operating
 *      system.
 */
STATIC INLINE void
ASR_failActiveCommands (
        IN Asr_softc_t                         * sc)
{
        struct ccb_hdr                         * ccb;
        int                                      s;

#if 0 /* Currently handled by callers, unnecessary paranoia currently */
      /* Left in for historical perspective. */
        defAlignLong(I2O_EXEC_LCT_NOTIFY_MESSAGE,Message);
        PI2O_EXEC_LCT_NOTIFY_MESSAGE             Message_Ptr;

        /* Send a blind LCT command to wait for the enableSys to complete */
        Message_Ptr = (PI2O_EXEC_LCT_NOTIFY_MESSAGE)ASR_fillMessage(Message,
          sizeof(I2O_EXEC_LCT_NOTIFY_MESSAGE) - sizeof(I2O_SG_ELEMENT));
        I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
          I2O_EXEC_LCT_NOTIFY);
        I2O_EXEC_LCT_NOTIFY_MESSAGE_setClassIdentifier(Message_Ptr,
          I2O_CLASS_MATCH_ANYCLASS);
        (void)ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
#endif

        s = splcam();
        /*
         *      We do not need to inform the CAM layer that we had a bus
         * reset since we manage it on our own, this also prevents the
         * SCSI_DELAY settling that would be required on other systems.
         * The `SCSI_DELAY' has already been handled by the card via the
         * acquisition of the LCT table while we are at CAM priority level.
         *  for (int bus = 0; bus <= sc->ha_MaxBus; ++bus) {
         *      xpt_async (AC_BUS_RESET, sc->ha_path[bus], NULL);
         *  }
         */
        while ((ccb = LIST_FIRST(&(sc->ha_ccb))) != (struct ccb_hdr *)NULL) {
                ASR_ccbRemove (sc, (union asr_ccb *)ccb);

                ccb->status &= ~CAM_STATUS_MASK;
                ccb->status |= CAM_REQUEUE_REQ;
                /* Nothing Transfered */
                ((struct ccb_scsiio *)ccb)->resid
                  = ((struct ccb_scsiio *)ccb)->dxfer_len;

                if (ccb->path) {
                        xpt_done ((union ccb *)ccb);
                } else {
                        wakeup ((caddr_t)ccb);
                }
        }
        splx(s);
} /* ASR_failActiveCommands */

/*
 *      The following command causes the HBA to reset the specific bus
 */
STATIC INLINE void
ASR_resetBus(
        IN Asr_softc_t                       * sc,
        IN int                                 bus)
{
        defAlignLong(I2O_HBA_BUS_RESET_MESSAGE,Message);
        I2O_HBA_BUS_RESET_MESSAGE            * Message_Ptr;
        PI2O_LCT_ENTRY                         Device;

        Message_Ptr = (I2O_HBA_BUS_RESET_MESSAGE *)ASR_fillMessage(Message,
          sizeof(I2O_HBA_BUS_RESET_MESSAGE));
        I2O_MESSAGE_FRAME_setFunction(&Message_Ptr->StdMessageFrame,
          I2O_HBA_BUS_RESET);
        for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
          (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
          ++Device) {
                if (((Device->le_type & I2O_PORT) != 0)
                 && (Device->le_bus == bus)) {
                        I2O_MESSAGE_FRAME_setTargetAddress(
                          &Message_Ptr->StdMessageFrame,
                          I2O_LCT_ENTRY_getLocalTID(Device));
                        /* Asynchronous command, with no expectations */
                        (void)ASR_queue(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
                        break;
                }
        }
} /* ASR_resetBus */

STATIC INLINE int
ASR_getBlinkLedCode (
        IN Asr_softc_t * sc)
{
        if ((sc != (Asr_softc_t *)NULL)
         && (sc->ha_blinkLED != (u_int8_t *)NULL)
         && (sc->ha_blinkLED[1] == 0xBC)) {
                return (sc->ha_blinkLED[0]);
        }
        return (0);
} /* ASR_getBlinkCode */

/*
 *      Determine the address of an TID lookup. Must be done at high priority
 *      since the address can be changed by other threads of execution.
 *
 *      Returns NULL pointer if not indexible (but will attempt to generate
 *      an index if `new_entry' flag is set to TRUE).
 *
 *      All addressible entries are to be guaranteed zero if never initialized.
 */
STATIC INLINE tid_t *
ASR_getTidAddress(
        INOUT Asr_softc_t * sc,
        IN int              bus,
        IN int              target,
        IN int              lun,
        IN int              new_entry)
{
        target2lun_t      * bus_ptr;
        lun2tid_t         * target_ptr;
        unsigned            new_size;

        /*
         *      Validity checking of incoming parameters. More of a bound
         * expansion limit than an issue with the code dealing with the
         * values.
         *
         *      sc must be valid before it gets here, so that check could be
         * dropped if speed a critical issue.
         */
        if ((sc == (Asr_softc_t *)NULL)
         || (bus > MAX_CHANNEL)
         || (target > sc->ha_MaxId)
         || (lun > sc->ha_MaxLun)) {
                debug_asr_printf("(%lx,%d,%d,%d) target out of range\n",
                  (u_long)sc, bus, target, lun);
                return ((tid_t *)NULL);
        }
        /*
         *      See if there is an associated bus list.
         *
         *      for performance, allocate in size of BUS_CHUNK chunks.
         *      BUS_CHUNK must be a power of two. This is to reduce
         *      fragmentation effects on the allocations.
         */
#       define BUS_CHUNK 8
        new_size = ((target + BUS_CHUNK - 1) & ~(BUS_CHUNK - 1));
        if ((bus_ptr = sc->ha_targets[bus]) == (target2lun_t *)NULL) {
                /*
                 *      Allocate a new structure?
                 *              Since one element in structure, the +1
                 *              needed for size has been abstracted.
                 */
                if ((new_entry == FALSE)
                 || ((sc->ha_targets[bus] = bus_ptr = (target2lun_t *)malloc (
                    sizeof(*bus_ptr) + (sizeof(bus_ptr->LUN) * new_size),
                    M_TEMP, M_WAITOK | M_ZERO))
                   == (target2lun_t *)NULL)) {
                        debug_asr_printf("failed to allocate bus list\n");
                        return ((tid_t *)NULL);
                }
                bus_ptr->size = new_size + 1;
        } else if (bus_ptr->size <= new_size) {
                target2lun_t * new_bus_ptr;

                /*
                 *      Reallocate a new structure?
                 *              Since one element in structure, the +1
                 *              needed for size has been abstracted.
                 */
                if ((new_entry == FALSE)
                 || ((new_bus_ptr = (target2lun_t *)malloc (
                    sizeof(*bus_ptr) + (sizeof(bus_ptr->LUN) * new_size),
                    M_TEMP, M_WAITOK | M_ZERO))
                   == (target2lun_t *)NULL)) {
                        debug_asr_printf("failed to reallocate bus list\n");
                        return ((tid_t *)NULL);
                }
                /*
                 *      Copy the whole thing, safer, simpler coding
                 * and not really performance critical at this point.
                 */
                bcopy (bus_ptr, new_bus_ptr, sizeof(*bus_ptr)
                  + (sizeof(bus_ptr->LUN) * (bus_ptr->size - 1)));
                sc->ha_targets[bus] = new_bus_ptr;
                free (bus_ptr, M_TEMP);
                bus_ptr = new_bus_ptr;
                bus_ptr->size = new_size + 1;
        }
        /*
         *      We now have the bus list, lets get to the target list.
         *      Since most systems have only *one* lun, we do not allocate
         *      in chunks as above, here we allow one, then in chunk sizes.
         *      TARGET_CHUNK must be a power of two. This is to reduce
         *      fragmentation effects on the allocations.
         */
#       define TARGET_CHUNK 8
        if ((new_size = lun) != 0) {
                new_size = ((lun + TARGET_CHUNK - 1) & ~(TARGET_CHUNK - 1));
        }
        if ((target_ptr = bus_ptr->LUN[target]) == (lun2tid_t *)NULL) {
                /*
                 *      Allocate a new structure?
                 *              Since one element in structure, the +1
                 *              needed for size has been abstracted.
                 */
                if ((new_entry == FALSE)
                 || ((bus_ptr->LUN[target] = target_ptr = (lun2tid_t *)malloc (
                    sizeof(*target_ptr) + (sizeof(target_ptr->TID) * new_size),
                    M_TEMP, M_WAITOK | M_ZERO))
                   == (lun2tid_t *)NULL)) {
                        debug_asr_printf("failed to allocate target list\n");
                        return ((tid_t *)NULL);
                }
                target_ptr->size = new_size + 1;
        } else if (target_ptr->size <= new_size) {
                lun2tid_t * new_target_ptr;

                /*
                 *      Reallocate a new structure?
                 *              Since one element in structure, the +1
                 *              needed for size has been abstracted.
                 */
                if ((new_entry == FALSE)
                 || ((new_target_ptr = (lun2tid_t *)malloc (
                    sizeof(*target_ptr) + (sizeof(target_ptr->TID) * new_size),
                    M_TEMP, M_WAITOK | M_ZERO))
                   == (lun2tid_t *)NULL)) {
                        debug_asr_printf("failed to reallocate target list\n");
                        return ((tid_t *)NULL);
                }
                /*
                 *      Copy the whole thing, safer, simpler coding
                 * and not really performance critical at this point.
                 */
                bcopy (target_ptr, new_target_ptr,
                  sizeof(*target_ptr)
                  + (sizeof(target_ptr->TID) * (target_ptr->size - 1)));
                bus_ptr->LUN[target] = new_target_ptr;
                free (target_ptr, M_TEMP);
                target_ptr = new_target_ptr;
                target_ptr->size = new_size + 1;
        }
        /*
         *      Now, acquire the TID address from the LUN indexed list.
         */
        return (&(target_ptr->TID[lun]));
} /* ASR_getTidAddress */

/*
 *      Get a pre-existing TID relationship.
 *
 *      If the TID was never set, return (tid_t)-1.
 *
 *      should use mutex rather than spl.
 */
STATIC INLINE tid_t
ASR_getTid (
        IN Asr_softc_t * sc,
        IN int           bus,
        IN int           target,
        IN int           lun)
{
        tid_t          * tid_ptr;
        int              s;
        OUT tid_t        retval;

        s = splcam();
        if (((tid_ptr = ASR_getTidAddress (sc, bus, target, lun, FALSE))
          == (tid_t *)NULL)
        /* (tid_t)0 or (tid_t)-1 indicate no TID */
         || (*tid_ptr == (tid_t)0)) {
                splx(s);
                return ((tid_t)-1);
        }
        retval = *tid_ptr;
        splx(s);
        return (retval);
} /* ASR_getTid */

/*
 *      Set a TID relationship.
 *
 *      If the TID was not set, return (tid_t)-1.
 *
 *      should use mutex rather than spl.
 */
STATIC INLINE tid_t
ASR_setTid (
        INOUT Asr_softc_t * sc,
        IN int              bus,
        IN int              target,
        IN int              lun,
        INOUT tid_t         TID)
{
        tid_t             * tid_ptr;
        int                 s;

        if (TID != (tid_t)-1) {
                if (TID == 0) {
                        return ((tid_t)-1);
                }
                s = splcam();
                if ((tid_ptr = ASR_getTidAddress (sc, bus, target, lun, TRUE))
                 == (tid_t *)NULL) {
                        splx(s);
                        return ((tid_t)-1);
                }
                *tid_ptr = TID;
                splx(s);
        }
        return (TID);
} /* ASR_setTid */

/*-------------------------------------------------------------------------*/
/*                    Function ASR_rescan                                  */
/*-------------------------------------------------------------------------*/
/* The Parameters Passed To This Function Are :                            */
/*     Asr_softc_t *     : HBA miniport driver's adapter data storage.     */
/*                                                                         */
/* This Function Will rescan the adapter and resynchronize any data        */
/*                                                                         */
/* Return : 0 For OK, Error Code Otherwise                                 */
/*-------------------------------------------------------------------------*/

STATIC INLINE int
ASR_rescan(
        IN Asr_softc_t * sc)
{
        int              bus;
        OUT int          error;

        /*
         * Re-acquire the LCT table and synchronize us to the adapter.
         */
        if ((error = ASR_acquireLct(sc)) == 0) {
                error = ASR_acquireHrt(sc);
        }

        if (error != 0) {
                return error;
        }

        bus = sc->ha_MaxBus;
        /* Reset all existing cached TID lookups */
        do {
                int target, event = 0;

                /*
                 *      Scan for all targets on this bus to see if they
                 * got affected by the rescan.
                 */
                for (target = 0; target <= sc->ha_MaxId; ++target) {
                        int lun;

                        /* Stay away from the controller ID */
                        if (target == sc->ha_adapter_target[bus]) {
                                continue;
                        }
                        for (lun = 0; lun <= sc->ha_MaxLun; ++lun) {
                                PI2O_LCT_ENTRY Device;
                                tid_t          TID = (tid_t)-1;
                                tid_t          LastTID;

                                /*
                                 * See if the cached TID changed. Search for
                                 * the device in our new LCT.
                                 */
                                for (Device = sc->ha_LCT->LCTEntry;
                                  Device < (PI2O_LCT_ENTRY)(((U32 *)sc->ha_LCT)
                                   + I2O_LCT_getTableSize(sc->ha_LCT));
                                  ++Device) {
                                        if ((Device->le_type != I2O_UNKNOWN)
                                         && (Device->le_bus == bus)
                                         && (Device->le_target == target)
                                         && (Device->le_lun == lun)
                                         && (I2O_LCT_ENTRY_getUserTID(Device)
                                          == 0xFFF)) {
                                                TID = I2O_LCT_ENTRY_getLocalTID(
                                                  Device);
                                                break;
                                        }
                                }
                                /*
                                 * Indicate to the OS that the label needs
                                 * to be recalculated, or that the specific
                                 * open device is no longer valid (Merde)
                                 * because the cached TID changed.
                                 */
                                LastTID = ASR_getTid (sc, bus, target, lun);
                                if (LastTID != TID) {
                                        struct cam_path * path;

                                        if (xpt_create_path(&path,
                                          /*periph*/NULL,
                                          cam_sim_path(sc->ha_sim[bus]),
                                          target, lun) != CAM_REQ_CMP) {
                                                if (TID == (tid_t)-1) {
                                                        event |= AC_LOST_DEVICE;
                                                } else {
                                                        event |= AC_INQ_CHANGED
                                                               | AC_GETDEV_CHANGED;
                                                }
                                        } else {
                                                if (TID == (tid_t)-1) {
                                                        xpt_async(
                                                          AC_LOST_DEVICE,
                                                          path, NULL);
                                                } else if (LastTID == (tid_t)-1) {
                                                        struct ccb_getdev ccb;

                                                        xpt_setup_ccb(
                                                          &(ccb.ccb_h),
                                                          path, /*priority*/5);
                                                        xpt_async(
                                                          AC_FOUND_DEVICE,
                                                          path,
                                                          &ccb);
                                                } else {
                                                        xpt_async(
                                                          AC_INQ_CHANGED,
                                                          path, NULL);
                                                        xpt_async(
                                                          AC_GETDEV_CHANGED,
                                                          path, NULL);
                                                }
                                        }
                                }
                                /*
                                 *      We have the option of clearing the
                                 * cached TID for it to be rescanned, or to
                                 * set it now even if the device never got
                                 * accessed. We chose the later since we
                                 * currently do not use the condition that
                                 * the TID ever got cached.
                                 */
                                ASR_setTid (sc, bus, target, lun, TID);
                        }
                }
                /*
                 *      The xpt layer can not handle multiple events at the
                 * same call.
                 */
                if (event & AC_LOST_DEVICE) {
                        xpt_async(AC_LOST_DEVICE, sc->ha_path[bus], NULL);
                }
                if (event & AC_INQ_CHANGED) {
                        xpt_async(AC_INQ_CHANGED, sc->ha_path[bus], NULL);
                }
                if (event & AC_GETDEV_CHANGED) {
                        xpt_async(AC_GETDEV_CHANGED, sc->ha_path[bus], NULL);
                }
        } while (--bus >= 0);
        return (error);
} /* ASR_rescan */

/*-------------------------------------------------------------------------*/
/*                    Function ASR_reset                                   */
/*-------------------------------------------------------------------------*/
/* The Parameters Passed To This Function Are :                            */
/*     Asr_softc_t *      : HBA miniport driver's adapter data storage.    */
/*                                                                         */
/* This Function Will reset the adapter and resynchronize any data         */
/*                                                                         */
/* Return : None                                                           */
/*-------------------------------------------------------------------------*/

STATIC INLINE int
ASR_reset(
        IN Asr_softc_t * sc)
{
        int              s, retVal;

        s = splcam();
        if ((sc->ha_in_reset == HA_IN_RESET)
         || (sc->ha_in_reset == HA_OFF_LINE_RECOVERY)) {
                splx (s);
                return (EBUSY);
        }
        /*
         *      Promotes HA_OPERATIONAL to HA_IN_RESET,
         * or HA_OFF_LINE to HA_OFF_LINE_RECOVERY.
         */
        ++(sc->ha_in_reset);
        if (ASR_resetIOP (sc->ha_Virt, sc->ha_Fvirt) == 0) {
                debug_asr_printf ("ASR_resetIOP failed\n");
                /*
                 *      We really need to take this card off-line, easier said
                 * than make sense. Better to keep retrying for now since if a
                 * UART cable is connected the blinkLEDs the adapter is now in
                 * a hard state requiring action from the monitor commands to
                 * the HBA to continue. For debugging waiting forever is a
                 * good thing. In a production system, however, one may wish
                 * to instead take the card off-line ...
                 */
#               if 0 && (defined(HA_OFF_LINE))
                        /*
                         * Take adapter off-line.
                         */
                        printf ("asr%d: Taking adapter off-line\n",
                          sc->ha_path[0]
                            ? cam_sim_unit(xpt_path_sim(sc->ha_path[0]))
                            : 0);
                        sc->ha_in_reset = HA_OFF_LINE;
                        splx (s);
                        return (ENXIO);
#               else
                        /* Wait Forever */
                        while (ASR_resetIOP (sc->ha_Virt, sc->ha_Fvirt) == 0);
#               endif
        }
        retVal = ASR_init (sc);
        splx (s);
        if (retVal != 0) {
                debug_asr_printf ("ASR_init failed\n");
                sc->ha_in_reset = HA_OFF_LINE;
                return (ENXIO);
        }
        if (ASR_rescan (sc) != 0) {
                debug_asr_printf ("ASR_rescan failed\n");
        }
        ASR_failActiveCommands (sc);
        if (sc->ha_in_reset == HA_OFF_LINE_RECOVERY) {
                printf ("asr%d: Brining adapter back on-line\n",
                  sc->ha_path[0]
                    ? cam_sim_unit(xpt_path_sim(sc->ha_path[0]))
                    : 0);
        }
        sc->ha_in_reset = HA_OPERATIONAL;
        return (0);
} /* ASR_reset */

/*
 *      Device timeout handler.
 */
STATIC void
asr_timeout(
        INOUT void  * arg)
{
        union asr_ccb * ccb = (union asr_ccb *)arg;
        Asr_softc_t   * sc = (Asr_softc_t *)(ccb->ccb_h.spriv_ptr0);
        int             s;

        debug_asr_print_path(ccb);
        debug_asr_printf("timed out");

        /*
         *      Check if the adapter has locked up?
         */
        if ((s = ASR_getBlinkLedCode(sc)) != 0) {
                /* Reset Adapter */
                printf ("asr%d: Blink LED 0x%x resetting adapter\n",
                  cam_sim_unit(xpt_path_sim(ccb->ccb_h.path)), s);
                if (ASR_reset (sc) == ENXIO) {
                        /* Try again later */
                        ccb->ccb_h.timeout_ch = timeout(asr_timeout,
                          (caddr_t)ccb,
                          (ccb->ccb_h.timeout * hz) / 1000);
                }
                return;
        }
        /*
         *      Abort does not function on the ASR card!!! Walking away from
         * the SCSI command is also *very* dangerous. A SCSI BUS reset is
         * our best bet, followed by a complete adapter reset if that fails.
         */
        s = splcam();
        /* Check if we already timed out once to raise the issue */
        if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_CMD_TIMEOUT) {
                debug_asr_printf (" AGAIN\nreinitializing adapter\n");
                if (ASR_reset (sc) == ENXIO) {
                        ccb->ccb_h.timeout_ch = timeout(asr_timeout,
                          (caddr_t)ccb,
                          (ccb->ccb_h.timeout * hz) / 1000);
                }
                splx(s);
                return;
        }
        debug_asr_printf ("\nresetting bus\n");
        /* If the BUS reset does not take, then an adapter reset is next! */
        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
        ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
        ccb->ccb_h.timeout_ch = timeout(asr_timeout, (caddr_t)ccb,
          (ccb->ccb_h.timeout * hz) / 1000);
        ASR_resetBus (sc, cam_sim_bus(xpt_path_sim(ccb->ccb_h.path)));
        xpt_async (AC_BUS_RESET, ccb->ccb_h.path, NULL);
        splx(s);
} /* asr_timeout */

/*
 * send a message asynchronously
 */
STATIC INLINE int
ASR_queue(
        IN Asr_softc_t      * sc,
        IN PI2O_MESSAGE_FRAME Message)
{
        OUT U32               MessageOffset;
        union asr_ccb       * ccb;

        debug_asr_printf ("Host Command Dump:\n");
        debug_asr_dump_message (Message);

        ccb = (union asr_ccb *)(long)
          I2O_MESSAGE_FRAME_getInitiatorContext64(Message);

        if ((MessageOffset = ASR_getMessage(sc->ha_Virt)) != EMPTY_QUEUE) {
#ifdef ASR_MEASURE_PERFORMANCE
                int     startTimeIndex;

                if (ccb) {
                        ++sc->ha_performance.command_count[
                          (int) ccb->csio.cdb_io.cdb_bytes[0]];
                        DEQ_TIMEQ_FREE_LIST(startTimeIndex,
                          sc->ha_timeQFreeList,
                          sc->ha_timeQFreeHead,
                          sc->ha_timeQFreeTail);
                        if (-1 != startTimeIndex) {
                                microtime(&(sc->ha_timeQ[startTimeIndex]));
                        }
                        /* Time stamp the command before we send it out */
                        ((PRIVATE_SCSI_SCB_EXECUTE_MESSAGE *) Message)->
                          PrivateMessageFrame.TransactionContext
                            = (I2O_TRANSACTION_CONTEXT) startTimeIndex;

                        ++sc->ha_submitted_ccbs_count;
                        if (sc->ha_performance.max_submit_count
                          < sc->ha_submitted_ccbs_count) {
                                sc->ha_performance.max_submit_count
                                  = sc->ha_submitted_ccbs_count;
                        }
                }
#endif
                bcopy (Message, sc->ha_Fvirt + MessageOffset,
                  I2O_MESSAGE_FRAME_getMessageSize(Message) << 2);
                if (ccb) {
                        ASR_ccbAdd (sc, ccb);
                }
                /* Post the command */
                sc->ha_Virt->ToFIFO = MessageOffset;
        } else {
                if (ASR_getBlinkLedCode(sc)) {
                        /*
                         *      Unlikely we can do anything if we can't grab a
                         * message frame :-(, but lets give it a try.
                         */
                        (void)ASR_reset (sc);
                }
        }
        return (MessageOffset);
} /* ASR_queue */


/* Simple Scatter Gather elements */
#define SG(SGL,Index,Flags,Buffer,Size)                            \
        I2O_FLAGS_COUNT_setCount(                                  \
          &(((PI2O_SG_ELEMENT)(SGL))->u.Simple[Index].FlagsCount), \
          Size);                                                   \
        I2O_FLAGS_COUNT_setFlags(                                  \
          &(((PI2O_SG_ELEMENT)(SGL))->u.Simple[Index].FlagsCount), \
          I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT | (Flags));         \
        I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(                 \
          &(((PI2O_SG_ELEMENT)(SGL))->u.Simple[Index]),            \
          (Buffer == NULL) ? NULL : KVTOPHYS(Buffer))

/*
 *      Retrieve Parameter Group.
 *              Buffer must be allocated using defAlignLong macro.
 */
STATIC void *
ASR_getParams(
        IN Asr_softc_t                     * sc,
        IN tid_t                             TID,
        IN int                               Group,
        OUT void                           * Buffer,
        IN unsigned                          BufferSize)
{
        struct paramGetMessage {
                I2O_UTIL_PARAMS_GET_MESSAGE M;
                char                         F[
                  sizeof(I2O_SGE_SIMPLE_ELEMENT)*2 - sizeof(I2O_SG_ELEMENT)];
                struct Operations {
                        I2O_PARAM_OPERATIONS_LIST_HEADER Header;
                        I2O_PARAM_OPERATION_ALL_TEMPLATE Template[1];
                }                            O;
        };
        defAlignLong(struct paramGetMessage, Message);
        struct Operations                  * Operations_Ptr;
        I2O_UTIL_PARAMS_GET_MESSAGE        * Message_Ptr;
        struct ParamBuffer {
                I2O_PARAM_RESULTS_LIST_HEADER       Header;
                I2O_PARAM_READ_OPERATION_RESULT     Read;
                char                                Info[1];
        }                                  * Buffer_Ptr;

        Message_Ptr = (I2O_UTIL_PARAMS_GET_MESSAGE *)ASR_fillMessage(Message,
          sizeof(I2O_UTIL_PARAMS_GET_MESSAGE)
            + sizeof(I2O_SGE_SIMPLE_ELEMENT)*2 - sizeof(I2O_SG_ELEMENT));
        Operations_Ptr = (struct Operations *)((char *)Message_Ptr
          + sizeof(I2O_UTIL_PARAMS_GET_MESSAGE)
          + sizeof(I2O_SGE_SIMPLE_ELEMENT)*2 - sizeof(I2O_SG_ELEMENT));
        bzero ((void *)Operations_Ptr, sizeof(struct Operations));
        I2O_PARAM_OPERATIONS_LIST_HEADER_setOperationCount(
          &(Operations_Ptr->Header), 1);
        I2O_PARAM_OPERATION_ALL_TEMPLATE_setOperation(
          &(Operations_Ptr->Template[0]), I2O_PARAMS_OPERATION_FIELD_GET);
        I2O_PARAM_OPERATION_ALL_TEMPLATE_setFieldCount(
          &(Operations_Ptr->Template[0]), 0xFFFF);
        I2O_PARAM_OPERATION_ALL_TEMPLATE_setGroupNumber(
          &(Operations_Ptr->Template[0]), Group);
        bzero ((void *)(Buffer_Ptr = getAlignLong(struct ParamBuffer, Buffer)),
          BufferSize);

        I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
          I2O_VERSION_11
          + (((sizeof(I2O_UTIL_PARAMS_GET_MESSAGE) - sizeof(I2O_SG_ELEMENT))
            / sizeof(U32)) << 4));
        I2O_MESSAGE_FRAME_setTargetAddress (&(Message_Ptr->StdMessageFrame),
          TID);
        I2O_MESSAGE_FRAME_setFunction (&(Message_Ptr->StdMessageFrame),
          I2O_UTIL_PARAMS_GET);
        /*
         *  Set up the buffers as scatter gather elements.
         */
        SG(&(Message_Ptr->SGL), 0,
          I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_END_OF_BUFFER,
          Operations_Ptr, sizeof(struct Operations));
        SG(&(Message_Ptr->SGL), 1,
          I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
          Buffer_Ptr, BufferSize);

        if ((ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr) == CAM_REQ_CMP)
         && (Buffer_Ptr->Header.ResultCount)) {
                return ((void *)(Buffer_Ptr->Info));
        }
        return ((void *)NULL);
} /* ASR_getParams */

/*
 *      Acquire the LCT information.
 */
STATIC INLINE int
ASR_acquireLct (
        INOUT Asr_softc_t          * sc)
{
        PI2O_EXEC_LCT_NOTIFY_MESSAGE Message_Ptr;
        PI2O_SGE_SIMPLE_ELEMENT      sg;
        int                          MessageSizeInBytes;
        caddr_t                      v;
        int                          len;
        I2O_LCT                      Table;
        PI2O_LCT_ENTRY               Entry;

        /*
         *      sc value assumed valid
         */
        MessageSizeInBytes = sizeof(I2O_EXEC_LCT_NOTIFY_MESSAGE)
          - sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT);
        if ((Message_Ptr = (PI2O_EXEC_LCT_NOTIFY_MESSAGE)malloc (
          MessageSizeInBytes, M_TEMP, M_WAITOK))
          == (PI2O_EXEC_LCT_NOTIFY_MESSAGE)NULL) {
                return (ENOMEM);
        }
        (void)ASR_fillMessage((char *)Message_Ptr, MessageSizeInBytes);
        I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
          (I2O_VERSION_11 +
          (((sizeof(I2O_EXEC_LCT_NOTIFY_MESSAGE) - sizeof(I2O_SG_ELEMENT))
                        / sizeof(U32)) << 4)));
        I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
          I2O_EXEC_LCT_NOTIFY);
        I2O_EXEC_LCT_NOTIFY_MESSAGE_setClassIdentifier(Message_Ptr,
          I2O_CLASS_MATCH_ANYCLASS);
        /*
         *      Call the LCT table to determine the number of device entries
         * to reserve space for.
         */
        SG(&(Message_Ptr->SGL), 0,
          I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER, &Table,
          sizeof(I2O_LCT));
        /*
         *      since this code is reused in several systems, code efficiency
         * is greater by using a shift operation rather than a divide by
         * sizeof(u_int32_t).
         */
        I2O_LCT_setTableSize(&Table,
          (sizeof(I2O_LCT) - sizeof(I2O_LCT_ENTRY)) >> 2);
        (void)ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
        /*
         *      Determine the size of the LCT table.
         */
        if (sc->ha_LCT) {
                free (sc->ha_LCT, M_TEMP);
        }
        /*
         *      malloc only generates contiguous memory when less than a
         * page is expected. We must break the request up into an SG list ...
         */
        if (((len = (I2O_LCT_getTableSize(&Table) << 2)) <=
          (sizeof(I2O_LCT) - sizeof(I2O_LCT_ENTRY)))
         || (len > (128 * 1024))) {     /* Arbitrary */
                free (Message_Ptr, M_TEMP);
                return (EINVAL);
        }
        if ((sc->ha_LCT = (PI2O_LCT)malloc (len, M_TEMP, M_WAITOK))
          == (PI2O_LCT)NULL) {
                free (Message_Ptr, M_TEMP);
                return (ENOMEM);
        }
        /*
         *      since this code is reused in several systems, code efficiency
         * is greater by using a shift operation rather than a divide by
         * sizeof(u_int32_t).
         */
        I2O_LCT_setTableSize(sc->ha_LCT,
          (sizeof(I2O_LCT) - sizeof(I2O_LCT_ENTRY)) >> 2);
        /*
         *      Convert the access to the LCT table into a SG list.
         */
        sg = Message_Ptr->SGL.u.Simple;
        v = (caddr_t)(sc->ha_LCT);
        for (;;) {
                int next, base, span;

                span = 0;
                next = base = KVTOPHYS(v);
                I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(sg, base);

                /* How far can we go contiguously */
                while ((len > 0) && (base == next)) {
                        int size;

                        next = trunc_page(base) + PAGE_SIZE;
                        size = next - base;
                        if (size > len) {
                                size = len;
                        }
                        span += size;
                        v += size;
                        len -= size;
                        base = KVTOPHYS(v);
                }

                /* Construct the Flags */
                I2O_FLAGS_COUNT_setCount(&(sg->FlagsCount), span);
                {
                        int rw = I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT;
                        if (len <= 0) {
                                rw = (I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT
                                    | I2O_SGL_FLAGS_LAST_ELEMENT
                                    | I2O_SGL_FLAGS_END_OF_BUFFER);
                        }
                        I2O_FLAGS_COUNT_setFlags(&(sg->FlagsCount), rw);
                }

                if (len <= 0) {
                        break;
                }

                /*
                 * Incrementing requires resizing of the packet.
                 */
                ++sg;
                MessageSizeInBytes += sizeof(*sg);
                I2O_MESSAGE_FRAME_setMessageSize(
                  &(Message_Ptr->StdMessageFrame),
                  I2O_MESSAGE_FRAME_getMessageSize(
                    &(Message_Ptr->StdMessageFrame))
                  + (sizeof(*sg) / sizeof(U32)));
                {
                        PI2O_EXEC_LCT_NOTIFY_MESSAGE NewMessage_Ptr;

                        if ((NewMessage_Ptr = (PI2O_EXEC_LCT_NOTIFY_MESSAGE)
                            malloc (MessageSizeInBytes, M_TEMP, M_WAITOK))
                            == (PI2O_EXEC_LCT_NOTIFY_MESSAGE)NULL) {
                                free (sc->ha_LCT, M_TEMP);
                                sc->ha_LCT = (PI2O_LCT)NULL;
                                free (Message_Ptr, M_TEMP);
                                return (ENOMEM);
                        }
                        span = ((caddr_t)sg) - (caddr_t)Message_Ptr;
                        bcopy ((caddr_t)Message_Ptr,
                          (caddr_t)NewMessage_Ptr, span);
                        free (Message_Ptr, M_TEMP);
                        sg = (PI2O_SGE_SIMPLE_ELEMENT)
                          (((caddr_t)NewMessage_Ptr) + span);
                        Message_Ptr = NewMessage_Ptr;
                }
        }
        {       int retval;

                retval = ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
                free (Message_Ptr, M_TEMP);
                if (retval != CAM_REQ_CMP) {
                        return (ENODEV);
                }
        }
        /* If the LCT table grew, lets truncate accesses */
        if (I2O_LCT_getTableSize(&Table) < I2O_LCT_getTableSize(sc->ha_LCT)) {
                I2O_LCT_setTableSize(sc->ha_LCT, I2O_LCT_getTableSize(&Table));
        }
        for (Entry = sc->ha_LCT->LCTEntry; Entry < (PI2O_LCT_ENTRY)
          (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
          ++Entry) {
                Entry->le_type = I2O_UNKNOWN;
                switch (I2O_CLASS_ID_getClass(&(Entry->ClassID))) {

                case I2O_CLASS_RANDOM_BLOCK_STORAGE:
                        Entry->le_type = I2O_BSA;
                        break;

                case I2O_CLASS_SCSI_PERIPHERAL:
                        Entry->le_type = I2O_SCSI;
                        break;

                case I2O_CLASS_FIBRE_CHANNEL_PERIPHERAL:
                        Entry->le_type = I2O_FCA;
                        break;

                case I2O_CLASS_BUS_ADAPTER_PORT:
                        Entry->le_type = I2O_PORT | I2O_SCSI;
                        /* FALLTHRU */
                case I2O_CLASS_FIBRE_CHANNEL_PORT:
                        if (I2O_CLASS_ID_getClass(&(Entry->ClassID)) ==
                          I2O_CLASS_FIBRE_CHANNEL_PORT) {
                                Entry->le_type = I2O_PORT | I2O_FCA;
                        }
                {       struct ControllerInfo {
                                I2O_PARAM_RESULTS_LIST_HEADER       Header;
                                I2O_PARAM_READ_OPERATION_RESULT     Read;
                                I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR Info;
                        };
                        defAlignLong(struct ControllerInfo, Buffer);
                        PI2O_HBA_SCSI_CONTROLLER_INFO_SCALAR Info;

                        Entry->le_bus = 0xff;
                        Entry->le_target = 0xff;
                        Entry->le_lun = 0xff;

                        if ((Info = (PI2O_HBA_SCSI_CONTROLLER_INFO_SCALAR)
                          ASR_getParams(sc,
                            I2O_LCT_ENTRY_getLocalTID(Entry),
                            I2O_HBA_SCSI_CONTROLLER_INFO_GROUP_NO,
                            Buffer, sizeof(struct ControllerInfo)))
                        == (PI2O_HBA_SCSI_CONTROLLER_INFO_SCALAR)NULL) {
                                continue;
                        }
                        Entry->le_target
                          = I2O_HBA_SCSI_CONTROLLER_INFO_SCALAR_getInitiatorID(
                            Info);
                        Entry->le_lun = 0;
                }       /* FALLTHRU */
                default:
                        continue;
                }
                {       struct DeviceInfo {
                                I2O_PARAM_RESULTS_LIST_HEADER   Header;
                                I2O_PARAM_READ_OPERATION_RESULT Read;
                                I2O_DPT_DEVICE_INFO_SCALAR      Info;
                        };
                        defAlignLong (struct DeviceInfo, Buffer);
                        PI2O_DPT_DEVICE_INFO_SCALAR      Info;

                        Entry->le_bus = 0xff;
                        Entry->le_target = 0xff;
                        Entry->le_lun = 0xff;

                        if ((Info = (PI2O_DPT_DEVICE_INFO_SCALAR)
                          ASR_getParams(sc,
                            I2O_LCT_ENTRY_getLocalTID(Entry),
                            I2O_DPT_DEVICE_INFO_GROUP_NO,
                            Buffer, sizeof(struct DeviceInfo)))
                        == (PI2O_DPT_DEVICE_INFO_SCALAR)NULL) {
                                continue;
                        }
                        Entry->le_type
                          |= I2O_DPT_DEVICE_INFO_SCALAR_getDeviceType(Info);
                        Entry->le_bus
                          = I2O_DPT_DEVICE_INFO_SCALAR_getBus(Info);
                        if ((Entry->le_bus > sc->ha_MaxBus)
                         && (Entry->le_bus <= MAX_CHANNEL)) {
                                sc->ha_MaxBus = Entry->le_bus;
                        }
                        Entry->le_target
                          = I2O_DPT_DEVICE_INFO_SCALAR_getIdentifier(Info);
                        Entry->le_lun
                          = I2O_DPT_DEVICE_INFO_SCALAR_getLunInfo(Info);
                }
        }
        /*
         *      A zero return value indicates success.
         */
        return (0);
} /* ASR_acquireLct */

/*
 * Initialize a message frame.
 * We assume that the CDB has already been set up, so all we do here is
 * generate the Scatter Gather list.
 */
STATIC INLINE PI2O_MESSAGE_FRAME
ASR_init_message(
        IN union asr_ccb      * ccb,
        OUT PI2O_MESSAGE_FRAME  Message)
{
        int                     next, span, base, rw;
        OUT PI2O_MESSAGE_FRAME  Message_Ptr;
        Asr_softc_t           * sc = (Asr_softc_t *)(ccb->ccb_h.spriv_ptr0);
        PI2O_SGE_SIMPLE_ELEMENT sg;
        caddr_t                 v;
        vm_size_t               size, len;
        U32                     MessageSize;

        /* We only need to zero out the PRIVATE_SCSI_SCB_EXECUTE_MESSAGE */
        bzero (Message_Ptr = getAlignLong(I2O_MESSAGE_FRAME, Message),
          (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE) - sizeof(I2O_SG_ELEMENT)));

        {
                int   target = ccb->ccb_h.target_id;
                int   lun = ccb->ccb_h.target_lun;
                int   bus = cam_sim_bus(xpt_path_sim(ccb->ccb_h.path));
                tid_t TID;

                if ((TID = ASR_getTid (sc, bus, target, lun)) == (tid_t)-1) {
                        PI2O_LCT_ENTRY Device;

                        TID = (tid_t)0;
                        for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
                          (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
                          ++Device) {
                                if ((Device->le_type != I2O_UNKNOWN)
                                 && (Device->le_bus == bus)
                                 && (Device->le_target == target)
                                 && (Device->le_lun == lun)
                                 && (I2O_LCT_ENTRY_getUserTID(Device) == 0xFFF)) {
                                        TID = I2O_LCT_ENTRY_getLocalTID(Device);
                                        ASR_setTid (sc, Device->le_bus,
                                          Device->le_target, Device->le_lun,
                                          TID);
                                        break;
                                }
                        }
                }
                if (TID == (tid_t)0) {
                        return ((PI2O_MESSAGE_FRAME)NULL);
                }
                I2O_MESSAGE_FRAME_setTargetAddress(Message_Ptr, TID);
                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setTID(
                  (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr, TID);
        }
        I2O_MESSAGE_FRAME_setVersionOffset(Message_Ptr, I2O_VERSION_11 |
          (((sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE) - sizeof(I2O_SG_ELEMENT))
                / sizeof(U32)) << 4));
        I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
          (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
          - sizeof(I2O_SG_ELEMENT)) / sizeof(U32));
        I2O_MESSAGE_FRAME_setInitiatorAddress (Message_Ptr, 1);
        I2O_MESSAGE_FRAME_setFunction(Message_Ptr, I2O_PRIVATE_MESSAGE);
        I2O_PRIVATE_MESSAGE_FRAME_setXFunctionCode (
          (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr, I2O_SCSI_SCB_EXEC);
        PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (
          (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr,
            I2O_SCB_FLAG_ENABLE_DISCONNECT
          | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
          | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER);
        /*
         * We do not need any (optional byteswapping) method access to
         * the Initiator & Transaction context field.
         */
        I2O_MESSAGE_FRAME_setInitiatorContext64(Message, (long)ccb);

        I2O_PRIVATE_MESSAGE_FRAME_setOrganizationID(
          (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr, DPT_ORGANIZATION_ID);
        /*
         * copy the cdb over
         */
        PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(
          (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr, ccb->csio.cdb_len);
        bcopy (&(ccb->csio.cdb_io),
          ((PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr)->CDB, ccb->csio.cdb_len);

        /*
         * Given a buffer describing a transfer, set up a scatter/gather map
         * in a ccb to map that SCSI transfer.
         */

        rw = (ccb->ccb_h.flags & CAM_DIR_IN) ? 0 : I2O_SGL_FLAGS_DIR;

        PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (
          (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr,
          (ccb->csio.dxfer_len)
            ? ((rw) ? (I2O_SCB_FLAG_XFER_TO_DEVICE
                     | I2O_SCB_FLAG_ENABLE_DISCONNECT
                     | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
                     | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER)
                    : (I2O_SCB_FLAG_XFER_FROM_DEVICE
                     | I2O_SCB_FLAG_ENABLE_DISCONNECT
                     | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
                     | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER))
            :         (I2O_SCB_FLAG_ENABLE_DISCONNECT
                     | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
                     | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER));

        /*
         * Given a transfer described by a `data', fill in the SG list.
         */
        sg = &((PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr)->SGL.u.Simple[0];

        len = ccb->csio.dxfer_len;
        v = ccb->csio.data_ptr;
        ASSERT (ccb->csio.dxfer_len >= 0);
        MessageSize = I2O_MESSAGE_FRAME_getMessageSize(Message_Ptr);
        PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setByteCount(
          (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr, len);
        while ((len > 0) && (sg < &((PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
          Message_Ptr)->SGL.u.Simple[SG_SIZE])) {
                span = 0;
                next = base = KVTOPHYS(v);
                I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(sg, base);

                /* How far can we go contiguously */
                while ((len > 0) && (base == next)) {
                        next = trunc_page(base) + PAGE_SIZE;
                        size = next - base;
                        if (size > len) {
                                size = len;
                        }
                        span += size;
                        v += size;
                        len -= size;
                        base = KVTOPHYS(v);
                }

                I2O_FLAGS_COUNT_setCount(&(sg->FlagsCount), span);
                if (len == 0) {
                        rw |= I2O_SGL_FLAGS_LAST_ELEMENT;
                }
                I2O_FLAGS_COUNT_setFlags(&(sg->FlagsCount),
                  I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT | rw);
                ++sg;
                MessageSize += sizeof(*sg) / sizeof(U32);
        }
        /* We always do the request sense ... */
        if ((span = ccb->csio.sense_len) == 0) {
                span = sizeof(ccb->csio.sense_data);
        }
        SG(sg, 0, I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
          &(ccb->csio.sense_data), span);
        I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
          MessageSize + (sizeof(*sg) / sizeof(U32)));
        return (Message_Ptr);
} /* ASR_init_message */

/*
 *      Reset the adapter.
 */
STATIC INLINE U32
ASR_initOutBound (
        INOUT Asr_softc_t                     * sc)
{
        struct initOutBoundMessage {
                I2O_EXEC_OUTBOUND_INIT_MESSAGE M;
                U32                            R;
        };
        defAlignLong(struct initOutBoundMessage,Message);
        PI2O_EXEC_OUTBOUND_INIT_MESSAGE         Message_Ptr;
        OUT U32                      * volatile Reply_Ptr;
        U32                                     Old;

        /*
         *  Build up our copy of the Message.
         */
        Message_Ptr = (PI2O_EXEC_OUTBOUND_INIT_MESSAGE)ASR_fillMessage(Message,
          sizeof(I2O_EXEC_OUTBOUND_INIT_MESSAGE));
        I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
          I2O_EXEC_OUTBOUND_INIT);
        I2O_EXEC_OUTBOUND_INIT_MESSAGE_setHostPageFrameSize(Message_Ptr, PAGE_SIZE);
        I2O_EXEC_OUTBOUND_INIT_MESSAGE_setOutboundMFrameSize(Message_Ptr,
          sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME));
        /*
         *  Reset the Reply Status
         */
        *(Reply_Ptr = (U32 *)((char *)Message_Ptr
          + sizeof(I2O_EXEC_OUTBOUND_INIT_MESSAGE))) = 0;
        SG (&(Message_Ptr->SGL), 0, I2O_SGL_FLAGS_LAST_ELEMENT, Reply_Ptr,
          sizeof(U32));
        /*
         *      Send the Message out
         */
        if ((Old = ASR_initiateCp (sc->ha_Virt, sc->ha_Fvirt, (PI2O_MESSAGE_FRAME)Message_Ptr)) != (U32)-1L) {
                u_long size, addr;

                /*
                 *      Wait for a response (Poll).
                 */
                while (*Reply_Ptr < I2O_EXEC_OUTBOUND_INIT_REJECTED);
                /*
                 *      Re-enable the interrupts.
                 */
                sc->ha_Virt->Mask = Old;
                /*
                 *      Populate the outbound table.
                 */
                if (sc->ha_Msgs == (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)NULL) {

                        /* Allocate the reply frames */
                        size = sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
                          * sc->ha_Msgs_Count;

                        /*
                         *      contigmalloc only works reliably at
                         * initialization time.
                         */
                        if ((sc->ha_Msgs = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
                          contigmalloc (size, M_DEVBUF, M_WAITOK, 0ul,
                            0xFFFFFFFFul, (u_long)sizeof(U32), 0ul))
                          != (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)NULL) {
                                (void)bzero ((char *)sc->ha_Msgs, size);
                                sc->ha_Msgs_Phys = KVTOPHYS(sc->ha_Msgs);
                        }
                }

                /* Initialize the outbound FIFO */
                if (sc->ha_Msgs != (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)NULL)
                for (size = sc->ha_Msgs_Count, addr = sc->ha_Msgs_Phys;
                  size; --size) {
                        sc->ha_Virt->FromFIFO = addr;
                        addr += sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME);
                }
                return (*Reply_Ptr);
        }
        return (0);
} /* ASR_initOutBound */

/*
 *      Set the system table
 */
STATIC INLINE int
ASR_setSysTab(
        IN Asr_softc_t              * sc)
{
        PI2O_EXEC_SYS_TAB_SET_MESSAGE Message_Ptr;
        PI2O_SET_SYSTAB_HEADER        SystemTable;
        Asr_softc_t                 * ha;
        PI2O_SGE_SIMPLE_ELEMENT       sg;
        int                           retVal;

        if ((SystemTable = (PI2O_SET_SYSTAB_HEADER)malloc (
          sizeof(I2O_SET_SYSTAB_HEADER), M_TEMP, M_WAITOK | M_ZERO))
          == (PI2O_SET_SYSTAB_HEADER)NULL) {
                return (ENOMEM);
        }
        for (ha = Asr_softc; ha; ha = ha->ha_next) {
                ++SystemTable->NumberEntries;
        }
        if ((Message_Ptr = (PI2O_EXEC_SYS_TAB_SET_MESSAGE)malloc (
          sizeof(I2O_EXEC_SYS_TAB_SET_MESSAGE) - sizeof(I2O_SG_ELEMENT)
           + ((3+SystemTable->NumberEntries) * sizeof(I2O_SGE_SIMPLE_ELEMENT)),
          M_TEMP, M_WAITOK)) == (PI2O_EXEC_SYS_TAB_SET_MESSAGE)NULL) {
                free (SystemTable, M_TEMP);
                return (ENOMEM);
        }
        (void)ASR_fillMessage((char *)Message_Ptr,
          sizeof(I2O_EXEC_SYS_TAB_SET_MESSAGE) - sizeof(I2O_SG_ELEMENT)
           + ((3+SystemTable->NumberEntries) * sizeof(I2O_SGE_SIMPLE_ELEMENT)));
        I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
          (I2O_VERSION_11 +
          (((sizeof(I2O_EXEC_SYS_TAB_SET_MESSAGE) - sizeof(I2O_SG_ELEMENT))
                        / sizeof(U32)) << 4)));
        I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
          I2O_EXEC_SYS_TAB_SET);
        /*
         *      Call the LCT table to determine the number of device entries
         * to reserve space for.
         *      since this code is reused in several systems, code efficiency
         * is greater by using a shift operation rather than a divide by
         * sizeof(u_int32_t).
         */
        sg = (PI2O_SGE_SIMPLE_ELEMENT)((char *)Message_Ptr
          + ((I2O_MESSAGE_FRAME_getVersionOffset(
              &(Message_Ptr->StdMessageFrame)) & 0xF0) >> 2));
        SG(sg, 0, I2O_SGL_FLAGS_DIR, SystemTable, sizeof(I2O_SET_SYSTAB_HEADER));
        ++sg;
        for (ha = Asr_softc; ha; ha = ha->ha_next) {
                SG(sg, 0,
                  ((ha->ha_next)
                    ? (I2O_SGL_FLAGS_DIR)
                    : (I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_END_OF_BUFFER)),
                  &(ha->ha_SystemTable), sizeof(ha->ha_SystemTable));
                ++sg;
        }
        SG(sg, 0, I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_END_OF_BUFFER, NULL, 0);
        SG(sg, 1, I2O_SGL_FLAGS_DIR | I2O_SGL_FLAGS_LAST_ELEMENT
            | I2O_SGL_FLAGS_END_OF_BUFFER, NULL, 0);
        retVal = ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);
        free (Message_Ptr, M_TEMP);
        free (SystemTable, M_TEMP);
        return (retVal);
} /* ASR_setSysTab */

STATIC INLINE int
ASR_acquireHrt (
        INOUT Asr_softc_t                   * sc)
{
        defAlignLong(I2O_EXEC_HRT_GET_MESSAGE,Message);
        I2O_EXEC_HRT_GET_MESSAGE *            Message_Ptr;
        struct {
                I2O_HRT       Header;
                I2O_HRT_ENTRY Entry[MAX_CHANNEL];
        }                                     Hrt;
        u_int8_t                              NumberOfEntries;
        PI2O_HRT_ENTRY                        Entry;

        bzero ((void *)&Hrt, sizeof (Hrt));
        Message_Ptr = (I2O_EXEC_HRT_GET_MESSAGE *)ASR_fillMessage(Message,
          sizeof(I2O_EXEC_HRT_GET_MESSAGE) - sizeof(I2O_SG_ELEMENT)
          + sizeof(I2O_SGE_SIMPLE_ELEMENT));
        I2O_MESSAGE_FRAME_setVersionOffset(&(Message_Ptr->StdMessageFrame),
          (I2O_VERSION_11
          + (((sizeof(I2O_EXEC_HRT_GET_MESSAGE) - sizeof(I2O_SG_ELEMENT))
                   / sizeof(U32)) << 4)));
        I2O_MESSAGE_FRAME_setFunction (&(Message_Ptr->StdMessageFrame),
          I2O_EXEC_HRT_GET);

        /*
         *  Set up the buffers as scatter gather elements.
         */
        SG(&(Message_Ptr->SGL), 0,
          I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
          &Hrt, sizeof(Hrt));
        if (ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr) != CAM_REQ_CMP) {
                return (ENODEV);
        }
        if ((NumberOfEntries = I2O_HRT_getNumberEntries(&Hrt.Header))
          > (MAX_CHANNEL + 1)) {
                NumberOfEntries = MAX_CHANNEL + 1;
        }
        for (Entry = Hrt.Header.HRTEntry;
          NumberOfEntries != 0;
          ++Entry, --NumberOfEntries) {
                PI2O_LCT_ENTRY Device;

                for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
                  (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
                  ++Device) {
                        if (I2O_LCT_ENTRY_getLocalTID(Device)
                          == (I2O_HRT_ENTRY_getAdapterID(Entry) & 0xFFF)) {
                                Device->le_bus = I2O_HRT_ENTRY_getAdapterID(
                                  Entry) >> 16;
                                if ((Device->le_bus > sc->ha_MaxBus)
                                 && (Device->le_bus <= MAX_CHANNEL)) {
                                        sc->ha_MaxBus = Device->le_bus;
                                }
                        }
                }
        }
        return (0);
} /* ASR_acquireHrt */

/*
 *      Enable the adapter.
 */
STATIC INLINE int
ASR_enableSys (
        IN Asr_softc_t                         * sc)
{
        defAlignLong(I2O_EXEC_SYS_ENABLE_MESSAGE,Message);
        PI2O_EXEC_SYS_ENABLE_MESSAGE             Message_Ptr;

        Message_Ptr = (PI2O_EXEC_SYS_ENABLE_MESSAGE)ASR_fillMessage(Message,
          sizeof(I2O_EXEC_SYS_ENABLE_MESSAGE));
        I2O_MESSAGE_FRAME_setFunction(&(Message_Ptr->StdMessageFrame),
          I2O_EXEC_SYS_ENABLE);
        return (ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr) != 0);
} /* ASR_enableSys */

/*
 *      Perform the stages necessary to initialize the adapter
 */
STATIC int
ASR_init(
        IN Asr_softc_t * sc)
{
        return ((ASR_initOutBound(sc) == 0)
         || (ASR_setSysTab(sc) != CAM_REQ_CMP)
         || (ASR_enableSys(sc) != CAM_REQ_CMP));
} /* ASR_init */

/*
 *      Send a Synchronize Cache command to the target device.
 */
STATIC INLINE void
ASR_sync (
        IN Asr_softc_t * sc,
        IN int           bus,
        IN int           target,
        IN int           lun)
{
        tid_t            TID;

        /*
         * We will not synchronize the device when there are outstanding
         * commands issued by the OS (this is due to a locked up device,
         * as the OS normally would flush all outstanding commands before
         * issuing a shutdown or an adapter reset).
         */
        if ((sc != (Asr_softc_t *)NULL)
         && (LIST_FIRST(&(sc->ha_ccb)) != (struct ccb_hdr *)NULL)
         && ((TID = ASR_getTid (sc, bus, target, lun)) != (tid_t)-1)
         && (TID != (tid_t)0)) {
                defAlignLong(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE,Message);
                PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE             Message_Ptr;

                bzero (Message_Ptr
                  = getAlignLong(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE, Message),
                  sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
                  - sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT));

                I2O_MESSAGE_FRAME_setVersionOffset(
                  (PI2O_MESSAGE_FRAME)Message_Ptr,
                  I2O_VERSION_11
                    | (((sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
                    - sizeof(I2O_SG_ELEMENT))
                        / sizeof(U32)) << 4));
                I2O_MESSAGE_FRAME_setMessageSize(
                  (PI2O_MESSAGE_FRAME)Message_Ptr,
                  (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
                  - sizeof(I2O_SG_ELEMENT))
                        / sizeof(U32));
                I2O_MESSAGE_FRAME_setInitiatorAddress (
                  (PI2O_MESSAGE_FRAME)Message_Ptr, 1);
                I2O_MESSAGE_FRAME_setFunction(
                  (PI2O_MESSAGE_FRAME)Message_Ptr, I2O_PRIVATE_MESSAGE);
                I2O_MESSAGE_FRAME_setTargetAddress(
                  (PI2O_MESSAGE_FRAME)Message_Ptr, TID);
                I2O_PRIVATE_MESSAGE_FRAME_setXFunctionCode (
                  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr,
                  I2O_SCSI_SCB_EXEC);
                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setTID(Message_Ptr, TID);
                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
                    I2O_SCB_FLAG_ENABLE_DISCONNECT
                  | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
                  | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER);
                I2O_PRIVATE_MESSAGE_FRAME_setOrganizationID(
                  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr,
                  DPT_ORGANIZATION_ID);
                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(Message_Ptr, 6);
                Message_Ptr->CDB[0] = SYNCHRONIZE_CACHE;
                Message_Ptr->CDB[1] = (lun << 5);

                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
                  (I2O_SCB_FLAG_XFER_FROM_DEVICE
                    | I2O_SCB_FLAG_ENABLE_DISCONNECT
                    | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
                    | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER));

                (void)ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);

        }
}

STATIC INLINE void
ASR_synchronize (
        IN Asr_softc_t * sc)
{
        int              bus, target, lun;

        for (bus = 0; bus <= sc->ha_MaxBus; ++bus) {
                for (target = 0; target <= sc->ha_MaxId; ++target) {
                        for (lun = 0; lun <= sc->ha_MaxLun; ++lun) {
                                ASR_sync(sc,bus,target,lun);
                        }
                }
        }
}

/*
 *      Reset the HBA, targets and BUS.
 *              Currently this resets *all* the SCSI busses.
 */
STATIC INLINE void
asr_hbareset(
        IN Asr_softc_t * sc)
{
        ASR_synchronize (sc);
        (void)ASR_reset (sc);
} /* asr_hbareset */

/*
 *      A reduced copy of the real pci_map_mem, incorporating the MAX_MAP
 * limit and a reduction in error checking (in the pre 4.0 case).
 */
STATIC int
asr_pci_map_mem (
#if __FreeBSD_version >= 400000
        IN device_t      tag,
#else
        IN pcici_t       tag,
#endif
        IN Asr_softc_t * sc)
{
        int              rid;
        u_int32_t        p, l, s;

#if __FreeBSD_version >= 400000
        /*
         * I2O specification says we must find first *memory* mapped BAR
         */
        for (rid = PCIR_MAPS;
          rid < (PCIR_MAPS + 4 * sizeof(u_int32_t));
          rid += sizeof(u_int32_t)) {
                p = pci_read_config(tag, rid, sizeof(p));
                if ((p & 1) == 0) {
                        break;
                }
        }
        /*
         *      Give up?
         */
        if (rid >= (PCIR_MAPS + 4 * sizeof(u_int32_t))) {
                rid = PCIR_MAPS;
        }
        p = pci_read_config(tag, rid, sizeof(p));
        pci_write_config(tag, rid, -1, sizeof(p));
        l = 0 - (pci_read_config(tag, rid, sizeof(l)) & ~15);
        pci_write_config(tag, rid, p, sizeof(p));
        if (l > MAX_MAP) {
                l = MAX_MAP;
        }
        /*
         * The 2005S Zero Channel RAID solution is not a perfect PCI
         * citizen. It asks for 4MB on BAR0, and 0MB on BAR1, once
         * enabled it rewrites the size of BAR0 to 2MB, sets BAR1 to
         * BAR0+2MB and sets it's size to 2MB. The IOP registers are
         * accessible via BAR0, the messaging registers are accessible
         * via BAR1. If the subdevice code is 50 to 59 decimal.
         */
        s = pci_read_config(tag, PCIR_DEVVENDOR, sizeof(s));
        if (s != 0xA5111044) {
                s = pci_read_config(tag, PCIR_SUBVEND_0, sizeof(s));
                if ((((ADPTDOMINATOR_SUB_ID_START ^ s) & 0xF000FFFF) == 0)
                 && (ADPTDOMINATOR_SUB_ID_START <= s)
                 && (s <= ADPTDOMINATOR_SUB_ID_END)) {
                        l = MAX_MAP; /* Conjoined BAR Raptor Daptor */
                }
        }
        p &= ~15;
        sc->ha_mem_res = bus_alloc_resource(tag, SYS_RES_MEMORY, &rid,
          p, p + l, l, RF_ACTIVE);
        if (sc->ha_mem_res == (struct resource *)NULL) {
                return (0);
        }
        sc->ha_Base = (void *)rman_get_start(sc->ha_mem_res);
        if (sc->ha_Base == (void *)NULL) {
                return (0);
        }
        sc->ha_Virt = (i2oRegs_t *) rman_get_virtual(sc->ha_mem_res);
        if (s == 0xA5111044) { /* Split BAR Raptor Daptor */
                if ((rid += sizeof(u_int32_t))
                  >= (PCIR_MAPS + 4 * sizeof(u_int32_t))) {
                        return (0);
                }
                p = pci_read_config(tag, rid, sizeof(p));
                pci_write_config(tag, rid, -1, sizeof(p));
                l = 0 - (pci_read_config(tag, rid, sizeof(l)) & ~15);
                pci_write_config(tag, rid, p, sizeof(p));
                if (l > MAX_MAP) {
                        l = MAX_MAP;
                }
                p &= ~15;
                sc->ha_mes_res = bus_alloc_resource(tag, SYS_RES_MEMORY, &rid,
                  p, p + l, l, RF_ACTIVE);
                if (sc->ha_mes_res == (struct resource *)NULL) {
                        return (0);
                }
                if ((void *)rman_get_start(sc->ha_mes_res) == (void *)NULL) {
                        return (0);
                }
                sc->ha_Fvirt = (U8 *) rman_get_virtual(sc->ha_mes_res);
        } else {
                sc->ha_Fvirt = (U8 *)(sc->ha_Virt);
        }
#else
        vm_size_t psize, poffs;

        /*
         * I2O specification says we must find first *memory* mapped BAR
         */
        for (rid = PCI_MAP_REG_START;
          rid < (PCI_MAP_REG_START + 4 * sizeof(u_int32_t));
          rid += sizeof(u_int32_t)) {
                p = pci_conf_read (tag, rid);
                if ((p & 1) == 0) {
                        break;
                }
        }
        if (rid >= (PCI_MAP_REG_START + 4 * sizeof(u_int32_t))) {
                rid = PCI_MAP_REG_START;
        }
        /*
        **      save old mapping, get size and type of memory
        **
        **      type is in the lowest four bits.
        **      If device requires 2^n bytes, the next
        **      n-4 bits are read as 0.
        */

        sc->ha_Base = (void *)((p = pci_conf_read (tag, rid))
          & PCI_MAP_MEMORY_ADDRESS_MASK);
        pci_conf_write (tag, rid, 0xfffffffful);
        l = pci_conf_read (tag, rid);
        pci_conf_write (tag, rid, p);

        /*
        **      check the type
        */

        if (!((l & PCI_MAP_MEMORY_TYPE_MASK) == PCI_MAP_MEMORY_TYPE_32BIT_1M
           && ((u_long)sc->ha_Base & ~0xfffff) == 0)
          && ((l & PCI_MAP_MEMORY_TYPE_MASK) != PCI_MAP_MEMORY_TYPE_32BIT)) {
                debug_asr_printf (
                  "asr_pci_map_mem failed: bad memory type=0x%x\n",
                  (unsigned) l);
                return (0);
        };

        /*
        **      get the size.
        */

        psize = -(l & PCI_MAP_MEMORY_ADDRESS_MASK);
        if (psize > MAX_MAP) {
                psize = MAX_MAP;
        }
        /*
         * The 2005S Zero Channel RAID solution is not a perfect PCI
         * citizen. It asks for 4MB on BAR0, and 0MB on BAR1, once
         * enabled it rewrites the size of BAR0 to 2MB, sets BAR1 to
         * BAR0+2MB and sets it's size to 2MB. The IOP registers are
         * accessible via BAR0, the messaging registers are accessible
         * via BAR1. If the subdevice code is 50 to 59 decimal.
         */
        s = pci_read_config(tag, PCIR_DEVVENDOR, sizeof(s));
        if (s != 0xA5111044) {
                s = pci_conf_read (tag, PCIR_SUBVEND_0)
                if ((((ADPTDOMINATOR_SUB_ID_START ^ s) & 0xF000FFFF) == 0)
                 && (ADPTDOMINATOR_SUB_ID_START <= s)
                 && (s <= ADPTDOMINATOR_SUB_ID_END)) {
                        psize = MAX_MAP;
                }
        }

        if ((sc->ha_Base == (void *)NULL)
         || (sc->ha_Base == (void *)PCI_MAP_MEMORY_ADDRESS_MASK)) {
                debug_asr_printf ("asr_pci_map_mem: not configured by bios.\n");
                return (0);
        };

        /*
        **      Truncate sc->ha_Base to page boundary.
        **      (Or does pmap_mapdev the job?)
        */

        poffs = (u_long)sc->ha_Base - trunc_page ((u_long)sc->ha_Base);
        sc->ha_Virt = (i2oRegs_t *)pmap_mapdev ((u_long)sc->ha_Base - poffs,
          psize + poffs);

        if (sc->ha_Virt == (i2oRegs_t *)NULL) {
                return (0);
        }

        sc->ha_Virt = (i2oRegs_t *)((u_long)sc->ha_Virt + poffs);
        if (s == 0xA5111044) {
                if ((rid += sizeof(u_int32_t))
                  >= (PCI_MAP_REG_START + 4 * sizeof(u_int32_t))) {
                        return (0);
                }

                /*
                **      save old mapping, get size and type of memory
                **
                **      type is in the lowest four bits.
                **      If device requires 2^n bytes, the next
                **      n-4 bits are read as 0.
                */

                if ((((p = pci_conf_read (tag, rid))
                  & PCI_MAP_MEMORY_ADDRESS_MASK) == 0L)
                 || ((p & PCI_MAP_MEMORY_ADDRESS_MASK)
                  == PCI_MAP_MEMORY_ADDRESS_MASK)) {
                        debug_asr_printf ("asr_pci_map_mem: not configured by bios.\n");
                }
                pci_conf_write (tag, rid, 0xfffffffful);
                l = pci_conf_read (tag, rid);
                pci_conf_write (tag, rid, p);
                p &= PCI_MAP_MEMORY_TYPE_MASK;

                /*
                **      check the type
                */

                if (!((l & PCI_MAP_MEMORY_TYPE_MASK)
                    == PCI_MAP_MEMORY_TYPE_32BIT_1M
                   && (p & ~0xfffff) == 0)
                  && ((l & PCI_MAP_MEMORY_TYPE_MASK)
                   != PCI_MAP_MEMORY_TYPE_32BIT)) {
                        debug_asr_printf (
                          "asr_pci_map_mem failed: bad memory type=0x%x\n",
                          (unsigned) l);
                        return (0);
                };

                /*
                **      get the size.
                */

                psize = -(l & PCI_MAP_MEMORY_ADDRESS_MASK);
                if (psize > MAX_MAP) {
                        psize = MAX_MAP;
                }

                /*
                **      Truncate p to page boundary.
                **      (Or does pmap_mapdev the job?)
                */

                poffs = p - trunc_page (p);
                sc->ha_Fvirt = (U8 *)pmap_mapdev (p - poffs, psize + poffs);

                if (sc->ha_Fvirt == (U8 *)NULL) {
                        return (0);
                }

                sc->ha_Fvirt = (U8 *)((u_long)sc->ha_Fvirt + poffs);
        } else {
                sc->ha_Fvirt = (U8 *)(sc->ha_Virt);
        }
#endif
        return (1);
} /* asr_pci_map_mem */

/*
 *      A simplified copy of the real pci_map_int with additional
 * registration requirements.
 */
STATIC int
asr_pci_map_int (
#if __FreeBSD_version >= 400000
        IN device_t      tag,
#else
        IN pcici_t       tag,
#endif
        IN Asr_softc_t * sc)
{
#if __FreeBSD_version >= 400000
        int              rid = 0;

        sc->ha_irq_res = bus_alloc_resource(tag, SYS_RES_IRQ, &rid,
          0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
        if (sc->ha_irq_res == (struct resource *)NULL) {
                return (0);
        }
        if (bus_setup_intr(tag, sc->ha_irq_res, INTR_TYPE_CAM | INTR_ENTROPY,
          (driver_intr_t *)asr_intr, (void *)sc, &(sc->ha_intr))) {
                return (0);
        }
        sc->ha_irq = pci_read_config(tag, PCIR_INTLINE, sizeof(char));
#else
        if (!pci_map_int(tag, (pci_inthand_t *)asr_intr,
          (void *)sc, &cam_imask)) {
                return (0);
        }
        sc->ha_irq = pci_conf_read(tag, PCIR_INTLINE);
#endif
        return (1);
} /* asr_pci_map_int */

/*
 *      Attach the devices, and virtual devices to the driver list.
 */
STATIC ATTACH_RET
asr_attach (ATTACH_ARGS)
{
        Asr_softc_t              * sc;
        struct scsi_inquiry_data * iq;
        ATTACH_SET();

        if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO)) ==
		(Asr_softc_t *)NULL)
        {
                ATTACH_RETURN(ENOMEM);
        }
        if (Asr_softc == (Asr_softc_t *)NULL) {
                /*
                 *      Fixup the OS revision as saved in the dptsig for the
                 *      engine (dptioctl.h) to pick up.
                 */
                bcopy (osrelease, &ASR_sig.dsDescription[16], 5);
                printf ("asr%d: major=%d\n", unit, asr_cdevsw.d_maj);
        }
        /*
         *      Initialize the software structure
         */
        LIST_INIT(&(sc->ha_ccb));
#       ifdef ASR_MEASURE_PERFORMANCE
                {
                        u_int32_t i;

                        /* initialize free list for timeQ */
                        sc->ha_timeQFreeHead = 0;
                        sc->ha_timeQFreeTail = MAX_TIMEQ_SIZE - 1;
                        for (i = 0; i < MAX_TIMEQ_SIZE; i++) {
                                sc->ha_timeQFreeList[i] = i;
                        }
                }
#       endif
        /* Link us into the HA list */
        {
                Asr_softc_t **ha;

                for (ha = &Asr_softc; *ha; ha = &((*ha)->ha_next));
                *(ha) = sc;
        }
        {
                PI2O_EXEC_STATUS_GET_REPLY status;
                int size;

                /*
                 *      This is the real McCoy!
                 */
                if (!asr_pci_map_mem(tag, sc)) {
                        printf ("asr%d: could not map memory\n", unit);
                        ATTACH_RETURN(ENXIO);
                }
                /* Enable if not formerly enabled */
#if __FreeBSD_version >= 400000
                pci_write_config (tag, PCIR_COMMAND,
                  pci_read_config (tag, PCIR_COMMAND, sizeof(char))
                  | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN, sizeof(char));
                /* Knowledge is power, responsibility is direct */
                {
                        struct pci_devinfo {
                                STAILQ_ENTRY(pci_devinfo) pci_links;
                                struct resource_list      resources;
                                pcicfgregs                cfg;
                        } * dinfo = device_get_ivars(tag);
                        sc->ha_pciBusNum = dinfo->cfg.bus;
                        sc->ha_pciDeviceNum = (dinfo->cfg.slot << 3)
                                            | dinfo->cfg.func;
                }
#else
                pci_conf_write (tag, PCIR_COMMAND,
                  pci_conf_read (tag, PCIR_COMMAND)
                  | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
                /* Knowledge is power, responsibility is direct */
                switch (pci_mechanism) {

                case 1:
                        sc->ha_pciBusNum = tag.cfg1 >> 16;
                        sc->ha_pciDeviceNum = tag.cfg1 >> 8;

                case 2:
                        sc->ha_pciBusNum = tag.cfg2.forward;
                        sc->ha_pciDeviceNum = ((tag.cfg2.enable >> 1) & 7)
                                            | (tag.cfg2.port >> 5);
                }
#endif
                /* Check if the device is there? */
                if ((ASR_resetIOP(sc->ha_Virt, sc->ha_Fvirt) == 0)
                 || ((status = (PI2O_EXEC_STATUS_GET_REPLY)malloc (
                  sizeof(I2O_EXEC_STATUS_GET_REPLY), M_TEMP, M_WAITOK))
                  == (PI2O_EXEC_STATUS_GET_REPLY)NULL)
                 || (ASR_getStatus(sc->ha_Virt, sc->ha_Fvirt, status) == NULL)) {
                        printf ("asr%d: could not initialize hardware\n", unit);
                        ATTACH_RETURN(ENODEV);  /* Get next, maybe better luck */
                }
                sc->ha_SystemTable.OrganizationID = status->OrganizationID;
                sc->ha_SystemTable.IOP_ID = status->IOP_ID;
                sc->ha_SystemTable.I2oVersion = status->I2oVersion;
                sc->ha_SystemTable.IopState = status->IopState;
                sc->ha_SystemTable.MessengerType = status->MessengerType;
                sc->ha_SystemTable.InboundMessageFrameSize
                  = status->InboundMFrameSize;
                sc->ha_SystemTable.MessengerInfo.InboundMessagePortAddressLow
                  = (U32)(sc->ha_Base) + (U32)(&(((i2oRegs_t *)NULL)->ToFIFO));

                if (!asr_pci_map_int(tag, (void *)sc)) {
                        printf ("asr%d: could not map interrupt\n", unit);
                        ATTACH_RETURN(ENXIO);
                }

                /* Adjust the maximim inbound count */
                if (((sc->ha_QueueSize
                  = I2O_EXEC_STATUS_GET_REPLY_getMaxInboundMFrames(status))
                     > MAX_INBOUND)
                 || (sc->ha_QueueSize == 0)) {
                        sc->ha_QueueSize = MAX_INBOUND;
                }

                /* Adjust the maximum outbound count */
                if (((sc->ha_Msgs_Count
                  = I2O_EXEC_STATUS_GET_REPLY_getMaxOutboundMFrames(status))
                     > MAX_OUTBOUND)
                 || (sc->ha_Msgs_Count == 0)) {
                        sc->ha_Msgs_Count = MAX_OUTBOUND;
                }
                if (sc->ha_Msgs_Count > sc->ha_QueueSize) {
                        sc->ha_Msgs_Count = sc->ha_QueueSize;
                }

                /* Adjust the maximum SG size to adapter */
                if ((size = (I2O_EXEC_STATUS_GET_REPLY_getInboundMFrameSize(
                  status) << 2)) > MAX_INBOUND_SIZE) {
                        size = MAX_INBOUND_SIZE;
                }
                free (status, M_TEMP);
                sc->ha_SgSize = (size - sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
                  + sizeof(I2O_SG_ELEMENT)) / sizeof(I2O_SGE_SIMPLE_ELEMENT);
        }

        /*
         *      Only do a bus/HBA reset on the first time through. On this
         * first time through, we do not send a flush to the devices.
         */
        if (ASR_init(sc) == 0) {
                struct BufferInfo {
                        I2O_PARAM_RESULTS_LIST_HEADER       Header;
                        I2O_PARAM_READ_OPERATION_RESULT     Read;
                        I2O_DPT_EXEC_IOP_BUFFERS_SCALAR     Info;
                };
                defAlignLong (struct BufferInfo, Buffer);
                PI2O_DPT_EXEC_IOP_BUFFERS_SCALAR Info;
#                       define FW_DEBUG_BLED_OFFSET 8

                if ((Info = (PI2O_DPT_EXEC_IOP_BUFFERS_SCALAR)
                  ASR_getParams(sc, 0,
                    I2O_DPT_EXEC_IOP_BUFFERS_GROUP_NO,
                    Buffer, sizeof(struct BufferInfo)))
                != (PI2O_DPT_EXEC_IOP_BUFFERS_SCALAR)NULL) {
                        sc->ha_blinkLED = sc->ha_Fvirt
                          + I2O_DPT_EXEC_IOP_BUFFERS_SCALAR_getSerialOutputOffset(Info)
                          + FW_DEBUG_BLED_OFFSET;
                }
                if (ASR_acquireLct(sc) == 0) {
                        (void)ASR_acquireHrt(sc);
                }
        } else {
                printf ("asr%d: failed to initialize\n", unit);
                ATTACH_RETURN(ENXIO);
        }
        /*
         *      Add in additional probe responses for more channels. We
         * are reusing the variable `target' for a channel loop counter.
         * Done here because of we need both the acquireLct and
         * acquireHrt data.
         */
        {       PI2O_LCT_ENTRY Device;

                for (Device = sc->ha_LCT->LCTEntry; Device < (PI2O_LCT_ENTRY)
                  (((U32 *)sc->ha_LCT)+I2O_LCT_getTableSize(sc->ha_LCT));
                  ++Device) {
                        if (Device->le_type == I2O_UNKNOWN) {
                                continue;
                        }
                        if (I2O_LCT_ENTRY_getUserTID(Device) == 0xFFF) {
                                if (Device->le_target > sc->ha_MaxId) {
                                        sc->ha_MaxId = Device->le_target;
                                }
                                if (Device->le_lun > sc->ha_MaxLun) {
                                        sc->ha_MaxLun = Device->le_lun;
                                }
                        }
                        if (((Device->le_type & I2O_PORT) != 0)
                         && (Device->le_bus <= MAX_CHANNEL)) {
                                /* Do not increase MaxId for efficiency */
                                sc->ha_adapter_target[Device->le_bus]
                                        = Device->le_target;
                        }
                }
        }


        /*
         *      Print the HBA model number as inquired from the card.
         */

        printf ("asr%d:", unit);

        if ((iq = (struct scsi_inquiry_data *)malloc (
            sizeof(struct scsi_inquiry_data), M_TEMP, M_WAITOK | M_ZERO))
          != (struct scsi_inquiry_data *)NULL) {
                defAlignLong(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE,Message);
                PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE             Message_Ptr;
                int                                           posted = 0;

                bzero (Message_Ptr
                  = getAlignLong(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE, Message),
                  sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
                  - sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT));

                I2O_MESSAGE_FRAME_setVersionOffset(
                  (PI2O_MESSAGE_FRAME)Message_Ptr,
                  I2O_VERSION_11
                    | (((sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
                    - sizeof(I2O_SG_ELEMENT))
                        / sizeof(U32)) << 4));
                I2O_MESSAGE_FRAME_setMessageSize(
                  (PI2O_MESSAGE_FRAME)Message_Ptr,
                  (sizeof(PRIVATE_SCSI_SCB_EXECUTE_MESSAGE)
                  - sizeof(I2O_SG_ELEMENT) + sizeof(I2O_SGE_SIMPLE_ELEMENT))
                        / sizeof(U32));
                I2O_MESSAGE_FRAME_setInitiatorAddress (
                  (PI2O_MESSAGE_FRAME)Message_Ptr, 1);
                I2O_MESSAGE_FRAME_setFunction(
                  (PI2O_MESSAGE_FRAME)Message_Ptr, I2O_PRIVATE_MESSAGE);
                I2O_PRIVATE_MESSAGE_FRAME_setXFunctionCode (
                  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr,
                  I2O_SCSI_SCB_EXEC);
                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
                    I2O_SCB_FLAG_ENABLE_DISCONNECT
                  | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
                  | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER);
                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setInterpret(Message_Ptr, 1);
                I2O_PRIVATE_MESSAGE_FRAME_setOrganizationID(
                  (PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr,
                  DPT_ORGANIZATION_ID);
                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setCDBLength(Message_Ptr, 6);
                Message_Ptr->CDB[0] = INQUIRY;
                Message_Ptr->CDB[4] = (unsigned char)sizeof(struct scsi_inquiry_data);
                if (Message_Ptr->CDB[4] == 0) {
                        Message_Ptr->CDB[4] = 255;
                }

                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setSCBFlags (Message_Ptr,
                  (I2O_SCB_FLAG_XFER_FROM_DEVICE
                    | I2O_SCB_FLAG_ENABLE_DISCONNECT
                    | I2O_SCB_FLAG_SIMPLE_QUEUE_TAG
                    | I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER));

                PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_setByteCount(
                  (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr,
                  sizeof(struct scsi_inquiry_data));
                SG(&(Message_Ptr->SGL), 0,
                  I2O_SGL_FLAGS_LAST_ELEMENT | I2O_SGL_FLAGS_END_OF_BUFFER,
                  iq, sizeof(struct scsi_inquiry_data));
                (void)ASR_queue_c(sc, (PI2O_MESSAGE_FRAME)Message_Ptr);

                if (iq->vendor[0] && (iq->vendor[0] != ' ')) {
                        printf (" ");
                        ASR_prstring (iq->vendor, 8);
                        ++posted;
                }
                if (iq->product[0] && (iq->product[0] != ' ')) {
                        printf (" ");
                        ASR_prstring (iq->product, 16);
                        ++posted;
                }
                if (iq->revision[0] && (iq->revision[0] != ' ')) {
                        printf (" FW Rev. ");
                        ASR_prstring (iq->revision, 4);
                        ++posted;
                }
                free ((caddr_t)iq, M_TEMP);
                if (posted) {
                        printf (",");
                }
        }
        printf (" %d channel, %d CCBs, Protocol I2O\n", sc->ha_MaxBus + 1,
          (sc->ha_QueueSize > MAX_INBOUND) ? MAX_INBOUND : sc->ha_QueueSize);

        /*
         * fill in the prototype cam_path.
         */
        {
                int             bus;
                union asr_ccb * ccb;

                if ((ccb = asr_alloc_ccb (sc)) == (union asr_ccb *)NULL) {
                        printf ("asr%d: CAM could not be notified of asynchronous callback parameters\n", unit);
                        ATTACH_RETURN(ENOMEM);
                }
                for (bus = 0; bus <= sc->ha_MaxBus; ++bus) {
                        struct cam_devq   * devq;
                        int                 QueueSize = sc->ha_QueueSize;

                        if (QueueSize > MAX_INBOUND) {
                                QueueSize = MAX_INBOUND;
                        }

                        /*
                         *      Create the device queue for our SIM(s).
                         */
                        if ((devq = cam_simq_alloc(QueueSize)) == NULL) {
                                continue;
                        }

                        /*
                         *      Construct our first channel SIM entry
                         */
                        sc->ha_sim[bus] = cam_sim_alloc(
                          asr_action, asr_poll, "asr", sc,
                          unit, 1, QueueSize, devq);
                        if (sc->ha_sim[bus] == NULL) {
                                continue;
                        }

                        if (xpt_bus_register(sc->ha_sim[bus], bus)
                          != CAM_SUCCESS) {
                                cam_sim_free(sc->ha_sim[bus],
                                  /*free_devq*/TRUE);
                                sc->ha_sim[bus] = NULL;
                                continue;
                        }

                        if (xpt_create_path(&(sc->ha_path[bus]), /*periph*/NULL,
                          cam_sim_path(sc->ha_sim[bus]), CAM_TARGET_WILDCARD,
                          CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
                                xpt_bus_deregister(
                                  cam_sim_path(sc->ha_sim[bus]));
                                cam_sim_free(sc->ha_sim[bus],
                                  /*free_devq*/TRUE);
                                sc->ha_sim[bus] = NULL;
                                continue;
                        }
                }
                asr_free_ccb (ccb);
        }
        /*
         *      Generate the device node information
         */
        (void)make_dev(&asr_cdevsw, unit, 0, 0, S_IRWXU, "rasr%d", unit);
        destroy_dev(makedev(asr_cdevsw.d_maj,unit+1));
        ATTACH_RETURN(0);
} /* asr_attach */

STATIC void
asr_poll(
        IN struct cam_sim *sim)
{
        asr_intr(cam_sim_softc(sim));
} /* asr_poll */

STATIC void
asr_action(
        IN struct cam_sim * sim,
        IN union ccb      * ccb)
{
        struct Asr_softc  * sc;

        debug_asr_printf ("asr_action(%lx,%lx{%x})\n",
          (u_long)sim, (u_long)ccb, ccb->ccb_h.func_code);

        CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("asr_action\n"));

        ccb->ccb_h.spriv_ptr0 = sc = (struct Asr_softc *)cam_sim_softc(sim);

        switch (ccb->ccb_h.func_code) {

        /* Common cases first */
        case XPT_SCSI_IO:       /* Execute the requested I/O operation */
        {
                struct Message {
                        char M[MAX_INBOUND_SIZE];
                };
                defAlignLong(struct Message,Message);
                PI2O_MESSAGE_FRAME   Message_Ptr;

                /* Reject incoming commands while we are resetting the card */
                if (sc->ha_in_reset != HA_OPERATIONAL) {
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        if (sc->ha_in_reset >= HA_OFF_LINE) {
                                /* HBA is now off-line */
                                ccb->ccb_h.status |= CAM_UNREC_HBA_ERROR;
                        } else {
                                /* HBA currently resetting, try again later. */
                                ccb->ccb_h.status |= CAM_REQUEUE_REQ;
                        }
                        debug_asr_cmd_printf (" e\n");
                        xpt_done(ccb);
                        debug_asr_cmd_printf (" q\n");
                        break;
                }
                if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
                        printf(
                          "asr%d WARNING: scsi_cmd(%x) already done on b%dt%du%d\n",
                          cam_sim_unit(xpt_path_sim(ccb->ccb_h.path)),
                          ccb->csio.cdb_io.cdb_bytes[0],
                          cam_sim_bus(sim),
                          ccb->ccb_h.target_id,
                          ccb->ccb_h.target_lun);
                }
                debug_asr_cmd_printf ("(%d,%d,%d,%d)",
                  cam_sim_unit(sim),
                  cam_sim_bus(sim),
                  ccb->ccb_h.target_id,
                  ccb->ccb_h.target_lun);
                debug_asr_cmd_dump_ccb(ccb);

                if ((Message_Ptr = ASR_init_message ((union asr_ccb *)ccb,
                  (PI2O_MESSAGE_FRAME)Message)) != (PI2O_MESSAGE_FRAME)NULL) {
                        debug_asr_cmd2_printf ("TID=%x:\n",
                          PRIVATE_SCSI_SCB_EXECUTE_MESSAGE_getTID(
                            (PPRIVATE_SCSI_SCB_EXECUTE_MESSAGE)Message_Ptr));
                        debug_asr_cmd2_dump_message(Message_Ptr);
                        debug_asr_cmd1_printf (" q");

                        if (ASR_queue (sc, Message_Ptr) == EMPTY_QUEUE) {
#ifdef ASR_MEASURE_PERFORMANCE
                                ++sc->ha_performance.command_too_busy;
#endif
                                ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                                ccb->ccb_h.status |= CAM_REQUEUE_REQ;
                                debug_asr_cmd_printf (" E\n");
                                xpt_done(ccb);
                        }
                        debug_asr_cmd_printf (" Q\n");
                        break;
                }
                /*
                 *      We will get here if there is no valid TID for the device
                 * referenced in the scsi command packet.
                 */
                ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                ccb->ccb_h.status |= CAM_SEL_TIMEOUT;
                debug_asr_cmd_printf (" B\n");
                xpt_done(ccb);
                break;
        }

        case XPT_RESET_DEV:     /* Bus Device Reset the specified SCSI device */
                /* Rese HBA device ... */
                asr_hbareset (sc);
                ccb->ccb_h.status = CAM_REQ_CMP;
                xpt_done(ccb);
                break;

#       if (defined(REPORT_LUNS))
        case REPORT_LUNS:
#       endif
        case XPT_ABORT:                 /* Abort the specified CCB */
                /* XXX Implement */
                ccb->ccb_h.status = CAM_REQ_INVALID;
                xpt_done(ccb);
                break;

        case XPT_SET_TRAN_SETTINGS:
                /* XXX Implement */
                ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
                xpt_done(ccb);
                break;

        case XPT_GET_TRAN_SETTINGS:
        /* Get default/user set transfer settings for the target */
        {
                struct  ccb_trans_settings *cts;
                u_int   target_mask;

                cts = &(ccb->cts);
                target_mask = 0x01 << ccb->ccb_h.target_id;
                if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0) {
                        cts->flags = CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB;
                        cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
                        cts->sync_period = 6; /* 40MHz */
                        cts->sync_offset = 15;

                        cts->valid = CCB_TRANS_SYNC_RATE_VALID
                                   | CCB_TRANS_SYNC_OFFSET_VALID
                                   | CCB_TRANS_BUS_WIDTH_VALID
                                   | CCB_TRANS_DISC_VALID
                                   | CCB_TRANS_TQ_VALID;
                        ccb->ccb_h.status = CAM_REQ_CMP;
                } else {
                        ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
                }
                xpt_done(ccb);
                break;
        }

        case XPT_CALC_GEOMETRY:
        {
                struct    ccb_calc_geometry *ccg;
                u_int32_t size_mb;
                u_int32_t secs_per_cylinder;

                ccg = &(ccb->ccg);
                size_mb = ccg->volume_size
                        / ((1024L * 1024L) / ccg->block_size);

                if (size_mb > 4096) {
                        ccg->heads = 255;
                        ccg->secs_per_track = 63;
                } else if (size_mb > 2048) {
                        ccg->heads = 128;
                        ccg->secs_per_track = 63;
                } else if (size_mb > 1024) {
                        ccg->heads = 65;
                        ccg->secs_per_track = 63;
                } else {
                        ccg->heads = 64;
                        ccg->secs_per_track = 32;
                }
                secs_per_cylinder = ccg->heads * ccg->secs_per_track;
                ccg->cylinders = ccg->volume_size / secs_per_cylinder;
                ccb->ccb_h.status = CAM_REQ_CMP;
                xpt_done(ccb);
                break;
        }

        case XPT_RESET_BUS:             /* Reset the specified SCSI bus */
                ASR_resetBus (sc, cam_sim_bus(sim));
                ccb->ccb_h.status = CAM_REQ_CMP;
                xpt_done(ccb);
                break;

        case XPT_TERM_IO:               /* Terminate the I/O process */
                /* XXX Implement */
                ccb->ccb_h.status = CAM_REQ_INVALID;
                xpt_done(ccb);
                break;

        case XPT_PATH_INQ:              /* Path routing inquiry */
        {
                struct ccb_pathinq *cpi = &(ccb->cpi);

                cpi->version_num = 1; /* XXX??? */
                cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
                cpi->target_sprt = 0;
                /* Not necessary to reset bus, done by HDM initialization */
                cpi->hba_misc = PIM_NOBUSRESET;
                cpi->hba_eng_cnt = 0;
                cpi->max_target = sc->ha_MaxId;
                cpi->max_lun = sc->ha_MaxLun;
                cpi->initiator_id = sc->ha_adapter_target[cam_sim_bus(sim)];
                cpi->bus_id = cam_sim_bus(sim);
                cpi->base_transfer_speed = 3300;
                strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
                strncpy(cpi->hba_vid, "Adaptec", HBA_IDLEN);
                strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
                cpi->unit_number = cam_sim_unit(sim);
                cpi->ccb_h.status = CAM_REQ_CMP;
                xpt_done(ccb);
                break;
        }
        default:
                ccb->ccb_h.status = CAM_REQ_INVALID;
                xpt_done(ccb);
                break;
        }
} /* asr_action */

#ifdef ASR_MEASURE_PERFORMANCE
#define WRITE_OP 1
#define READ_OP 2
#define min_submitR     sc->ha_performance.read_by_size_min_time[index]
#define max_submitR     sc->ha_performance.read_by_size_max_time[index]
#define min_submitW     sc->ha_performance.write_by_size_min_time[index]
#define max_submitW     sc->ha_performance.write_by_size_max_time[index]

STATIC INLINE void
asr_IObySize(
        IN Asr_softc_t * sc,
        IN u_int32_t     submitted_time,
        IN int           op,
        IN int           index)
{
        struct timeval   submitted_timeval;

        submitted_timeval.tv_sec = 0;
        submitted_timeval.tv_usec = submitted_time;

        if ( op == READ_OP ) {
                ++sc->ha_performance.read_by_size_count[index];

                if ( submitted_time != 0xffffffff ) {
                        timevaladd(
                          &(sc->ha_performance.read_by_size_total_time[index]),
                          &submitted_timeval);
                        if ( (min_submitR == 0)
                          || (submitted_time < min_submitR) ) {
                                min_submitR = submitted_time;
                        }

                        if ( submitted_time > max_submitR ) {
                                max_submitR = submitted_time;
                        }
                }
        } else {
                ++sc->ha_performance.write_by_size_count[index];
                if ( submitted_time != 0xffffffff ) {
                        timevaladd(
                          &(sc->ha_performance.write_by_size_total_time[index]),
                          &submitted_timeval);
                        if ( (submitted_time < min_submitW)
                          || (min_submitW == 0) ) {
                                min_submitW = submitted_time;
                        }

                        if ( submitted_time > max_submitW ) {
                                max_submitW = submitted_time;
                        }
                }
        }
} /* asr_IObySize */
#endif

/*
 * Handle processing of current CCB as pointed to by the Status.
 */
STATIC int
asr_intr (
        IN Asr_softc_t * sc)
{
        OUT int          processed;

#ifdef ASR_MEASURE_PERFORMANCE
        struct timeval junk;

        microtime(&junk);
        sc->ha_performance.intr_started = junk;
#endif

        for (processed = 0;
          sc->ha_Virt->Status & Mask_InterruptsDisabled;
          processed = 1) {
                union asr_ccb                     * ccb;
                U32                                 ReplyOffset;
                PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME Reply;

                if (((ReplyOffset = sc->ha_Virt->FromFIFO) == EMPTY_QUEUE)
                 && ((ReplyOffset = sc->ha_Virt->FromFIFO) == EMPTY_QUEUE)) {
                        break;
                }
                Reply = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)(ReplyOffset
                  - sc->ha_Msgs_Phys + (char *)(sc->ha_Msgs));
                /*
                 * We do not need any (optional byteswapping) method access to
                 * the Initiator context field.
                 */
                ccb = (union asr_ccb *)(long)
                  I2O_MESSAGE_FRAME_getInitiatorContext64(
                    &(Reply->StdReplyFrame.StdMessageFrame));
                if (I2O_MESSAGE_FRAME_getMsgFlags(
                  &(Reply->StdReplyFrame.StdMessageFrame))
                  & I2O_MESSAGE_FLAGS_FAIL) {
                        defAlignLong(I2O_UTIL_NOP_MESSAGE,Message);
                        PI2O_UTIL_NOP_MESSAGE             Message_Ptr;
                        U32                               MessageOffset;

                        MessageOffset = (u_long)
                          I2O_FAILURE_REPLY_MESSAGE_FRAME_getPreservedMFA(
                            (PI2O_FAILURE_REPLY_MESSAGE_FRAME)Reply);
                        /*
                         *  Get the Original Message Frame's address, and get
                         * it's Transaction Context into our space. (Currently
                         * unused at original authorship, but better to be
                         * safe than sorry). Straight copy means that we
                         * need not concern ourselves with the (optional
                         * byteswapping) method access.
                         */
                        Reply->StdReplyFrame.TransactionContext
                          = ((PI2O_SINGLE_REPLY_MESSAGE_FRAME)
                            (sc->ha_Fvirt + MessageOffset))->TransactionContext;
                        /*
                         *      For 64 bit machines, we need to reconstruct the
                         * 64 bit context.
                         */
                        ccb = (union asr_ccb *)(long)
                          I2O_MESSAGE_FRAME_getInitiatorContext64(
                            &(Reply->StdReplyFrame.StdMessageFrame));
                        /*
                         * Unique error code for command failure.
                         */
                        I2O_SINGLE_REPLY_MESSAGE_FRAME_setDetailedStatusCode(
                          &(Reply->StdReplyFrame), (u_int16_t)-2);
                        /*
                         *  Modify the message frame to contain a NOP and
                         * re-issue it to the controller.
                         */
                        Message_Ptr = (PI2O_UTIL_NOP_MESSAGE)ASR_fillMessage(
                          Message, sizeof(I2O_UTIL_NOP_MESSAGE));
#                       if (I2O_UTIL_NOP != 0)
                                I2O_MESSAGE_FRAME_setFunction (
                                  &(Message_Ptr->StdMessageFrame),
                                  I2O_UTIL_NOP);
#                       endif
                        /*
                         *  Copy the packet out to the Original Message
                         */
                        bcopy ((caddr_t)Message_Ptr,
                          sc->ha_Fvirt + MessageOffset,
                          sizeof(I2O_UTIL_NOP_MESSAGE));
                        /*
                         *  Issue the NOP
                         */
                        sc->ha_Virt->ToFIFO = MessageOffset;
                }

                /*
                 *      Asynchronous command with no return requirements,
                 * and a generic handler for immunity against odd error
                 * returns from the adapter.
                 */
                if (ccb == (union asr_ccb *)NULL) {
                        /*
                         * Return Reply so that it can be used for the
                         * next command
                         */
                        sc->ha_Virt->FromFIFO = ReplyOffset;
                        continue;
                }

                /* Welease Wadjah! (and stop timeouts) */
                ASR_ccbRemove (sc, ccb);

                switch (
                  I2O_SINGLE_REPLY_MESSAGE_FRAME_getDetailedStatusCode(
                    &(Reply->StdReplyFrame))) {

                case I2O_SCSI_DSC_SUCCESS:
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQ_CMP;
                        break;

                case I2O_SCSI_DSC_CHECK_CONDITION:
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQ_CMP|CAM_AUTOSNS_VALID;
                        break;

                case I2O_SCSI_DSC_BUSY:
                        /* FALLTHRU */
                case I2O_SCSI_HBA_DSC_ADAPTER_BUSY:
                        /* FALLTHRU */
                case I2O_SCSI_HBA_DSC_SCSI_BUS_RESET:
                        /* FALLTHRU */
                case I2O_SCSI_HBA_DSC_BUS_BUSY:
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_SCSI_BUSY;
                        break;

                case I2O_SCSI_HBA_DSC_SELECTION_TIMEOUT:
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_SEL_TIMEOUT;
                        break;

                case I2O_SCSI_HBA_DSC_COMMAND_TIMEOUT:
                        /* FALLTHRU */
                case I2O_SCSI_HBA_DSC_DEVICE_NOT_PRESENT:
                        /* FALLTHRU */
                case I2O_SCSI_HBA_DSC_LUN_INVALID:
                        /* FALLTHRU */
                case I2O_SCSI_HBA_DSC_SCSI_TID_INVALID:
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
                        break;

                case I2O_SCSI_HBA_DSC_DATA_OVERRUN:
                        /* FALLTHRU */
                case I2O_SCSI_HBA_DSC_REQUEST_LENGTH_ERROR:
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_DATA_RUN_ERR;
                        break;

                default:
                        ccb->ccb_h.status &= ~CAM_STATUS_MASK;
                        ccb->ccb_h.status |= CAM_REQUEUE_REQ;
                        break;
                }
                if ((ccb->csio.resid = ccb->csio.dxfer_len) != 0) {
                        ccb->csio.resid -=
                          I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_getTransferCount(
                            Reply);
                }

#ifdef ASR_MEASURE_PERFORMANCE
                {
                        struct timeval  endTime;
                        u_int32_t       submitted_time;
                        u_int32_t       size;
                        int             op_type;
                        int             startTimeIndex;

                        --sc->ha_submitted_ccbs_count;
                        startTimeIndex
                          = (int)Reply->StdReplyFrame.TransactionContext;
                        if (-1 != startTimeIndex) {
                                /* Compute the time spent in device/adapter */
                                microtime(&endTime);
                                submitted_time = asr_time_delta(sc->ha_timeQ[
                                  startTimeIndex], endTime);
                                /* put the startTimeIndex back on free list */
                                ENQ_TIMEQ_FREE_LIST(startTimeIndex,
                                  sc->ha_timeQFreeList,
                                  sc->ha_timeQFreeHead,
                                  sc->ha_timeQFreeTail);
                        } else {
                                submitted_time = 0xffffffff;
                        }

#define maxctime sc->ha_performance.max_command_time[ccb->csio.cdb_io.cdb_bytes[0]]
#define minctime sc->ha_performance.min_command_time[ccb->csio.cdb_io.cdb_bytes[0]]
                        if (submitted_time != 0xffffffff) {
                                if ( maxctime < submitted_time ) {
                                        maxctime = submitted_time;
                                }
                                if ( (minctime == 0)
                                  || (minctime > submitted_time) ) {
                                        minctime = submitted_time;
                                }

                                if ( sc->ha_performance.max_submit_time
                                  < submitted_time ) {
                                        sc->ha_performance.max_submit_time
                                          = submitted_time;
                                }
                                if ( sc->ha_performance.min_submit_time == 0
                                  || sc->ha_performance.min_submit_time
                                    > submitted_time) {
                                        sc->ha_performance.min_submit_time
                                          = submitted_time;
                                }

                                switch ( ccb->csio.cdb_io.cdb_bytes[0] ) {

                                case 0xa8:      /* 12-byte READ */
                                        /* FALLTHRU */
                                case 0x08:      /* 6-byte READ  */
                                        /* FALLTHRU */
                                case 0x28:      /* 10-byte READ */
                                        op_type = READ_OP;
                                        break;

                                case 0x0a:      /* 6-byte WRITE */
                                        /* FALLTHRU */
                                case 0xaa:      /* 12-byte WRITE */
                                        /* FALLTHRU */
                                case 0x2a:      /* 10-byte WRITE */
                                        op_type = WRITE_OP;
                                        break;

                                default:
                                        op_type = 0;
                                        break;
                                }

                                if ( op_type != 0 ) {
                                        struct scsi_rw_big * cmd;

                                        cmd = (struct scsi_rw_big *)
                                          &(ccb->csio.cdb_io);

                                        size = (((u_int32_t) cmd->length2 << 8)
                                          | ((u_int32_t) cmd->length1)) << 9;

                                        switch ( size ) {

                                        case 512:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_512);
                                                break;

                                        case 1024:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_1K);
                                                break;

                                        case 2048:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_2K);
                                                break;

                                        case 4096:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_4K);
                                                break;

                                        case 8192:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_8K);
                                                break;

                                        case 16384:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_16K);
                                                break;

                                        case 32768:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_32K);
                                                break;

                                        case 65536:
                                                asr_IObySize(sc,
                                                  submitted_time, op_type,
                                                  SIZE_64K);
                                                break;

                                        default:
                                                if ( size > (1 << 16) ) {
                                                        asr_IObySize(sc,
                                                          submitted_time,
                                                          op_type,
                                                          SIZE_BIGGER);
                                                } else {
                                                        asr_IObySize(sc,
                                                          submitted_time,
                                                          op_type,
                                                          SIZE_OTHER);
                                                }
                                                break;
                                        }
                                }
                        }
                }
#endif
                /* Sense data in reply packet */
                if (ccb->ccb_h.status & CAM_AUTOSNS_VALID) {
                        u_int16_t size = I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_getAutoSenseTransferCount(Reply);

                        if (size) {
                                if (size > sizeof(ccb->csio.sense_data)) {
                                        size = sizeof(ccb->csio.sense_data);
                                }
                                if (size > I2O_SCSI_SENSE_DATA_SZ) {
                                        size = I2O_SCSI_SENSE_DATA_SZ;
                                }
                                if ((ccb->csio.sense_len)
                                 && (size > ccb->csio.sense_len)) {
                                        size = ccb->csio.sense_len;
                                }
                                bcopy ((caddr_t)Reply->SenseData,
                                  (caddr_t)&(ccb->csio.sense_data), size);
                        }
                }

                /*
                 * Return Reply so that it can be used for the next command
                 * since we have no more need for it now
                 */
                sc->ha_Virt->FromFIFO = ReplyOffset;

                if (ccb->ccb_h.path) {
                        xpt_done ((union ccb *)ccb);
                } else {
                        wakeup ((caddr_t)ccb);
                }
        }
#ifdef ASR_MEASURE_PERFORMANCE
        {
                u_int32_t result;

                microtime(&junk);
                result = asr_time_delta(sc->ha_performance.intr_started, junk);

                if (result != 0xffffffff) {
                        if ( sc->ha_performance.max_intr_time < result ) {
                                sc->ha_performance.max_intr_time = result;
                        }

                        if ( (sc->ha_performance.min_intr_time == 0)
                          || (sc->ha_performance.min_intr_time > result) ) {
                                sc->ha_performance.min_intr_time = result;
                        }
                }
        }
#endif
        return (processed);
} /* asr_intr */

#undef QueueSize        /* Grrrr */
#undef SG_Size          /* Grrrr */

/*
 *      Meant to be included at the bottom of asr.c !!!
 */

/*
 *      Included here as hard coded. Done because other necessary include
 *      files utilize C++ comment structures which make them a nuisance to
 *      included here just to pick up these three typedefs.
 */
typedef U32   DPT_TAG_T;
typedef U32   DPT_MSG_T;
typedef U32   DPT_RTN_T;

#undef SCSI_RESET       /* Conflicts with "scsi/scsiconf.h" defintion */
#include        "dev/asr/osd_unix.h"

#define asr_unit(dev)     minor(dev)

STATIC INLINE Asr_softc_t *
ASR_get_sc (
        IN dev_t          dev)
{
        int               unit = asr_unit(dev);
        OUT Asr_softc_t * sc = Asr_softc;

        while (sc && sc->ha_sim[0] && (cam_sim_unit(sc->ha_sim[0]) != unit)) {
                sc = sc->ha_next;
        }
        return (sc);
} /* ASR_get_sc */

STATIC u_int8_t ASR_ctlr_held;
#if (!defined(UNREFERENCED_PARAMETER))
# define UNREFERENCED_PARAMETER(x) (void)(x)
#endif

STATIC int
asr_open(
        IN dev_t         dev,
        int32_t          flags,
        int32_t          ifmt,
        IN struct thread * td)
{
        int              s;
        OUT int          error;
        UNREFERENCED_PARAMETER(flags);
        UNREFERENCED_PARAMETER(ifmt);

        if (ASR_get_sc (dev) == (Asr_softc_t *)NULL) {
                return (ENODEV);
        }
        s = splcam ();
        if (ASR_ctlr_held) {
                error = EBUSY;
        } else if ((error = suser(td->td_proc)) == 0) {
                ++ASR_ctlr_held;
        }
        splx(s);
        return (error);
} /* asr_open */

STATIC int
asr_close(
        dev_t         dev,
        int           flags,
        int           ifmt,
        struct thread * td)
{
        UNREFERENCED_PARAMETER(dev);
        UNREFERENCED_PARAMETER(flags);
        UNREFERENCED_PARAMETER(ifmt);
        UNREFERENCED_PARAMETER(td);

        ASR_ctlr_held = 0;
        return (0);
} /* asr_close */


/*-------------------------------------------------------------------------*/
/*                    Function ASR_queue_i                                 */
/*-------------------------------------------------------------------------*/
/* The Parameters Passed To This Function Are :                            */
/*     Asr_softc_t *      : HBA miniport driver's adapter data storage.    */
/*     PI2O_MESSAGE_FRAME : Msg Structure Pointer For This Command         */
/*      I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME following the Msg Structure     */
/*                                                                         */
/* This Function Will Take The User Request Packet And Convert It To An    */
/* I2O MSG And Send It Off To The Adapter.                                 */
/*                                                                         */
/* Return : 0 For OK, Error Code Otherwise                                 */
/*-------------------------------------------------------------------------*/
STATIC INLINE int
ASR_queue_i(
        IN Asr_softc_t                             * sc,
        INOUT PI2O_MESSAGE_FRAME                     Packet)
{
        union asr_ccb                              * ccb;
        PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME          Reply;
        PI2O_MESSAGE_FRAME                           Message_Ptr;
        PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME          Reply_Ptr;
        int                                          MessageSizeInBytes;
        int                                          ReplySizeInBytes;
        int                                          error;
        int                                          s;
        /* Scatter Gather buffer list */
        struct ioctlSgList_S {
                SLIST_ENTRY(ioctlSgList_S) link;
                caddr_t                    UserSpace;
                I2O_FLAGS_COUNT            FlagsCount;
                char                       KernelSpace[sizeof(long)];
        }                                          * elm;
        /* Generates a `first' entry */
        SLIST_HEAD(ioctlSgListHead_S, ioctlSgList_S) sgList;

        if (ASR_getBlinkLedCode(sc)) {
                debug_usr_cmd_printf ("Adapter currently in BlinkLed %x\n",
                  ASR_getBlinkLedCode(sc));
                return (EIO);
        }
        /* Copy in the message into a local allocation */
        if ((Message_Ptr = (PI2O_MESSAGE_FRAME)malloc (
          sizeof(I2O_MESSAGE_FRAME), M_TEMP, M_WAITOK))
         == (PI2O_MESSAGE_FRAME)NULL) {
                debug_usr_cmd_printf (
                  "Failed to acquire I2O_MESSAGE_FRAME memory\n");
                return (ENOMEM);
        }
        if ((error = copyin ((caddr_t)Packet, (caddr_t)Message_Ptr,
          sizeof(I2O_MESSAGE_FRAME))) != 0) {
                free (Message_Ptr, M_TEMP);
                debug_usr_cmd_printf ("Can't copy in packet errno=%d\n", error);
                return (error);
        }
        /* Acquire information to determine type of packet */
        MessageSizeInBytes = (I2O_MESSAGE_FRAME_getMessageSize(Message_Ptr)<<2);
        /* The offset of the reply information within the user packet */
        Reply = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)((char *)Packet
          + MessageSizeInBytes);

        /* Check if the message is a synchronous initialization command */
        s = I2O_MESSAGE_FRAME_getFunction(Message_Ptr);
        free (Message_Ptr, M_TEMP);
        switch (s) {

        case I2O_EXEC_IOP_RESET:
        {       U32 status;

                status = ASR_resetIOP(sc->ha_Virt, sc->ha_Fvirt);
                ReplySizeInBytes = sizeof(status);
                debug_usr_cmd_printf ("resetIOP done\n");
                return (copyout ((caddr_t)&status, (caddr_t)Reply,
                  ReplySizeInBytes));
        }

        case I2O_EXEC_STATUS_GET:
        {       I2O_EXEC_STATUS_GET_REPLY status;

                if (ASR_getStatus (sc->ha_Virt, sc->ha_Fvirt, &status)
                  == (PI2O_EXEC_STATUS_GET_REPLY)NULL) {
                        debug_usr_cmd_printf ("getStatus failed\n");
                        return (ENXIO);
                }
                ReplySizeInBytes = sizeof(status);
                debug_usr_cmd_printf ("getStatus done\n");
                return (copyout ((caddr_t)&status, (caddr_t)Reply,
                  ReplySizeInBytes));
        }

        case I2O_EXEC_OUTBOUND_INIT:
        {       U32 status;

                status = ASR_initOutBound(sc);
                ReplySizeInBytes = sizeof(status);
                debug_usr_cmd_printf ("intOutBound done\n");
                return (copyout ((caddr_t)&status, (caddr_t)Reply,
                  ReplySizeInBytes));
        }
        }

        /* Determine if the message size is valid */
        if ((MessageSizeInBytes < sizeof(I2O_MESSAGE_FRAME))
         || (MAX_INBOUND_SIZE < MessageSizeInBytes)) {
                debug_usr_cmd_printf ("Packet size %d incorrect\n",
                  MessageSizeInBytes);
                return (EINVAL);
        }

        if ((Message_Ptr = (PI2O_MESSAGE_FRAME)malloc (MessageSizeInBytes,
          M_TEMP, M_WAITOK)) == (PI2O_MESSAGE_FRAME)NULL) {
                debug_usr_cmd_printf ("Failed to acquire frame[%d] memory\n",
                  MessageSizeInBytes);
                return (ENOMEM);
        }
        if ((error = copyin ((caddr_t)Packet, (caddr_t)Message_Ptr,
          MessageSizeInBytes)) != 0) {
                free (Message_Ptr, M_TEMP);
                debug_usr_cmd_printf ("Can't copy in packet[%d] errno=%d\n",
                  MessageSizeInBytes, error);
                return (error);
        }

        /* Check the size of the reply frame, and start constructing */

        if ((Reply_Ptr = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)malloc (
          sizeof(I2O_MESSAGE_FRAME), M_TEMP, M_WAITOK))
          == (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)NULL) {
                free (Message_Ptr, M_TEMP);
                debug_usr_cmd_printf (
                  "Failed to acquire I2O_MESSAGE_FRAME memory\n");
                return (ENOMEM);
        }
        if ((error = copyin ((caddr_t)Reply, (caddr_t)Reply_Ptr,
          sizeof(I2O_MESSAGE_FRAME))) != 0) {
                free (Reply_Ptr, M_TEMP);
                free (Message_Ptr, M_TEMP);
                debug_usr_cmd_printf (
                  "Failed to copy in reply frame, errno=%d\n",
                  error);
                return (error);
        }
        ReplySizeInBytes = (I2O_MESSAGE_FRAME_getMessageSize(
          &(Reply_Ptr->StdReplyFrame.StdMessageFrame)) << 2);
        free (Reply_Ptr, M_TEMP);
        if (ReplySizeInBytes < sizeof(I2O_SINGLE_REPLY_MESSAGE_FRAME)) {
                free (Message_Ptr, M_TEMP);
                debug_usr_cmd_printf (
                  "Failed to copy in reply frame[%d], errno=%d\n",
                  ReplySizeInBytes, error);
                return (EINVAL);
        }

        if ((Reply_Ptr = (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)malloc (
          ((ReplySizeInBytes > sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME))
            ? ReplySizeInBytes
            : sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)),
          M_TEMP, M_WAITOK)) == (PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)NULL) {
                free (Message_Ptr, M_TEMP);
                debug_usr_cmd_printf ("Failed to acquire frame[%d] memory\n",
                  ReplySizeInBytes);
                return (ENOMEM);
        }
        (void)ASR_fillMessage ((char *)Reply_Ptr, ReplySizeInBytes);
        Reply_Ptr->StdReplyFrame.StdMessageFrame.InitiatorContext
          = Message_Ptr->InitiatorContext;
        Reply_Ptr->StdReplyFrame.TransactionContext
          = ((PI2O_PRIVATE_MESSAGE_FRAME)Message_Ptr)->TransactionContext;
        I2O_MESSAGE_FRAME_setMsgFlags(
          &(Reply_Ptr->StdReplyFrame.StdMessageFrame),
          I2O_MESSAGE_FRAME_getMsgFlags(
            &(Reply_Ptr->StdReplyFrame.StdMessageFrame))
              | I2O_MESSAGE_FLAGS_REPLY);

        /* Check if the message is a special case command */
        switch (I2O_MESSAGE_FRAME_getFunction(Message_Ptr)) {
        case I2O_EXEC_SYS_TAB_SET: /* Special Case of empty Scatter Gather */
                if (MessageSizeInBytes == ((I2O_MESSAGE_FRAME_getVersionOffset(
                  Message_Ptr) & 0xF0) >> 2)) {
                        free (Message_Ptr, M_TEMP);
                        I2O_SINGLE_REPLY_MESSAGE_FRAME_setDetailedStatusCode(
                          &(Reply_Ptr->StdReplyFrame),
                          (ASR_setSysTab(sc) != CAM_REQ_CMP));
                        I2O_MESSAGE_FRAME_setMessageSize(
                          &(Reply_Ptr->StdReplyFrame.StdMessageFrame),
                          sizeof(I2O_SINGLE_REPLY_MESSAGE_FRAME));
                        error = copyout ((caddr_t)Reply_Ptr, (caddr_t)Reply,
                          ReplySizeInBytes);
                        free (Reply_Ptr, M_TEMP);
                        return (error);
                }
        }

        /* Deal in the general case */
        /* First allocate and optionally copy in each scatter gather element */
        SLIST_INIT(&sgList);
        if ((I2O_MESSAGE_FRAME_getVersionOffset(Message_Ptr) & 0xF0) != 0) {
                PI2O_SGE_SIMPLE_ELEMENT sg;

                /*
                 *      since this code is reused in several systems, code
                 * efficiency is greater by using a shift operation rather
                 * than a divide by sizeof(u_int32_t).
                 */
                sg = (PI2O_SGE_SIMPLE_ELEMENT)((char *)Message_Ptr
                  + ((I2O_MESSAGE_FRAME_getVersionOffset(Message_Ptr) & 0xF0)
                    >> 2));
                while (sg < (PI2O_SGE_SIMPLE_ELEMENT)(((caddr_t)Message_Ptr)
                  + MessageSizeInBytes)) {
                        caddr_t v;
                        int     len;

                        if ((I2O_FLAGS_COUNT_getFlags(&(sg->FlagsCount))
                         & I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT) == 0) {
                                error = EINVAL;
                                break;
                        }
                        len = I2O_FLAGS_COUNT_getCount(&(sg->FlagsCount));
                        debug_usr_cmd_printf ("SG[%d] = %x[%d]\n",
                          sg - (PI2O_SGE_SIMPLE_ELEMENT)((char *)Message_Ptr
                          + ((I2O_MESSAGE_FRAME_getVersionOffset(
                                Message_Ptr) & 0xF0) >> 2)),
                          I2O_SGE_SIMPLE_ELEMENT_getPhysicalAddress(sg), len);

                        if ((elm = (struct ioctlSgList_S *)malloc (
                          sizeof(*elm) - sizeof(elm->KernelSpace) + len,
                          M_TEMP, M_WAITOK))
                          == (struct ioctlSgList_S *)NULL) {
                                debug_usr_cmd_printf (
                                  "Failed to allocate SG[%d]\n", len);
                                error = ENOMEM;
                                break;
                        }
                        SLIST_INSERT_HEAD(&sgList, elm, link);
                        elm->FlagsCount = sg->FlagsCount;
                        elm->UserSpace = (caddr_t)
                          (I2O_SGE_SIMPLE_ELEMENT_getPhysicalAddress(sg));
                        v = elm->KernelSpace;
                        /* Copy in outgoing data (DIR bit could be invalid) */
                        if ((error = copyin (elm->UserSpace, (caddr_t)v, len))
                          != 0) {
                                break;
                        }
                        /*
                         *      If the buffer is not contiguous, lets
                         * break up the scatter/gather entries.
                         */
                        while ((len > 0)
                         && (sg < (PI2O_SGE_SIMPLE_ELEMENT)
                          (((caddr_t)Message_Ptr) + MAX_INBOUND_SIZE))) {
                                int next, base, span;

                                span = 0;
                                next = base = KVTOPHYS(v);
                                I2O_SGE_SIMPLE_ELEMENT_setPhysicalAddress(sg,
                                  base);

                                /* How far can we go physically contiguously */
                                while ((len > 0) && (base == next)) {
                                        int size;

                                        next = trunc_page(base) + PAGE_SIZE;
                                        size = next - base;
                                        if (size > len) {
                                                size = len;
                                        }
                                        span += size;
                                        v += size;
                                        len -= size;
                                        base = KVTOPHYS(v);
                                }

                                /* Construct the Flags */
                                I2O_FLAGS_COUNT_setCount(&(sg->FlagsCount),
                                  span);
                                {
                                        int flags = I2O_FLAGS_COUNT_getFlags(
                                          &(elm->FlagsCount));
                                        /* Any remaining length? */
                                        if (len > 0) {
                                            flags &=
                                                ~(I2O_SGL_FLAGS_END_OF_BUFFER
                                                 | I2O_SGL_FLAGS_LAST_ELEMENT);
                                        }
                                        I2O_FLAGS_COUNT_setFlags(
                                          &(sg->FlagsCount), flags);
                                }

                                debug_usr_cmd_printf ("sg[%d] = %x[%d]\n",
                                  sg - (PI2O_SGE_SIMPLE_ELEMENT)
                                    ((char *)Message_Ptr
                                  + ((I2O_MESSAGE_FRAME_getVersionOffset(
                                        Message_Ptr) & 0xF0) >> 2)),
                                  I2O_SGE_SIMPLE_ELEMENT_getPhysicalAddress(sg),
                                  span);
                                if (len <= 0) {
                                        break;
                                }

                                /*
                                 * Incrementing requires resizing of the
                                 * packet, and moving up the existing SG
                                 * elements.
                                 */
                                ++sg;
                                MessageSizeInBytes += sizeof(*sg);
                                I2O_MESSAGE_FRAME_setMessageSize(Message_Ptr,
                                  I2O_MESSAGE_FRAME_getMessageSize(Message_Ptr)
                                  + (sizeof(*sg) / sizeof(U32)));
                                {
                                        PI2O_MESSAGE_FRAME NewMessage_Ptr;

                                        if ((NewMessage_Ptr
                                          = (PI2O_MESSAGE_FRAME)
                                            malloc (MessageSizeInBytes,
                                             M_TEMP, M_WAITOK))
                                            == (PI2O_MESSAGE_FRAME)NULL) {
                                                debug_usr_cmd_printf (
                                                  "Failed to acquire frame[%d] memory\n",
                                                  MessageSizeInBytes);
                                                error = ENOMEM;
                                                break;
                                        }
                                        span = ((caddr_t)sg)
                                             - (caddr_t)Message_Ptr;
                                        bcopy ((caddr_t)Message_Ptr,
                                          (caddr_t)NewMessage_Ptr, span);
                                        bcopy ((caddr_t)(sg-1),
                                          ((caddr_t)NewMessage_Ptr) + span,
                                          MessageSizeInBytes - span);
                                        free (Message_Ptr, M_TEMP);
                                        sg = (PI2O_SGE_SIMPLE_ELEMENT)
                                          (((caddr_t)NewMessage_Ptr) + span);
                                        Message_Ptr = NewMessage_Ptr;
                                }
                        }
                        if ((error)
                         || ((I2O_FLAGS_COUNT_getFlags(&(sg->FlagsCount))
                          & I2O_SGL_FLAGS_LAST_ELEMENT) != 0)) {
                                break;
                        }
                        ++sg;
                }
                if (error) {
                        while ((elm = SLIST_FIRST(&sgList))
                          != (struct ioctlSgList_S *)NULL) {
                                SLIST_REMOVE_HEAD(&sgList, link);
                                free (elm, M_TEMP);
                        }
                        free (Reply_Ptr, M_TEMP);
                        free (Message_Ptr, M_TEMP);
                        return (error);
                }
        }

        debug_usr_cmd_printf ("Inbound: ");
        debug_usr_cmd_dump_message(Message_Ptr);

        /* Send the command */
        if ((ccb = asr_alloc_ccb (sc)) == (union asr_ccb *)NULL) {
                /* Free up in-kernel buffers */
                while ((elm = SLIST_FIRST(&sgList))
                  != (struct ioctlSgList_S *)NULL) {
                        SLIST_REMOVE_HEAD(&sgList, link);
                        free (elm, M_TEMP);
                }
                free (Reply_Ptr, M_TEMP);
                free (Message_Ptr, M_TEMP);
                return (ENOMEM);
        }

        /*
         * We do not need any (optional byteswapping) method access to
         * the Initiator context field.
         */
        I2O_MESSAGE_FRAME_setInitiatorContext64(
          (PI2O_MESSAGE_FRAME)Message_Ptr, (long)ccb);

        (void)ASR_queue (sc, (PI2O_MESSAGE_FRAME)Message_Ptr);

        free (Message_Ptr, M_TEMP);

        /*
         * Wait for the board to report a finished instruction.
         */
        s = splcam();
        while ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
                if (ASR_getBlinkLedCode(sc)) {
                        /* Reset Adapter */
                        printf ("asr%d: Blink LED 0x%x resetting adapter\n",
                          cam_sim_unit(xpt_path_sim(ccb->ccb_h.path)),
                          ASR_getBlinkLedCode(sc));
                        if (ASR_reset (sc) == ENXIO) {
                                /* Command Cleanup */
                                ASR_ccbRemove(sc, ccb);
                        }
                        splx(s);
                        /* Free up in-kernel buffers */
                        while ((elm = SLIST_FIRST(&sgList))
                          != (struct ioctlSgList_S *)NULL) {
                                SLIST_REMOVE_HEAD(&sgList, link);
                                free (elm, M_TEMP);
                        }
                        free (Reply_Ptr, M_TEMP);
                        asr_free_ccb(ccb);
                        return (EIO);
                }
                /* Check every second for BlinkLed */
                /* There is no PRICAM, but outwardly PRIBIO is functional */
                tsleep((caddr_t)ccb, PRIBIO, "asr", hz);
        }
        splx(s);

        debug_usr_cmd_printf ("Outbound: ");
        debug_usr_cmd_dump_message(Reply_Ptr);

        I2O_SINGLE_REPLY_MESSAGE_FRAME_setDetailedStatusCode(
          &(Reply_Ptr->StdReplyFrame),
          (ccb->ccb_h.status != CAM_REQ_CMP));

        if (ReplySizeInBytes >= (sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
          - I2O_SCSI_SENSE_DATA_SZ - sizeof(U32))) {
                I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_setTransferCount(Reply_Ptr,
                  ccb->csio.dxfer_len - ccb->csio.resid);
        }
        if ((ccb->ccb_h.status & CAM_AUTOSNS_VALID) && (ReplySizeInBytes
         > (sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
         - I2O_SCSI_SENSE_DATA_SZ))) {
                int size = ReplySizeInBytes
                  - sizeof(I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME)
                  - I2O_SCSI_SENSE_DATA_SZ;

                if (size > sizeof(ccb->csio.sense_data)) {
                        size = sizeof(ccb->csio.sense_data);
                }
                bcopy ((caddr_t)&(ccb->csio.sense_data), (caddr_t)Reply_Ptr->SenseData,
                  size);
                I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME_setAutoSenseTransferCount(
                  Reply_Ptr, size);
        }

        /* Free up in-kernel buffers */
        while ((elm = SLIST_FIRST(&sgList)) != (struct ioctlSgList_S *)NULL) {
                /* Copy out as necessary */
                if ((error == 0)
                /* DIR bit considered `valid', error due to ignorance works */
                 && ((I2O_FLAGS_COUNT_getFlags(&(elm->FlagsCount))
                  & I2O_SGL_FLAGS_DIR) == 0)) {
                        error = copyout ((caddr_t)(elm->KernelSpace),
                          elm->UserSpace,
                          I2O_FLAGS_COUNT_getCount(&(elm->FlagsCount)));
                }
                SLIST_REMOVE_HEAD(&sgList, link);
                free (elm, M_TEMP);
        }
        if (error == 0) {
        /* Copy reply frame to user space */
                error = copyout ((caddr_t)Reply_Ptr, (caddr_t)Reply,
                  ReplySizeInBytes);
        }
        free (Reply_Ptr, M_TEMP);
        asr_free_ccb(ccb);

        return (error);
} /* ASR_queue_i */

/*----------------------------------------------------------------------*/
/*                          Function asr_ioctl                         */
/*----------------------------------------------------------------------*/
/* The parameters passed to this function are :                         */
/*     dev  : Device number.                                            */
/*     cmd  : Ioctl Command                                             */
/*     data : User Argument Passed In.                                  */
/*     flag : Mode Parameter                                            */
/*     proc : Process Parameter                                         */
/*                                                                      */
/* This function is the user interface into this adapter driver         */
/*                                                                      */
/* Return : zero if OK, error code if not                               */
/*----------------------------------------------------------------------*/

STATIC int
asr_ioctl(
        IN dev_t      dev,
        IN u_long     cmd,
        INOUT caddr_t data,
        int           flag,
        struct thread * td)
{
        int           i, j;
        OUT int       error = 0;
        Asr_softc_t * sc = ASR_get_sc (dev);
        UNREFERENCED_PARAMETER(flag);
        UNREFERENCED_PARAMETER(td);

        if (sc != (Asr_softc_t *)NULL)
        switch(cmd) {

        case DPT_SIGNATURE:
#       if (dsDescription_size != 50)
            case DPT_SIGNATURE + ((50 - dsDescription_size) << 16):
#       endif
                if (cmd & 0xFFFF0000) {
                        (void)bcopy ((caddr_t)(&ASR_sig), data,
                            sizeof(dpt_sig_S));
                        return (0);
                }
        /* Traditional version of the ioctl interface */
        case DPT_SIGNATURE & 0x0000FFFF:
                return (copyout ((caddr_t)(&ASR_sig), *((caddr_t *)data),
                    sizeof(dpt_sig_S)));

        /* Traditional version of the ioctl interface */
        case DPT_CTRLINFO & 0x0000FFFF:
        case DPT_CTRLINFO: {
                struct {
                        u_int16_t length;
                        u_int16_t drvrHBAnum;
                        u_int32_t baseAddr;
                        u_int16_t blinkState;
                        u_int8_t  pciBusNum;
                        u_int8_t  pciDeviceNum;
                        u_int16_t hbaFlags;
                        u_int16_t Interrupt;
                        u_int32_t reserved1;
                        u_int32_t reserved2;
                        u_int32_t reserved3;
                } CtlrInfo;

                bzero (&CtlrInfo, sizeof(CtlrInfo));
                CtlrInfo.length = sizeof(CtlrInfo) - sizeof(u_int16_t);
                CtlrInfo.drvrHBAnum = asr_unit(dev);
                CtlrInfo.baseAddr = (u_long)sc->ha_Base;
                i = ASR_getBlinkLedCode (sc);
                if (i == -1) {
                        i = 0;
                }
                CtlrInfo.blinkState = i;
                CtlrInfo.pciBusNum = sc->ha_pciBusNum;
                CtlrInfo.pciDeviceNum = sc->ha_pciDeviceNum;
#define FLG_OSD_PCI_VALID 0x0001
#define FLG_OSD_DMA       0x0002
#define FLG_OSD_I2O       0x0004
                CtlrInfo.hbaFlags = FLG_OSD_PCI_VALID | FLG_OSD_DMA | FLG_OSD_I2O;
                CtlrInfo.Interrupt = sc->ha_irq;
                if (cmd & 0xFFFF0000) {
                        bcopy (&CtlrInfo, data, sizeof(CtlrInfo));
                } else {
                        error = copyout (&CtlrInfo, *(caddr_t *)data, sizeof(CtlrInfo));
                }
        }       return (error);

        /* Traditional version of the ioctl interface */
        case DPT_SYSINFO & 0x0000FFFF:
        case DPT_SYSINFO: {
                sysInfo_S       Info;
                char          * cp;
                /* Kernel Specific ptok `hack' */
#               define          ptok(a) ((char *)(a) + KERNBASE)

                bzero (&Info, sizeof(Info));

                /* Appears I am the only person in the Kernel doing this */
                outb (0x70, 0x12);
                i = inb(0x71);
                j = i >> 4;
                if (i == 0x0f) {
                        outb (0x70, 0x19);
                        j = inb (0x71);
                }
                Info.drive0CMOS = j;

                j = i & 0x0f;
                if (i == 0x0f) {
                        outb (0x70, 0x1a);
                        j = inb (0x71);
                }
                Info.drive1CMOS = j;

                Info.numDrives = *((char *)ptok(0x475));

                Info.processorFamily = ASR_sig.dsProcessorFamily;
#if defined (__i386__)
                switch (cpu) {
                case CPU_386SX: case CPU_386:
                        Info.processorType = PROC_386; break;
                case CPU_486SX: case CPU_486:
                        Info.processorType = PROC_486; break;
                case CPU_586:
                        Info.processorType = PROC_PENTIUM; break;
                case CPU_686:
                        Info.processorType = PROC_SEXIUM; break;
                }
#elif defined (__alpha__)
		Info.processorType = PROC_ALPHA;
#endif

                Info.osType = OS_BSDI_UNIX;
                Info.osMajorVersion = osrelease[0] - '0';
                Info.osMinorVersion = osrelease[2] - '0';
                /* Info.osRevision = 0; */
                /* Info.osSubRevision = 0; */
                Info.busType = SI_PCI_BUS;
                Info.flags = SI_CMOS_Valid | SI_NumDrivesValid
                       | SI_OSversionValid | SI_BusTypeValid | SI_NO_SmartROM;

                /* Go Out And Look For I2O SmartROM */
                for(j = 0xC8000; j < 0xE0000; j += 2048) {
                        int k;

                        cp = ptok(j);
                        if (*((unsigned short *)cp) != 0xAA55) {
                                continue;
                        }
                        j += (cp[2] * 512) - 2048;
                        if ((*((u_long *)(cp + 6))
                          != ('S' + (' ' * 256) + (' ' * 65536L)))
                         || (*((u_long *)(cp + 10))
                          != ('I' + ('2' * 256) + ('0' * 65536L)))) {
                                continue;
                        }
                        cp += 0x24;
                        for (k = 0; k < 64; ++k) {
                                if (*((unsigned short *)cp)
                                 == (' ' + ('v' * 256))) {
                                        break;
                                }
                        }
                        if (k < 64) {
                                Info.smartROMMajorVersion
                                    = *((unsigned char *)(cp += 4)) - '0';
                                Info.smartROMMinorVersion
                                    = *((unsigned char *)(cp += 2));
                                Info.smartROMRevision
                                    = *((unsigned char *)(++cp));
                                Info.flags |= SI_SmartROMverValid;
                                Info.flags &= ~SI_NO_SmartROM;
                                break;
                        }
                }
                /* Get The Conventional Memory Size From CMOS */
                outb (0x70, 0x16);
                j = inb (0x71);
                j <<= 8;
                outb (0x70, 0x15);
                j |= inb(0x71);
                Info.conventionalMemSize = j;

                /* Get The Extended Memory Found At Power On From CMOS */
                outb (0x70, 0x31);
                j = inb (0x71);
                j <<= 8;
                outb (0x70, 0x30);
                j |= inb(0x71);
                Info.extendedMemSize = j;
                Info.flags |= SI_MemorySizeValid;

#               if (defined(THIS_IS_BROKEN))
                /* If There Is 1 or 2 Drives Found, Set Up Drive Parameters */
                if (Info.numDrives > 0) {
                        /*
                         *      Get The Pointer From Int 41 For The First
                         *      Drive Parameters
                         */
                        j = ((unsigned)(*((unsigned short *)ptok(0x104+2))) << 4)
                           + (unsigned)(*((unsigned short *)ptok(0x104+0)));
                        /*
                         * It appears that SmartROM's Int41/Int46 pointers
                         * use memory that gets stepped on by the kernel
                         * loading. We no longer have access to this
                         * geometry information but try anyways (!?)
                         */
                        Info.drives[0].cylinders = *((unsigned char *)ptok(j));
                        ++j;
                        Info.drives[0].cylinders += ((int)*((unsigned char *)
                            ptok(j))) << 8;
                        ++j;
                        Info.drives[0].heads = *((unsigned char *)ptok(j));
                        j += 12;
                        Info.drives[0].sectors = *((unsigned char *)ptok(j));
                        Info.flags |= SI_DriveParamsValid;
                        if ((Info.drives[0].cylinders == 0)
                         || (Info.drives[0].heads == 0)
                         || (Info.drives[0].sectors == 0)) {
                                Info.flags &= ~SI_DriveParamsValid;
                        }
                        if (Info.numDrives > 1) {
                                /*
                                 *      Get The Pointer From Int 46 For The
                                 *      Second Drive Parameters
                                 */
                                j = ((unsigned)(*((unsigned short *)ptok(0x118+2))) << 4)
                                   + (unsigned)(*((unsigned short *)ptok(0x118+0)));
                                Info.drives[1].cylinders = *((unsigned char *)
                                    ptok(j));
                                ++j;
                                Info.drives[1].cylinders += ((int)
                                    *((unsigned char *)ptok(j))) << 8;
                                ++j;
                                Info.drives[1].heads = *((unsigned char *)
                                    ptok(j));
                                j += 12;
                                Info.drives[1].sectors = *((unsigned char *)
                                    ptok(j));
                                if ((Info.drives[1].cylinders == 0)
                                 || (Info.drives[1].heads == 0)
                                 || (Info.drives[1].sectors == 0)) {
                                        Info.flags &= ~SI_DriveParamsValid;
                                }
                        }
                }
#               endif
                /* Copy Out The Info Structure To The User */
                if (cmd & 0xFFFF0000) {
                        bcopy (&Info, data, sizeof(Info));
                } else {
                        error = copyout (&Info, *(caddr_t *)data, sizeof(Info));
                }
                return (error); }

                /* Get The BlinkLED State */
        case DPT_BLINKLED:
                i = ASR_getBlinkLedCode (sc);
                if (i == -1) {
                        i = 0;
                }
                if (cmd & 0xFFFF0000) {
                        bcopy ((caddr_t)(&i), data, sizeof(i));
                } else {
                        error = copyout (&i, *(caddr_t *)data, sizeof(i));
                }
                break;

                /* Get performance metrics */
#ifdef ASR_MEASURE_PERFORMANCE
        case DPT_PERF_INFO:
                bcopy((caddr_t) &(sc->ha_performance), data,
                  sizeof(sc->ha_performance));
                return (0);
#endif

                /* Send an I2O command */
        case I2OUSRCMD:
                return (ASR_queue_i (sc, *((PI2O_MESSAGE_FRAME *)data)));

                /* Reset and re-initialize the adapter */
        case I2ORESETCMD:
                return (ASR_reset (sc));

                /* Rescan the LCT table and resynchronize the information */
        case I2ORESCANCMD:
                return (ASR_rescan (sc));
        }
        return (EINVAL);
} /* asr_ioctl */

#ifdef ASR_MEASURE_PERFORMANCE
/*
 * This function subtracts one timeval structure from another,
 * Returning the result in usec.
 * It assumes that less than 4 billion usecs passed form start to end.
 * If times are sensless, 0xffffffff is returned.
 */

STATIC u_int32_t
asr_time_delta(
        IN struct timeval start,
        IN struct timeval end)
{
        OUT u_int32_t     result;

        if (start.tv_sec > end.tv_sec) {
                result = 0xffffffff;
        }
        else {
                if (start.tv_sec == end.tv_sec) {
                        if (start.tv_usec > end.tv_usec) {
                                result = 0xffffffff;
                        } else {
                                return (end.tv_usec - start.tv_usec);
                        }
                } else {
                        return (end.tv_sec - start.tv_sec) * 1000000 +
                                end.tv_usec + (1000000 - start.tv_usec);
                }
        }
        return(result);
} /* asr_time_delta */
#endif
