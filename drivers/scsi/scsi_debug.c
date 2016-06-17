/* 
 *  linux/kernel/scsi_debug.c
 *
 *  Copyright (C) 1992  Eric Youngdale
 *  Simulate a host adapter with 2 disks attached.  Do a lot of checking
 *  to make sure that we are not getting blocks mixed up, and PANIC if
 *  anything out of the ordinary is seen.
 *
 *  This version is more generic, simulating a variable number of disk 
 *  (or disk like devices) sharing a common amount of RAM (default 8 MB
 *  but can be set at driver/module load time).
 *
 *  For documentation see http://www.torque.net/sg/sdebug.html
 *
 *   D. Gilbert (dpg) work for Magneto-Optical device test [20010421]
 *   dpg: work for devfs large number of disks [20010809]
 *        use vmalloc() more inquiry+mode_sense [20020302]
 *        add timers for delayed responses [20020721]
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>

#include <asm/io.h>

#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"

#include <linux/stat.h>

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include "scsi_debug.h"

static const char * scsi_debug_version_str = "Version: 0.61 (20020815)";


#ifndef SCSI_CMD_READ_16
#define SCSI_CMD_READ_16 0x88
#endif
#ifndef SCSI_CMD_WRITE_16
#define SCSI_CMD_WRITE_16 0x8a
#endif
#ifndef REPORT_LUNS
#define REPORT_LUNS 0xa0
#endif

/* A few options that we want selected */
#define DEF_NR_FAKE_DEVS   1
#define DEF_DEV_SIZE_MB   8
#define DEF_FAKE_BLK0   0
#define DEF_EVERY_NTH   100
#define DEF_DELAY   1

#define DEF_OPTS   0
#define SCSI_DEBUG_OPT_NOISE   1
#define SCSI_DEBUG_OPT_MEDIUM_ERR   2
#define SCSI_DEBUG_OPT_EVERY_NTH   4

#define OPT_MEDIUM_ERR_ADDR   0x1234

static int scsi_debug_num_devs = DEF_NR_FAKE_DEVS;
static int scsi_debug_opts = DEF_OPTS;
static int scsi_debug_every_nth = DEF_EVERY_NTH;
static int scsi_debug_cmnd_count = 0;
static int scsi_debug_delay = DEF_DELAY;

#define NR_HOSTS_PRESENT (((scsi_debug_num_devs - 1) / 7) + 1)
#define N_HEAD          8
#define N_SECTOR        32
#define DEV_READONLY(TGT)      (0)
#define DEV_REMOVEABLE(TGT)    (0)
#define PERIPH_DEVICE_TYPE(TGT) (TYPE_DISK);

static int scsi_debug_dev_size_mb = DEF_DEV_SIZE_MB;
#define STORE_SIZE (scsi_debug_dev_size_mb * 1024 * 1024)

/* default sector size is 512 bytes, 2**9 bytes */
#define POW2_SECT_SIZE 9
#define SECT_SIZE (1 << POW2_SECT_SIZE)

#define N_CYLINDER (STORE_SIZE / (SECT_SIZE * N_SECTOR * N_HEAD))

/* Time to wait before completing a command */
#define CAPACITY (N_HEAD * N_SECTOR * N_CYLINDER)
#define SECT_SIZE_PER(TGT) SECT_SIZE


#define SDEBUG_SENSE_LEN 32

struct sdebug_dev_info {
	Scsi_Device * sdp;
	unsigned char sense_buff[SDEBUG_SENSE_LEN];	/* weak nexus */
	char reset;
};
static struct sdebug_dev_info * devInfop;

typedef void (* done_funct_t) (Scsi_Cmnd *);

struct sdebug_queued_cmd {
	int in_use;
	struct timer_list cmnd_timer;
	done_funct_t done_funct;
	struct scsi_cmnd * a_cmnd;
	int scsi_result;
};
static struct sdebug_queued_cmd queued_arr[SCSI_DEBUG_CANQUEUE];

static unsigned char * fake_storep;	/* ramdisk storage */

static unsigned char broken_buff[SDEBUG_SENSE_LEN];

static int num_aborts = 0;
static int num_dev_resets = 0;
static int num_bus_resets = 0;
static int num_host_resets = 0;

static spinlock_t queued_arr_lock = SPIN_LOCK_UNLOCKED;
static rwlock_t atomic_rw = RW_LOCK_UNLOCKED;


/* function declarations */
static int resp_inquiry(unsigned char * cmd, int target, unsigned char * buff,
			int bufflen, struct sdebug_dev_info * devip);
static int resp_mode_sense(unsigned char * cmd, int target,
			   unsigned char * buff, int bufflen,
			   struct sdebug_dev_info * devip);
static int resp_read(Scsi_Cmnd * SCpnt, int upper_blk, int block, 
		     int num, struct sdebug_dev_info * devip);
static int resp_write(Scsi_Cmnd * SCpnt, int upper_blk, int block, int num,
		      struct sdebug_dev_info * devip);
static int resp_report_luns(unsigned char * cmd, unsigned char * buff,
			    int bufflen, struct sdebug_dev_info * devip);
static void timer_intr_handler(unsigned long);
static struct sdebug_dev_info * devInfoReg(Scsi_Device * sdp);
static void mk_sense_buffer(struct sdebug_dev_info * devip, int key, 
			    int asc, int asq, int inbandLen);
static int check_reset(Scsi_Cmnd * SCpnt, struct sdebug_dev_info * devip);
static int schedule_resp(struct scsi_cmnd * cmnd, 
			 struct sdebug_dev_info * devip, 
			 done_funct_t done, int scsi_result, int delta_jiff);
static void init_all_queued(void);
static void stop_all_queued(void);
static int stop_queued_cmnd(struct scsi_cmnd * cmnd);
static int inquiry_evpd_83(unsigned char * arr, int dev_id_num,
                           const char * dev_id_str, int dev_id_str_len);


