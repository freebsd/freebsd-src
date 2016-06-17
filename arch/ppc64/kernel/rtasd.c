/*
 * Copyright (C) 2001 Anton Blanchard <anton@au.ibm.com>, IBM
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Communication to userspace based on kernel/printk.c
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/rtas.h>
#include <asm/prom.h>
#include <asm/nvram.h>
#include <asm/atomic.h>

#if 0
#define DEBUG(A...)	printk(KERN_ERR A)
#else
#define DEBUG(A...)
#endif

static spinlock_t log_lock = SPIN_LOCK_UNLOCKED;

DECLARE_WAIT_QUEUE_HEAD(rtas_log_wait);

static char *rtas_log_buf;
static unsigned long rtas_log_start;
static unsigned long rtas_log_size;

static int surveillance_timeout = -1;
static unsigned int rtas_event_scan_rate;
static unsigned int rtas_error_log_max;

static unsigned int rtas_error_log_buffer_max;

extern struct proc_dir_entry *proc_ppc64_root;
extern struct proc_dir_entry *rtas_proc_dir;
extern spinlock_t proc_ppc64_lock;
extern volatile int no_more_logging;

volatile int error_log_cnt = 0;

/*
 * Since we use 32 bit RTAS, the physical address of this must be below
 * 4G or else bad things happen. Allocate this in the kernel data and
 * make it big enough.
 */
static unsigned char logdata[RTAS_ERROR_LOG_MAX];

/* To see this info, grep RTAS /var/log/messages and each entry
 * will be collected together with obvious begin/end.
 * There will be a unique identifier on the begin and end lines.
 * This will persist across reboots.
 *
 * format of error logs returned from RTAS:
 * bytes 	(size)	: contents
 * --------------------------------------------------------
 * 0-7		(8)	: rtas_error_log
 * 8-47		(40) 	: extended info
 * 48-51	(4)	: vendor id
 * 52-1023 (vendor specific) : location code and debug data
 */
static void printk_log_rtas(char *buf, int len)
{

	int i,j,n;
	int perline = 16;
	char buffer[64];
	char * str = "RTAS event";

	printk(RTAS_ERR "%d -------- %s begin --------\n", error_log_cnt, str);

	/*
	 * Print perline bytes on each line, each line will start
	 * with RTAS and a changing number, so syslogd will
	 * print lines that are otherwise the same.  Separate every
	 * 4 bytes with a space.
	 */
	for (i=0; i < len; i++) {
		j = i % perline;
		if (j == 0) {
			memset(buffer, 0, sizeof(buffer));
			n = sprintf(buffer, "RTAS %d:", i/perline);
		}

		if ((i % 4) == 0)
			n += sprintf(buffer+n, " ");

		n += sprintf(buffer+n, "%02x", (unsigned char)buf[i]);

		if (j == (perline-1))
			printk(KERN_ERR "%s\n", buffer);
	}
	if ((i % perline) != 0)
		printk(KERN_ERR "%s\n", buffer);

	printk(RTAS_ERR "%d -------- %s end ----------\n", error_log_cnt, str);
}

static int log_rtas_len(char * buf)
{
	int len;
	struct rtas_error_log *err;

	/* rtas fixed header */
	len = 8;
	err = (struct rtas_error_log *)buf;
	if (err->extended_log_length) {

		/* extended header */
		len += err->extended_log_length;

		if (len > RTAS_ERROR_LOG_MAX)
			len = RTAS_ERROR_LOG_MAX;
	}
	return len;
}

