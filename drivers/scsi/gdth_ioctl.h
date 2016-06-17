#ifndef _GDTH_IOCTL_H
#define _GDTH_IOCTL_H

/* gdth_ioctl.h
 * $Id: gdth_ioctl.h,v 1.10 2001/05/22 06:28:59 achim Exp $
 */

/* IOCTLs */
#define GDTIOCTL_MASK       ('J'<<8)
#define GDTIOCTL_GENERAL    (GDTIOCTL_MASK | 0) /* general IOCTL */
#define GDTIOCTL_DRVERS     (GDTIOCTL_MASK | 1) /* get driver version */
#define GDTIOCTL_CTRTYPE    (GDTIOCTL_MASK | 2) /* get controller type */
#define GDTIOCTL_OSVERS     (GDTIOCTL_MASK | 3) /* get OS version */
#define GDTIOCTL_HDRLIST    (GDTIOCTL_MASK | 4) /* get host drive list */
#define GDTIOCTL_CTRCNT     (GDTIOCTL_MASK | 5) /* get controller count */
#define GDTIOCTL_LOCKDRV    (GDTIOCTL_MASK | 6) /* lock host drive */
#define GDTIOCTL_LOCKCHN    (GDTIOCTL_MASK | 7) /* lock channel */
#define GDTIOCTL_EVENT      (GDTIOCTL_MASK | 8) /* read controller events */
#define GDTIOCTL_SCSI       (GDTIOCTL_MASK | 9) /* SCSI command */
#define GDTIOCTL_RESET_BUS  (GDTIOCTL_MASK |10) /* reset SCSI bus */
#define GDTIOCTL_RESCAN     (GDTIOCTL_MASK |11) /* rescan host drives */
#define GDTIOCTL_RESET_DRV  (GDTIOCTL_MASK |12) /* reset (remote) drv. res. */

#define GDTIOCTL_MAGIC      0xaffe0004
#define EVENT_SIZE          294 
#define MAX_HDRIVES         100                     

/* IOCTL structure (write) */
typedef struct {
    ulong32                 magic;              /* IOCTL magic */
    ushort                  ioctl;              /* IOCTL */
    ushort                  ionode;             /* controller number */
    ushort                  service;            /* controller service */
    ushort                  timeout;            /* timeout */
    union {
        struct {
            unchar          command[512];       /* controller command */
            unchar          data[1];            /* add. data */
        } general;
        struct {
            unchar          lock;               /* lock/unlock */
            unchar          drive_cnt;          /* drive count */
            ushort          drives[MAX_HDRIVES];/* drives */
        } lockdrv;
        struct {
            unchar          lock;               /* lock/unlock */
            unchar          channel;            /* channel */
        } lockchn;
        struct {
            int             erase;              /* erase event ? */
            int             handle;
            unchar          evt[EVENT_SIZE];    /* event structure */
        } event;
        struct {
            unchar          bus;                /* SCSI bus */
            unchar          target;             /* target ID */
            unchar          lun;                /* LUN */
            unchar          cmd_len;            /* command length */
            unchar          cmd[12];            /* SCSI command */
        } scsi;
        struct {
            ushort          hdr_no;             /* host drive number */
            unchar          flag;               /* old meth./add/remove */
        } rescan;
    } iu;
} gdth_iowr_str;

/* IOCTL structure (read) */
typedef struct {
    ulong32                 size;               /* buffer size */
    ulong32                 status;             /* IOCTL error code */
    union {
        struct {
            unchar          data[1];            /* data */
        } general;
        struct {
            ushort          version;            /* driver version */
        } drvers;
        struct {
            unchar          type;               /* controller type */
            ushort          info;               /* slot etc. */
            ushort          oem_id;             /* OEM ID */
            ushort          bios_ver;           /* not used */
            ushort          access;             /* not used */
            ushort          ext_type;           /* extended type */
            ushort          device_id;          /* device ID */
            ushort          sub_device_id;      /* sub device ID */
        } ctrtype;
        struct {
            unchar          version;            /* OS version */
            unchar          subversion;         /* OS subversion */
            ushort          revision;           /* revision */
        } osvers;
        struct {
            ushort          count;              /* controller count */
        } ctrcnt;
        struct {
            int             handle;
            unchar          evt[EVENT_SIZE];    /* event structure */
        } event;
        struct {
            unchar          bus;                /* SCSI bus, 0xff: invalid */
            unchar          target;             /* target ID */
            unchar          lun;                /* LUN */
            unchar          cluster_type;       /* cluster properties */
        } hdr_list[MAX_HDRIVES];                /* index is host drive number */
    } iu;
} gdth_iord_str;


#endif