static Scsi_Host_Template driver_template = SCSI_DEBUG_TEMPLATE;


static
int scsi_debug_queuecommand(Scsi_Cmnd * SCpnt, done_funct_t done)
{
	unsigned char *cmd = (unsigned char *) SCpnt->cmnd;
	int block;
	int upper_blk;
	unsigned char *buff;
	int errsts = 0;
	int target = SCpnt->target;
	int bufflen = SCpnt->request_bufflen;
	int num, capac;
	struct sdebug_dev_info * devip = NULL;
	unsigned char * sbuff;

	if (done == NULL)
		return 0;	/* assume mid level reprocessing command */

	if (SCpnt->use_sg) { /* just use first element */
		struct scatterlist *sgpnt = (struct scatterlist *)
						SCpnt->request_buffer;

		buff = sgpnt[0].address;
		bufflen = sgpnt[0].length;
		/* READ and WRITE process scatterlist themselves */
	}
	else 
		buff = (unsigned char *) SCpnt->request_buffer;
	if (NULL == buff) {
		printk(KERN_WARNING "scsi_debug:qc: buff was NULL??\n");
		buff = broken_buff;	/* just point at dummy */
		bufflen = SDEBUG_SENSE_LEN;
	}

        if(target == driver_template.this_id) {
                printk(KERN_WARNING 
		       "scsi_debug: initiator's id used as target!\n");
		return schedule_resp(SCpnt, NULL, done, 0, 0);
        }

	if ((target > driver_template.this_id) || (SCpnt->lun != 0))
		return schedule_resp(SCpnt, NULL, done, 
				     DID_NO_CONNECT << 16, 0);
#if 0
	printk(KERN_INFO "sdebug:qc: host_no=%d, id=%d, sdp=%p, cmd=0x%x\n",
	       (int)SCpnt->device->host->host_no, (int)SCpnt->device->id,
	       SCpnt->device, (int)*cmd);
#endif
	if (NULL == SCpnt->device->hostdata) {
		devip = devInfoReg(SCpnt->device);
		if (NULL == devip)
			return schedule_resp(SCpnt, NULL, done, 
					     DID_NO_CONNECT << 16, 0);
		SCpnt->device->hostdata = devip;
	}
	devip = SCpnt->device->hostdata;

        if ((SCSI_DEBUG_OPT_EVERY_NTH & scsi_debug_opts) &&
            (scsi_debug_every_nth > 0) &&
            (++scsi_debug_cmnd_count >= scsi_debug_every_nth)) {
                scsi_debug_cmnd_count =0;
                return 0; /* ignore command causing timeout */
        }

	switch (*cmd) {
	case INQUIRY:     /* mandatory */
		/* assume INQUIRY called first so setup max_cmd_len */
		if (SCpnt->host->max_cmd_len != SCSI_DEBUG_MAX_CMD_LEN)
			SCpnt->host->max_cmd_len = SCSI_DEBUG_MAX_CMD_LEN;
		errsts = resp_inquiry(cmd, target, buff, bufflen, devip);
		break;
	case REQUEST_SENSE:	/* mandatory */
		/* Since this driver indicates autosense by placing the
		 * sense buffer in the scsi_cmnd structure in the response
		 * (when CHECK_CONDITION is set), the mid level shouldn't
		 * need to call REQUEST_SENSE */
		if (devip) {
			sbuff = devip->sense_buff;
			memcpy(buff, sbuff, (bufflen < SDEBUG_SENSE_LEN) ? 
					     bufflen : SDEBUG_SENSE_LEN);
			mk_sense_buffer(devip, 0, 0x0, 0, 7);
		} else {
			memset(buff, 0, bufflen);
			buff[0] = 0x70;
		}
		break;
	case START_STOP:
		errsts = check_reset(SCpnt, devip);
		break;
	case ALLOW_MEDIUM_REMOVAL:
		if ((errsts = check_reset(SCpnt, devip)))
			break;
		if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
			printk("\tMedium removal %s\n",
			       cmd[4] ? "inhibited" : "enabled");
		break;
	case SEND_DIAGNOSTIC:     /* mandatory */
		memset(buff, 0, bufflen);
		break;
	case TEST_UNIT_READY:     /* mandatory */
		memset(buff, 0, bufflen);
		break;
        case RESERVE:
		errsts = check_reset(SCpnt, devip);
		memset(buff, 0, bufflen);
                break;
        case RESERVE_10:
		errsts = check_reset(SCpnt, devip);
		memset(buff, 0, bufflen);
                break;
        case RELEASE:
		errsts = check_reset(SCpnt, devip);
		memset(buff, 0, bufflen);
                break;
        case RELEASE_10:
		errsts = check_reset(SCpnt, devip);
		memset(buff, 0, bufflen);
                break;
	case READ_CAPACITY:
		errsts = check_reset(SCpnt, devip);
		memset(buff, 0, bufflen);
		if (bufflen > 7) {
			capac = CAPACITY - 1;
			buff[0] = (capac >> 24);
			buff[1] = (capac >> 16) & 0xff;
			buff[2] = (capac >> 8) & 0xff;
			buff[3] = capac & 0xff;
			buff[6] = (SECT_SIZE_PER(target) >> 8) & 0xff;
			buff[7] = SECT_SIZE_PER(target) & 0xff;
		}
		break;
	case SCSI_CMD_READ_16:	/* SBC-2 */
	case READ_12:
	case READ_10:
	case READ_6:
		if ((errsts = check_reset(SCpnt, devip)))
			break;
		upper_blk = 0;
		if ((*cmd) == SCSI_CMD_READ_16) {
			upper_blk = cmd[5] + (cmd[4] << 8) + 
				    (cmd[3] << 16) + (cmd[2] << 24);
			block = cmd[9] + (cmd[8] << 8) + 
				(cmd[7] << 16) + (cmd[6] << 24);
			num = cmd[13] + (cmd[12] << 8) + 
				(cmd[11] << 16) + (cmd[10] << 24);
		} else if ((*cmd) == READ_12) {
			block = cmd[5] + (cmd[4] << 8) + 
				(cmd[3] << 16) + (cmd[2] << 24);
			num = cmd[9] + (cmd[8] << 8) + 
				(cmd[7] << 16) + (cmd[6] << 24);
		} else if ((*cmd) == READ_10) {
			block = cmd[5] + (cmd[4] << 8) + 
				(cmd[3] << 16) + (cmd[2] << 24);
			num = cmd[8] + (cmd[7] << 8);
		} else {
			block = cmd[3] + (cmd[2] << 8) + 
				((cmd[1] & 0x1f) << 16);
			num = cmd[4];
		}
		errsts = resp_read(SCpnt, upper_blk, block, num, devip);
		break;
	case REPORT_LUNS:
		errsts = resp_report_luns(cmd, buff, bufflen, devip);
		break;
	case SCSI_CMD_WRITE_16:	/* SBC-2 */
	case WRITE_12:
	case WRITE_10:
	case WRITE_6:
		if ((errsts = check_reset(SCpnt, devip)))
			break;
		upper_blk = 0;
		if ((*cmd) == SCSI_CMD_WRITE_16) {
			upper_blk = cmd[5] + (cmd[4] << 8) + 
				    (cmd[3] << 16) + (cmd[2] << 24);
			block = cmd[9] + (cmd[8] << 8) + 
				(cmd[7] << 16) + (cmd[6] << 24);
			num = cmd[13] + (cmd[12] << 8) + 
				(cmd[11] << 16) + (cmd[10] << 24);
		} else if ((*cmd) == WRITE_12) {
			block = cmd[5] + (cmd[4] << 8) + 
				(cmd[3] << 16) + (cmd[2] << 24);
			num = cmd[9] + (cmd[8] << 8) + 
				(cmd[7] << 16) + (cmd[6] << 24);
		} else if ((*cmd) == WRITE_10) {
			block = cmd[5] + (cmd[4] << 8) + 
				(cmd[3] << 16) + (cmd[2] << 24);
			num = cmd[8] + (cmd[7] << 8);
		} else {
			block = cmd[3] + (cmd[2] << 8) + 
				((cmd[1] & 0x1f) << 16);
			num = cmd[4];
		}
		errsts = resp_write(SCpnt, upper_blk, block, num, devip);
		break;
	case MODE_SENSE:
	case MODE_SENSE_10:
		errsts = resp_mode_sense(cmd, target, buff, bufflen, devip);
		break;
	default:
#if 0
		printk(KERN_INFO "scsi_debug: Unsupported command, "
		       "opcode=0x%x\n", (int)cmd[0]);
#endif
		if ((errsts = check_reset(SCpnt, devip)))
			break;
		mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x20, 0, 14);
		errsts = (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
		break;
	}
	return schedule_resp(SCpnt, devip, done, errsts, scsi_debug_delay);
}