/*
 * First write to nvram, if fatal error, that is the only
 * place we log the info.  The error will be picked up
 * on the next reboot by rtasd.  If not fatal, run the
 * method for the type of error.  Currently, only RTAS
 * errors have methods implemented, but in the future
 * there might be a need to store data in nvram before a
 * call to panic().
*/
void pSeries_log_error(char *buf, unsigned int err_type, int fatal)
{
	unsigned long offset;
	unsigned long s;
	int len = 0;

	DEBUG("logging event\n");

	if (buf == NULL)
		return;

	spin_lock_irqsave(&log_lock, s);

	/* get length and increase count */
	switch (err_type & ERR_TYPE_MASK) {
	case ERR_TYPE_RTAS_LOG:
		len = log_rtas_len(buf);
		if (!(err_type & ERR_FLAG_BOOT))
			error_log_cnt++;
		break;
	case ERR_TYPE_KERNEL_PANIC:
	default:
		spin_unlock_irqrestore(&log_lock, s);
		return;
	}

	/* Write error to NVRAM */
	if (!no_more_logging && !(err_type & ERR_FLAG_BOOT)) {
		write_error_log_nvram(buf, len, err_type);
	}

	/* Check to see if we need to or have stopped logging */
	if (fatal || no_more_logging) {
		no_more_logging = 1;
		spin_unlock_irqrestore(&log_lock, s);
		return;
	}

	/* call type specific method for error */
	switch (err_type & ERR_TYPE_MASK) {
	case ERR_TYPE_RTAS_LOG:
		/* put into syslog and error_log file */
		printk_log_rtas(buf, len);

		offset = rtas_error_log_buffer_max *
			((rtas_log_start+rtas_log_size) & LOG_NUMBER_MASK);

		/* First copy over sequence number */
		memcpy(&rtas_log_buf[offset], &error_log_cnt, sizeof(int));

		/* Second copy over error log data */
		offset += sizeof(int);
		memcpy(&rtas_log_buf[offset], buf, len);

		if (rtas_log_size < LOG_NUMBER)
			rtas_log_size += 1;
		else
			rtas_log_start += 1;

		spin_unlock_irqrestore(&log_lock, s);
		wake_up_interruptible(&rtas_log_wait);
		break;
	case ERR_TYPE_KERNEL_PANIC:
	default:
		spin_unlock_irqrestore(&log_lock, s);
		return;
	}

}
static int rtas_log_open(struct inode * inode, struct file * file)
{
	return 0;
}

static int rtas_log_release(struct inode * inode, struct file * file)
{
	return 0;
}

/* This will check if all events are logged, if they are then, we
 * know that we can safely clear the events in NVRAM.
 * Next we'll sit and wait for something else to log.
 */
static ssize_t rtas_log_read(struct file * file, char * buf,
			 size_t count, loff_t *ppos)
{
	int error;
	char *tmp;
	unsigned long s;
	unsigned long offset;

	if (!buf || count < rtas_error_log_buffer_max)
		return -EINVAL;

	count = rtas_error_log_buffer_max;

	error = verify_area(VERIFY_WRITE, buf, count);
	if (error)
		return -EFAULT;

	tmp = kmalloc(count, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;


	spin_lock_irqsave(&log_lock, s);
	/* if it's 0, then we know we got the last one (the one in NVRAM) */
	if (rtas_log_size == 0 && !no_more_logging)
		clear_error_log_nvram();
	spin_unlock_irqrestore(&log_lock, s);


	error = wait_event_interruptible(rtas_log_wait, rtas_log_size);
	if (error)
		goto out;

	spin_lock_irqsave(&log_lock, s);
	offset = rtas_error_log_buffer_max * (rtas_log_start & LOG_NUMBER_MASK);
	memcpy(tmp, &rtas_log_buf[offset], count);

	rtas_log_start += 1;
	rtas_log_size -= 1;
	spin_unlock_irqrestore(&log_lock, s);

	error = copy_to_user(buf, tmp, count) ? -EFAULT : count;
out:
	kfree(tmp);
	return error;
}

static unsigned int rtas_log_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &rtas_log_wait, wait);
	if (rtas_log_size)
		return POLLIN | POLLRDNORM;
	return 0;
}

struct file_operations proc_rtas_log_operations = {
	.read =		rtas_log_read,
	.poll =		rtas_log_poll,
	.open =		rtas_log_open,
	.release =	rtas_log_release,
};

static int enable_surveillance(int timeout)
{
	int error;

	error = rtas_call(rtas_token("set-indicator"), 3, 1, NULL, SURVEILLANCE_TOKEN,
			0, timeout);

	if (error) {
		printk(KERN_ERR "rtasd: could not enable surveillance\n");
		return -1;
	}

	return 0;
}

static int get_eventscan_parms(void)
{
	struct device_node *node;
	int *ip;

	node = find_path_device("/rtas");

	ip = (int *)get_property(node, "rtas-event-scan-rate", NULL);
	if (ip == NULL) {
		printk(KERN_ERR "rtasd: no rtas-event-scan-rate\n");
		return -1;
	}
	rtas_event_scan_rate = *ip;
	DEBUG("rtas-event-scan-rate %d\n", rtas_event_scan_rate);

	ip = (int *)get_property(node, "rtas-error-log-max", NULL);
	if (ip == NULL) {
		printk(KERN_ERR "rtasd: no rtas-error-log-max\n");
		return -1;
	}
	rtas_error_log_max = *ip;
	DEBUG("rtas-error-log-max %d\n", rtas_error_log_max);

	if (rtas_error_log_max > RTAS_ERROR_LOG_MAX) {
		printk(KERN_ERR "rtasd: truncated error log from %d to %d bytes\n", rtas_error_log_max, RTAS_ERROR_LOG_MAX);
		rtas_error_log_max = RTAS_ERROR_LOG_MAX;
	}

	/* Make room for the sequence number */
	rtas_error_log_buffer_max = rtas_error_log_max + sizeof(int);

	return 0;
}