static int scsi_debug_ioctl(Scsi_Device *dev, int cmd, void *arg)
{
	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts) {
		printk(KERN_INFO "scsi_debug: ioctl: cmd=0x%x\n", cmd);
	}
	/* return -ENOTTY; // Unix mandates this but apps get upset */
	return -EINVAL;
}

static int check_reset(Scsi_Cmnd * SCpnt, struct sdebug_dev_info * devip)
{
	if (devip->reset) {
		devip->reset = 0;
		mk_sense_buffer(devip, UNIT_ATTENTION, 0x29, 0, 14);
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	}
	return 0;
}

#define SDEBUG_LONG_INQ_SZ 58
#define SDEBUG_MAX_INQ_ARR_SZ 128

static const char * vendor_id = "Linux   ";
static const char * product_id = "scsi_debug      ";
static const char * product_rev = "0004";

static int inquiry_evpd_83(unsigned char * arr, int dev_id_num, 
			   const char * dev_id_str, int dev_id_str_len)
{
	int num;

	/* Two identification descriptors: */
	/* T10 vendor identifier field format (faked) */
	arr[0] = 0x2;	/* ASCII */
	arr[1] = 0x1;
	arr[2] = 0x0;
	memcpy(&arr[4], vendor_id, 8);
	memcpy(&arr[12], product_id, 16);
	memcpy(&arr[28], dev_id_str, dev_id_str_len);
	num = 8 + 16 + dev_id_str_len;
	arr[3] = num;
	num += 4;
	/* NAA IEEE registered identifier (faked) */
	arr[num] = 0x1;	/* binary */
	arr[num + 1] = 0x3;
	arr[num + 2] = 0x0;
	arr[num + 3] = 0x8;
	arr[num + 4] = 0x51;	/* ieee company id=0x123456 (faked) */
	arr[num + 5] = 0x23;
	arr[num + 6] = 0x45;
	arr[num + 7] = 0x60;
	arr[num + 8] = (dev_id_num >> 24);
	arr[num + 9] = (dev_id_num >> 16) & 0xff;
	arr[num + 10] = (dev_id_num >> 8) & 0xff;
	arr[num + 11] = dev_id_num & 0xff;
	return num + 12;
}

static int resp_inquiry(unsigned char * cmd, int target, unsigned char * buff,
			int bufflen, struct sdebug_dev_info * devip)
{
	unsigned char pq_pdt;
	unsigned char arr[SDEBUG_MAX_INQ_ARR_SZ];
	int min_len = bufflen > SDEBUG_MAX_INQ_ARR_SZ ? 
			SDEBUG_MAX_INQ_ARR_SZ : bufflen;

	if (bufflen < cmd[4])
		printk(KERN_INFO "scsi_debug: inquiry: bufflen=%d "
		       "< alloc_length=%d\n", bufflen, (int)cmd[4]);
	memset(buff, 0, bufflen);
	memset(arr, 0, SDEBUG_MAX_INQ_ARR_SZ);
	pq_pdt = PERIPH_DEVICE_TYPE(target);
	arr[0] = pq_pdt;
	if (0x2 & cmd[1]) {  /* CMDDT bit set */
		mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x24, 0, 14);
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	} else if (0x1 & cmd[1]) {  /* EVPD bit set */
		int dev_id_num, len;
		char dev_id_str[6];
		
		dev_id_num = ((devip->sdp->host->host_no + 1) * 1000) + 
			      devip->sdp->id;
		len = snprintf(dev_id_str, 6, "%d", dev_id_num);
		len = (len > 6) ? 6 : len;
		if (0 == cmd[2]) { /* supported vital product data pages */
			arr[3] = 3;
			arr[4] = 0x0; /* this page */
			arr[5] = 0x80; /* unit serial number */
			arr[6] = 0x83; /* device identification */
		} else if (0x80 == cmd[2]) { /* unit serial number */
			arr[1] = 0x80;
			arr[3] = len;
			memcpy(&arr[4], dev_id_str, len);
		} else if (0x83 == cmd[2]) { /* device identification */
			arr[1] = 0x83;
			arr[3] = inquiry_evpd_83(&arr[4], dev_id_num,
						 dev_id_str, len);
		} else {
			/* Illegal request, invalid field in cdb */
			mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x24, 0, 14);
			return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
		}
		memcpy(buff, arr, min_len); 
		return 0;
	}
	/* drops through here for a standard inquiry */
	arr[1] = DEV_REMOVEABLE(target) ? 0x80 : 0;	/* Removable disk */
	arr[2] = 3;	/* claim SCSI 3 */
	arr[4] = SDEBUG_LONG_INQ_SZ - 5;
	arr[7] = 0x3a; /* claim: WBUS16, SYNC, LINKED + CMDQUE */
	memcpy(&arr[8], vendor_id, 8);
	memcpy(&arr[16], product_id, 16);
	memcpy(&arr[32], product_rev, 4);
	memcpy(buff, arr, min_len);
	return 0;
}

/* <<Following mode page info copied from ST318451LW>> */ 

static int resp_err_recov_pg(unsigned char * p, int pcontrol, int target)
{	/* Read-Write Error Recovery page for mode_sense */
	unsigned char err_recov_pg[] = {0x1, 0xa, 0xc0, 11, 240, 0, 0, 0, 
					5, 0, 0xff, 0xff};

	memcpy(p, err_recov_pg, sizeof(err_recov_pg));
	if (1 == pcontrol)
		memset(p + 2, 0, sizeof(err_recov_pg) - 2);
	return sizeof(err_recov_pg);
}

static int resp_disconnect_pg(unsigned char * p, int pcontrol, int target)
{ 	/* Disconnect-Reconnect page for mode_sense */
	unsigned char disconnect_pg[] = {0x2, 0xe, 128, 128, 0, 10, 0, 0, 
					 0, 0, 0, 0, 0, 0, 0, 0};

	memcpy(p, disconnect_pg, sizeof(disconnect_pg));
	if (1 == pcontrol)
		memset(p + 2, 0, sizeof(disconnect_pg) - 2);
	return sizeof(disconnect_pg);
}

static int resp_format_pg(unsigned char * p, int pcontrol, int target)
{       /* Format device page for mode_sense */
        unsigned char format_pg[] = {0x3, 0x16, 0, 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0x40, 0, 0, 0};

        memcpy(p, format_pg, sizeof(format_pg));
        p[10] = (N_SECTOR >> 8) & 0xff;
        p[11] = N_SECTOR & 0xff;
        p[12] = (SECT_SIZE >> 8) & 0xff;
        p[13] = SECT_SIZE & 0xff;
        if (DEV_REMOVEABLE(target))
                p[20] |= 0x20; /* should agree with INQUIRY */
        if (1 == pcontrol)
                memset(p + 2, 0, sizeof(format_pg) - 2);
        return sizeof(format_pg);
}

static int resp_caching_pg(unsigned char * p, int pcontrol, int target)
{ 	/* Caching page for mode_sense */
	unsigned char caching_pg[] = {0x8, 18, 0x14, 0, 0xff, 0xff, 0, 0, 
		0xff, 0xff, 0xff, 0xff, 0x80, 0x14, 0, 0,     0, 0, 0, 0};

	memcpy(p, caching_pg, sizeof(caching_pg));
	if (1 == pcontrol)
		memset(p + 2, 0, sizeof(caching_pg) - 2);
	return sizeof(caching_pg);
}

static int resp_ctrl_m_pg(unsigned char * p, int pcontrol, int target)
{ 	/* Control mode page for mode_sense */
	unsigned char ctrl_m_pg[] = {0xa, 10, 2, 0, 0, 0, 0, 0,
				     0, 0, 0x2, 0x4b};

	memcpy(p, ctrl_m_pg, sizeof(ctrl_m_pg));
	if (1 == pcontrol)
		memset(p + 2, 0, sizeof(ctrl_m_pg) - 2);
	return sizeof(ctrl_m_pg);
}


#define SDEBUG_MAX_MSENSE_SZ 256

static int resp_mode_sense(unsigned char * cmd, int target,
			   unsigned char * buff, int bufflen,
			   struct sdebug_dev_info * devip)
{
	unsigned char dbd;
	int pcontrol, pcode;
	unsigned char dev_spec;
	int alloc_len, msense_6, offset, len;
	unsigned char * ap;
	unsigned char arr[SDEBUG_MAX_MSENSE_SZ];
	int min_len = bufflen > SDEBUG_MAX_MSENSE_SZ ? 
			SDEBUG_MAX_MSENSE_SZ : bufflen;