extern long sys_sched_get_priority_max(int policy);

static int rtasd(void *unused)
{
	unsigned int err_type;
	unsigned long s;
	int cpu = 0;
	int error;
	int first_pass = 1;
	int event_scan = rtas_token("event-scan");
	int rc;

	if (event_scan == RTAS_UNKNOWN_SERVICE || get_eventscan_parms() == -1)
		goto error;

	rtas_log_buf = vmalloc(rtas_error_log_buffer_max*LOG_NUMBER);
	if (!rtas_log_buf) {
		printk(KERN_ERR "rtasd: no memory\n");
		goto error;
	}

	no_more_logging = 0;

	DEBUG("will sleep for %d jiffies\n", (HZ*60/rtas_event_scan_rate) / 2);

	daemonize();
	sigfillset(&current->blocked);
	sprintf(current->comm, "rtasd");

	/* Rusty unreal time task */
	current->policy = SCHED_FIFO;
	current->nice = sys_sched_get_priority_max(SCHED_FIFO) + 1;

	cpu = 0;
	current->cpus_allowed = 1UL << cpu_logical_map(cpu);
	schedule();

	/* See if we have any error stored in NVRAM */
	memset(logdata, 0, rtas_error_log_max);

	rc = read_error_log_nvram(logdata, rtas_error_log_max, &err_type);
	if (!rc) {
		if (err_type != ERR_FLAG_ALREADY_LOGGED) {
			pSeries_log_error(logdata, err_type | ERR_FLAG_BOOT, 0);
		}
	}

	while(1) {


		do {
			memset(logdata, 0, rtas_error_log_max);
			error = rtas_call(event_scan, 4, 1, NULL,
					RTAS_EVENT_SCAN_ALL_EVENTS, 0,
					__pa(logdata), rtas_error_log_max);
			if (error == -1) {
				printk(KERN_ERR "event-scan failed\n");
				break;
			}

			if (error == 0)
				pSeries_log_error(logdata, ERR_TYPE_RTAS_LOG, 0);

		} while(error == 0);

		DEBUG("watchdog scheduled on cpu %d\n", smp_processor_id());

		cpu++;
		if (cpu >= smp_num_cpus) {

			if (first_pass && (surveillance_timeout != -1)) {
				DEBUG("enabling surveillance\n");
				if (enable_surveillance(surveillance_timeout))
					goto error_vfree;
				DEBUG("surveillance enabled\n");
			}

			first_pass = 0;
			cpu = 0;
		}

		current->cpus_allowed = 1UL << cpu_logical_map(cpu);

		/* Check all cpus for pending events before sleeping*/
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(first_pass ? HZ : (HZ*60/rtas_event_scan_rate) / 2);
	}

error_vfree:
	vfree(rtas_log_buf);
error:
	/* Should delete proc entries */
	return -EINVAL;
}

static int __init rtas_init(void)
{
	int ret = 0;
	struct proc_dir_entry *entry;

	spin_lock(&proc_ppc64_lock);
	if (proc_ppc64_root == NULL) {
		proc_ppc64_root = proc_mkdir("ppc64", 0);
		if (!proc_ppc64_root) {
			spin_unlock(&proc_ppc64_lock);
			return -EINVAL;
		}
	}
	spin_unlock(&proc_ppc64_lock);

	if (rtas_proc_dir == NULL) {
		rtas_proc_dir = proc_mkdir("rtas", proc_ppc64_root);
	}

	if (rtas_proc_dir == NULL) {
		printk(KERN_ERR "Failed to create /proc/ppc64/rtas in rtas_init\n");
		ret = -EINVAL;
	} else {
		entry = create_proc_entry("error_log", S_IRUSR, rtas_proc_dir);
		if (entry)
			entry->proc_fops = &proc_rtas_log_operations;
		else {
			printk(KERN_ERR "Failed to create rtas/error_log proc entry\n");
			ret = -EINVAL;
		}
	}

	if (kernel_thread(rtasd, 0, CLONE_FS) < 0) {
		printk(KERN_ERR "Failed to start RTAS daemon\n");
		ret = -EINVAL;
	}

	printk(KERN_ERR "RTAS daemon started\n");
	return ret;
}

static int __init surveillance_setup(char *str)
{
	int i;

	if (get_option(&str,&i)) {
		if (i >= 0 && i <= 255)
			surveillance_timeout = i;
	}

	return 1;
}

__initcall(rtas_init);
__setup("surveillance=", surveillance_setup);