	SCSI_LOG_LLQUEUE(3, printk("Mode sense ...(%p %d)\n", buff, bufflen));
	dbd = cmd[1] & 0x8;
	pcontrol = (cmd[2] & 0xc0) >> 6;
	pcode = cmd[2] & 0x3f;
	msense_6 = (MODE_SENSE == cmd[0]);
	alloc_len = msense_6 ? cmd[4] : ((cmd[7] << 8) | cmd[6]);
	/* printk(KERN_INFO "msense: dbd=%d pcontrol=%d pcode=%d "
		"msense_6=%d alloc_len=%d\n", dbd, pcontrol, pcode, "
		"msense_6, alloc_len); */
	if (bufflen < alloc_len)
		printk(KERN_INFO "scsi_debug: mode_sense: bufflen=%d "
		       "< alloc_length=%d\n", bufflen, alloc_len);
	memset(buff, 0, bufflen);
	memset(arr, 0, SDEBUG_MAX_MSENSE_SZ);
	if (0x3 == pcontrol) {  /* Saving values not supported */
		mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x39, 0, 14);
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	}
	dev_spec = DEV_READONLY(target) ? 0x80 : 0x0;
	if (msense_6) {
		arr[2] = dev_spec;
		offset = 4;
	} else {
		arr[3] = dev_spec;
		offset = 8;
	}
	ap = arr + offset;

	switch (pcode) {
	case 0x1:	/* Read-Write error recovery page, direct access */
		len = resp_err_recov_pg(ap, pcontrol, target);
		offset += len;
		break;
	case 0x2:	/* Disconnect-Reconnect page, all devices */
		len = resp_disconnect_pg(ap, pcontrol, target);
		offset += len;
		break;
        case 0x3:       /* Format device page, direct access */
                len = resp_format_pg(ap, pcontrol, target);
                offset += len;
                break;
	case 0x8:	/* Caching page, direct access */
		len = resp_caching_pg(ap, pcontrol, target);
		offset += len;
		break;
	case 0xa:	/* Control Mode page, all devices */
		len = resp_ctrl_m_pg(ap, pcontrol, target);
		offset += len;
		break;
	case 0x3f:	/* Read all Mode pages */
		len = resp_err_recov_pg(ap, pcontrol, target);
		len += resp_disconnect_pg(ap + len, pcontrol, target);
		len += resp_caching_pg(ap + len, pcontrol, target);
		len += resp_ctrl_m_pg(ap + len, pcontrol, target);
		offset += len;
		break;
	default:
		mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x24, 0, 14);
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	}
	if (msense_6)
		arr[0] = offset - 1;
	else {
		offset -= 2;
		arr[0] = (offset >> 8) & 0xff; 
		arr[1] = offset & 0xff; 
	}
	memcpy(buff, arr, min_len);
	return 0;
}

static int resp_read(Scsi_Cmnd * SCpnt, int upper_blk, int block, int num, 
		     struct sdebug_dev_info * devip)
{
        unsigned char *buff = (unsigned char *) SCpnt->request_buffer;
        int nbytes, sgcount;
        struct scatterlist *sgpnt = NULL;
        int bufflen = SCpnt->request_bufflen;
	unsigned long iflags;

	if (upper_blk || (block + num > CAPACITY)) {
		mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x21, 0, 14);
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	}
	if ((SCSI_DEBUG_OPT_MEDIUM_ERR & scsi_debug_opts) &&
	    (block >= OPT_MEDIUM_ERR_ADDR) && 
	    (block < (OPT_MEDIUM_ERR_ADDR + num))) {
		mk_sense_buffer(devip, MEDIUM_ERROR, 0x11, 0, 14);
		/* claim unrecoverable read error */
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	}
	read_lock_irqsave(&atomic_rw, iflags);
        sgcount = 0;
	nbytes = bufflen;
	/* printk(KERN_INFO "scsi_debug_read: block=%d, tot_bufflen=%d\n", 
	       block, bufflen); */
	if (SCpnt->use_sg) {
		sgcount = 0;
		sgpnt = (struct scatterlist *) buff;
		buff = sgpnt[sgcount].address;
		bufflen = sgpnt[sgcount].length;
	}
	do {
		memcpy(buff, fake_storep + (block * SECT_SIZE), bufflen);
		nbytes -= bufflen;
		if (SCpnt->use_sg) {
			block += bufflen >> POW2_SECT_SIZE;
			sgcount++;
			if (nbytes) {
				buff = sgpnt[sgcount].address;
				bufflen = sgpnt[sgcount].length;
			}
		} else if (nbytes > 0)
			printk(KERN_WARNING "scsi_debug:resp_read: unexpected "
			       "nbytes=%d\n", nbytes);
	} while (nbytes);
	read_unlock_irqrestore(&atomic_rw, iflags);
	return 0;
}

static int resp_write(Scsi_Cmnd * SCpnt, int upper_blk, int block, int num, 
		      struct sdebug_dev_info * devip)
{
        unsigned char *buff = (unsigned char *) SCpnt->request_buffer;
        int nbytes, sgcount;
        struct scatterlist *sgpnt = NULL;
        int bufflen = SCpnt->request_bufflen;
	unsigned long iflags;

	if (upper_blk || (block + num > CAPACITY)) {
		mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x21, 0, 14);
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	}

	write_lock_irqsave(&atomic_rw, iflags);
        sgcount = 0;
	nbytes = bufflen;
	if (SCpnt->use_sg) {
		sgcount = 0;
		sgpnt = (struct scatterlist *) buff;
		buff = sgpnt[sgcount].address;
		bufflen = sgpnt[sgcount].length;
	}
	do {
		memcpy(fake_storep + (block * SECT_SIZE), buff, bufflen);

		nbytes -= bufflen;
		if (SCpnt->use_sg) {
			block += bufflen >> POW2_SECT_SIZE;
			sgcount++;
			if (nbytes) {
				buff = sgpnt[sgcount].address;
				bufflen = sgpnt[sgcount].length;
			}
		} else if (nbytes > 0)
			printk(KERN_WARNING "scsi_debug:resp_write: "
			       "unexpected nbytes=%d\n", nbytes);
	} while (nbytes);
	write_unlock_irqrestore(&atomic_rw, iflags);
	return 0;
}

static int resp_report_luns(unsigned char * cmd, unsigned char * buff,
			    int bufflen, struct sdebug_dev_info * devip)
{
	unsigned int alloc_len;
	int select_report = (int)cmd[2];

	alloc_len = cmd[9] + (cmd[8] << 8) + (cmd[7] << 16) + (cmd[6] << 24);
	if ((alloc_len < 16) || (select_report > 2)) {
		mk_sense_buffer(devip, ILLEGAL_REQUEST, 0x24, 0, 14);
		return (COMMAND_COMPLETE << 8) | (CHECK_CONDITION << 1);
	}
	if (bufflen > 3) {
		memset(buff, 0, bufflen);
		buff[3] = 8;
	}
	return 0;
}

/* When timer goes off this function is called. */
static void timer_intr_handler(unsigned long indx)
{
	struct sdebug_queued_cmd * sqcp;
	unsigned int iflags;

	if (indx >= SCSI_DEBUG_CANQUEUE) {
		printk(KERN_ERR "scsi_debug:timer_intr_handler: indx too "
		       "large\n");
		return;
	}
	spin_lock_irqsave(&queued_arr_lock, iflags);
	sqcp = &queued_arr[(int)indx];
	if (! sqcp->in_use) {
		printk(KERN_ERR "scsi_debug:timer_intr_handler: Unexpected "
		       "interrupt\n");
		spin_unlock_irqrestore(&queued_arr_lock, iflags);
		return;
	}
	sqcp->in_use = 0;
	if (sqcp->done_funct)
		sqcp->done_funct(sqcp->a_cmnd); /* callback to mid level */
	sqcp->done_funct = NULL;
	spin_unlock_irqrestore(&queued_arr_lock, iflags);
}

static int initialized = 0;
static int num_present = 0;
static const char * sdebug_proc_name = "scsi_debug";

static int scsi_debug_detect(Scsi_Host_Template * tpnt)
{
	int k, sz;

	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
		printk(KERN_INFO "scsi_debug: detect\n");
	if (0 == initialized) {
		++initialized;
		sz = sizeof(struct sdebug_dev_info) * scsi_debug_num_devs;
		devInfop = vmalloc(sz);
		if (NULL == devInfop) {
			printk(KERN_ERR "scsi_debug_detect: out of "
			       "memory, 0.5\n");
			return 0;
		}
		memset(devInfop, 0, sz);
		sz = STORE_SIZE;
		fake_storep = vmalloc(sz);
		if (NULL == fake_storep) {
			printk(KERN_ERR "scsi_debug_detect: out of memory"
			       ", 0\n");
			return 0;
		}
		memset(fake_storep, 0, sz);
		init_all_queued();
		tpnt->proc_name = (char *)sdebug_proc_name;
		for (num_present = 0, k = 0; k < NR_HOSTS_PRESENT; k++) {
			if (NULL == scsi_register(tpnt, 0))
				printk(KERN_ERR "scsi_debug_detect: "
					"scsi_register failed k=%d\n", k);
			else
				++num_present;
		}
		return num_present;
	} else {
		printk(KERN_WARNING "scsi_debug_detect: called again\n");
		return 0;
	}
}


static int num_releases = 0;

static int scsi_debug_release(struct Scsi_Host * hpnt)
{
	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
		printk(KERN_INFO "scsi_debug: release\n");
	stop_all_queued();
	scsi_unregister(hpnt);
	if (++num_releases == num_present) {
		vfree(fake_storep);
		vfree(devInfop);
	}
	return 0;
}

static struct sdebug_dev_info * devInfoReg(Scsi_Device * sdp)
{
	int k;
	struct sdebug_dev_info * devip;

	for (k = 0; k < scsi_debug_num_devs; ++k) {
		devip = &devInfop[k];
		if (devip->sdp == sdp)
			return devip;
	}
	for (k = 0; k < scsi_debug_num_devs; ++k) {
		devip = &devInfop[k];
		if (NULL == devip->sdp) {
			devip->sdp = sdp;
			devip->reset = 1;
			memset(devip->sense_buff, 0, SDEBUG_SENSE_LEN);
			devip->sense_buff[0] = 0x70;
			return devip;
		}
	}
	return NULL;
}

static void mk_sense_buffer(struct sdebug_dev_info * devip, int key, 
			    int asc, int asq, int inbandLen)
{
	unsigned char * sbuff;

	sbuff = devip->sense_buff;
	memset(sbuff, 0, SDEBUG_SENSE_LEN);
	if (inbandLen > SDEBUG_SENSE_LEN)
		inbandLen = SDEBUG_SENSE_LEN;
	sbuff[0] = 0x70;
	sbuff[2] = key;
	sbuff[7] = (inbandLen > 7) ? (inbandLen - 8) : 0;
	sbuff[12] = asc;
	sbuff[13] = asq;
}

static int scsi_debug_abort(Scsi_Cmnd * SCpnt)
{
	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
		printk(KERN_INFO "scsi_debug: abort\n");
	++num_aborts;
	stop_queued_cmnd(SCpnt);
	return SUCCESS;
}

static int scsi_debug_biosparam(Disk * disk, kdev_t dev, int *info)
{
	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
		printk(KERN_INFO "scsi_debug: biosparam\n");
	/* int size = disk->capacity; */
	info[0] = N_HEAD;
	info[1] = N_SECTOR;
	info[2] = N_CYLINDER;
	if (info[2] >= 1024)
		info[2] = 1024;
	return 0;
}

static int scsi_debug_device_reset(Scsi_Cmnd * SCpnt)
{
	Scsi_Device * sdp;
	int k;

	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
		printk(KERN_INFO "scsi_debug: device_reset\n");
	++num_dev_resets;
	if (SCpnt && ((sdp = SCpnt->device))) {
		for (k = 0; k < scsi_debug_num_devs; ++k) {
			if (sdp->hostdata == (devInfop + k))
				break;
		}
		if (k < scsi_debug_num_devs)
			devInfop[k].reset = 1;
	}
	return SUCCESS;
}

static int scsi_debug_bus_reset(Scsi_Cmnd * SCpnt)
{
	Scsi_Device * sdp;
	struct Scsi_Host * hp;
	int k;

	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
		printk(KERN_INFO "scsi_debug: bus_reset\n");
	++num_bus_resets;
	if (SCpnt && ((sdp = SCpnt->device)) && ((hp = sdp->host))) {
		for (k = 0; k < scsi_debug_num_devs; ++k) {
			if (hp == devInfop[k].sdp->host)
				devInfop[k].reset = 1;
		}
	}
	return SUCCESS;
}

static int scsi_debug_host_reset(Scsi_Cmnd * SCpnt)
{
	int k;

	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts)
		printk(KERN_INFO "scsi_debug: host_reset\n");
	++num_host_resets;
	for (k = 0; k < scsi_debug_num_devs; ++k)
		devInfop[k].reset = 1;
	stop_all_queued();

	return SUCCESS;
}

/* Returns 1 if found 'cmnd' and deleted its timer. else returns 0 */
static int stop_queued_cmnd(struct scsi_cmnd * cmnd)
{
	unsigned long iflags;
	int k;
	struct sdebug_queued_cmd * sqcp;

	spin_lock_irqsave(&queued_arr_lock, iflags);
	for (k = 0; k < SCSI_DEBUG_CANQUEUE; ++k) {
		sqcp = &queued_arr[k];
		if (sqcp->in_use && (cmnd == sqcp->a_cmnd)) {
			del_timer_sync(&sqcp->cmnd_timer);
			sqcp->in_use = 0;
			sqcp->a_cmnd = NULL;
			break;
		}
	}
	spin_unlock_irqrestore(&queued_arr_lock, iflags);
	return (k < SCSI_DEBUG_CANQUEUE) ? 1 : 0;
}

/* Deletes (stops) timers of all queued commands */
static void stop_all_queued(void)
{
	unsigned long iflags;
	int k;
	struct sdebug_queued_cmd * sqcp;

	spin_lock_irqsave(&queued_arr_lock, iflags);
	for (k = 0; k < SCSI_DEBUG_CANQUEUE; ++k) {
		sqcp = &queued_arr[k];
		if (sqcp->in_use && sqcp->a_cmnd) {
			del_timer_sync(&sqcp->cmnd_timer);
			sqcp->in_use = 0;
			sqcp->a_cmnd = NULL;
		}
	}
	spin_unlock_irqrestore(&queued_arr_lock, iflags);
}

/* Initializes timers in queued array */
static void init_all_queued(void)
{
	unsigned long iflags;
	int k;
	struct sdebug_queued_cmd * sqcp;

	spin_lock_irqsave(&queued_arr_lock, iflags);
	for (k = 0; k < SCSI_DEBUG_CANQUEUE; ++k) {
		sqcp = &queued_arr[k];
		init_timer(&sqcp->cmnd_timer);
		sqcp->in_use = 0;
		sqcp->a_cmnd = NULL;
	}
	spin_unlock_irqrestore(&queued_arr_lock, iflags);
}

static int schedule_resp(struct scsi_cmnd * cmnd, 
			 struct sdebug_dev_info * devip,
			 done_funct_t done, int scsi_result, int delta_jiff)
{
	int k, num; 

	if (SCSI_DEBUG_OPT_NOISE & scsi_debug_opts) {
		printk(KERN_INFO "scsi_debug: cmd ");
		for (k = 0, num = cmnd->cmd_len; k < num; ++k)
	            printk("%02x ", (int)cmnd->cmnd[k]);
		printk("result=0x%x\n", scsi_result);
	}
	if (cmnd && devip) {
		/* simulate autosense by this driver */
		if (CHECK_CONDITION == status_byte(scsi_result))
			memcpy(cmnd->sense_buffer, devip->sense_buff, 
			       (SCSI_SENSE_BUFFERSIZE > SDEBUG_SENSE_LEN) ?
			       SDEBUG_SENSE_LEN : SCSI_SENSE_BUFFERSIZE);
	}
	if (delta_jiff <= 0) {
		if (cmnd)
			cmnd->result = scsi_result;
		if (done)
			done(cmnd);
		return 0;
	} else {
		unsigned long iflags;
		int k;
		struct sdebug_queued_cmd * sqcp = NULL;

		spin_lock_irqsave(&queued_arr_lock, iflags);
		for (k = 0; k < SCSI_DEBUG_CANQUEUE; ++k) {
			sqcp = &queued_arr[k];
			if (! sqcp->in_use)
				break;
		}
		if (k >= SCSI_DEBUG_CANQUEUE) {
			spin_unlock_irqrestore(&queued_arr_lock, iflags);
			printk(KERN_WARNING "scsi_debug: can_queue exceeded\n");
			return 1;	/* report busy to mid level */
		}
		sqcp->in_use = 1;
		sqcp->a_cmnd = cmnd;
		sqcp->scsi_result = scsi_result;
		sqcp->done_funct = done;
		sqcp->cmnd_timer.function = timer_intr_handler;
		sqcp->cmnd_timer.data = k;
		sqcp->cmnd_timer.expires = jiffies + delta_jiff;
		add_timer(&sqcp->cmnd_timer);
		spin_unlock_irqrestore(&queued_arr_lock, iflags);
		return 0;
	}
}

#ifndef MODULE
static int __init num_devs_setup(char *str)
{   
    int tmp; 
    
    if (get_option(&str, &tmp) == 1) {
        if (tmp > 0)
            scsi_debug_num_devs = tmp;
        return 1;
    } else {
        printk(KERN_INFO "scsi_debug_num_devs: usage scsi_debug_num_devs=<n> "
               "(<n> can be from 1 to around 2000)\n");
        return 0;
    }
}
__setup("scsi_debug_num_devs=", num_devs_setup);

static int __init dev_size_mb_setup(char *str)
{   
    int tmp; 
    
    if (get_option(&str, &tmp) == 1) {
        if (tmp > 0)
            scsi_debug_dev_size_mb = tmp;
        return 1;
    } else {
        printk(KERN_INFO "scsi_debug_dev_size_mb: usage "
	       "scsi_debug_dev_size_mb=<n>\n"
               "    (<n> is number of MB ram shared by all devs\n");
        return 0;
    }
}
__setup("scsi_debug_dev_size_mb=", dev_size_mb_setup);

static int __init opts_setup(char *str)
{   
    int tmp; 
    
    if (get_option(&str, &tmp) == 1) {
        if (tmp > 0)
            scsi_debug_opts = tmp;
        return 1;
    } else {
        printk(KERN_INFO "scsi_debug_opts: usage "
	       "scsi_debug_opts=<n>\n"
               "    (1->noise, 2->medium_error, 4->... (can be or-ed)\n");
        return 0;
    }
}
__setup("scsi_debug_opts=", opts_setup);

static int __init every_nth_setup(char *str)
{
    int tmp;

    if (get_option(&str, &tmp) == 1) {
        if (tmp > 0)
            scsi_debug_every_nth = tmp;
        return 1;
    } else {
        printk(KERN_INFO "scsi_debug_every_nth: usage "
               "scsi_debug_every_nth=<n>\n"
               "    timeout every nth command (when ...)\n");
        return 0;
    }
}
__setup("scsi_debug_every_nth=", every_nth_setup);

static int __init delay_setup(char *str)
{
    int tmp;

    if (get_option(&str, &tmp) == 1) {
	scsi_debug_delay = tmp;
        return 1;
    } else {
        printk(KERN_INFO "scsi_debug_delay: usage "
               "scsi_debug_delay=<n>\n"
               "    delay response <n> jiffies\n");
        return 0;
    }
}
__setup("scsi_debug_delay=", delay_setup);

#endif

MODULE_AUTHOR("Eric Youngdale + Douglas Gilbert");
MODULE_DESCRIPTION("SCSI debug adapter driver");
MODULE_PARM(scsi_debug_num_devs, "i");
MODULE_PARM_DESC(scsi_debug_num_devs, "number of SCSI devices to simulate");
MODULE_PARM(scsi_debug_dev_size_mb, "i");
MODULE_PARM_DESC(scsi_debug_dev_size_mb, "size in MB of ram shared by devs");
MODULE_PARM(scsi_debug_opts, "i");
MODULE_PARM_DESC(scsi_debug_opts, "1->noise, 2->medium_error, 4->...");
MODULE_PARM(scsi_debug_every_nth, "i");
MODULE_PARM_DESC(scsi_debug_every_nth, "timeout every nth command(def=100)");
MODULE_PARM(scsi_debug_delay, "i");
MODULE_PARM_DESC(scsi_debug_delay, "# of jiffies to delay response(def=1)");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

static char sdebug_info[256];

static const char * scsi_debug_info(struct Scsi_Host * shp)
{
	sprintf(sdebug_info, "scsi_debug, %s, num_devs=%d, "
		"dev_size_mb=%d, opts=0x%x", scsi_debug_version_str,
		scsi_debug_num_devs, scsi_debug_dev_size_mb,
		scsi_debug_opts);
	return sdebug_info;
}

/* scsi_debug_proc_info
 * Used if the driver currently has no own support for /proc/scsi
 */
static int scsi_debug_proc_info(char *buffer, char **start, off_t offset,
				int length, int inode, int inout)
{
	int len, pos, begin;
	int orig_length;

	orig_length = length;

	if (inout == 1) {
		char arr[16];
		int minLen = length > 15 ? 15 : length;

		if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
			return -EACCES;
		memcpy(arr, buffer, minLen);
		arr[minLen] = '\0';
		if (1 != sscanf(arr, "%d", &pos))
			return -EINVAL;
		scsi_debug_opts = pos;
		if (SCSI_DEBUG_OPT_EVERY_NTH & scsi_debug_opts)
                        scsi_debug_cmnd_count = 0;
		return length;
	}
	begin = 0;
	pos = len = sprintf(buffer, "scsi_debug adapter driver, %s\n"
	    "num_devs=%d, shared (ram) size=%d MB, opts=0x%x, "
	    "every_nth=%d(curr:%d)\n"
	    "sector_size=%d bytes, cylinders=%d, heads=%d, sectors=%d, "
	    "delay=%d\nnumber of aborts=%d, device_reset=%d, bus_resets=%d, " 
	    "host_resets=%d\n",
	    scsi_debug_version_str, scsi_debug_num_devs, 
	    scsi_debug_dev_size_mb, scsi_debug_opts, scsi_debug_every_nth,
	    scsi_debug_cmnd_count,
	    SECT_SIZE, N_CYLINDER, N_HEAD, N_SECTOR, scsi_debug_delay,
	    num_aborts, num_dev_resets, num_bus_resets, num_host_resets);
	if (pos < offset) {
		len = 0;
		begin = pos;
	}
	*start = buffer + (offset - begin);	/* Start of wanted data */
	len -= (offset - begin);
	if (len > length)
		len = length;

	return (len);
}

#include "scsi_module.c"
