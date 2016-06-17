/*
 *  c 2001 PPC 64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * /dev/nvram driver for PPC64
 *
 * This perhaps should live in drivers/char
 */

#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/nvram.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/nvram.h>
#include <asm/rtas.h>
#include <asm/prom.h>

#include <linux/string.h>

/*#define DEBUG_NVRAM*/

static int scan_nvram_partitions(void);
static int setup_nvram_partition(void);
static int create_os_nvram_partition(void);
static int remove_os_nvram_partition(void);
static unsigned char nvram_checksum(struct nvram_header *p);
static int write_nvram_header(struct nvram_partition * part);
static ssize_t __read_nvram(char *buf, size_t count, loff_t *index);
static ssize_t __write_nvram(char *buf, size_t count, loff_t *index);

static unsigned int rtas_nvram_size = 0;
static unsigned int nvram_fetch, nvram_store;
static char nvram_buf[NVRW_CNT];	/* assume this is in the first 4GB */
static struct nvram_partition * nvram_part;
static long error_log_nvram_index = -1;
static long error_log_nvram_size = 0;
static spinlock_t nvram_lock = SPIN_LOCK_UNLOCKED;

volatile int no_more_logging = 1;

extern volatile int error_log_cnt;

struct err_log_info {
	int error_type;
	unsigned int seq_num;
};

static loff_t dev_ppc64_nvram_llseek(struct file *file, loff_t offset, int origin)
{
	switch (origin) {
	case 1:
		offset += file->f_pos;
		break;
	case 2:
		offset += rtas_nvram_size;
		break;
	}
	if (offset < 0)
		return -EINVAL;
	file->f_pos = offset;
	return file->f_pos;
}


static ssize_t dev_ppc64_read_nvram(struct file *file, char *buf,
			  size_t count, loff_t *ppos)
{
	unsigned long len;
	char *tmp_buffer;

	if (verify_area(VERIFY_WRITE, buf, count))
		return -EFAULT;
	if (*ppos >= rtas_nvram_size)
		return 0;
	if (count > rtas_nvram_size)
		count = rtas_nvram_size;

	tmp_buffer = kmalloc(count, GFP_KERNEL);
	if (!tmp_buffer) {
		printk(KERN_ERR "dev_ppc64_read_nvram: kmalloc failed\n");
		return 0;
	}

	len = read_nvram(tmp_buffer, count, ppos);
	if ((long)len <= 0) {
		kfree(tmp_buffer);
		return len;
	}

	if (copy_to_user(buf, tmp_buffer, len)) {
		kfree(tmp_buffer);
		return -EFAULT;
	}

	kfree(tmp_buffer);
	return len;

}

static ssize_t dev_ppc64_write_nvram(struct file *file, const char *buf,
			   size_t count, loff_t *ppos)
{
	unsigned long len;
	char * tmp_buffer;

	if (verify_area(VERIFY_READ, buf, count))
		return -EFAULT;
	if (*ppos >= rtas_nvram_size)
		return 0;
	if (count > rtas_nvram_size)
		count = rtas_nvram_size;

	tmp_buffer = kmalloc(count, GFP_KERNEL);
	if (!tmp_buffer) {
		printk(KERN_ERR "dev_ppc64_write_nvram: kmalloc failed\n");
		return 0;
	}

	if (copy_from_user(tmp_buffer, buf, count)) {
		kfree(tmp_buffer);
		return -EFAULT;
	}

	len = write_nvram(tmp_buffer, count, ppos);

	kfree(tmp_buffer);
	return len;
}

static int dev_ppc64_nvram_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

struct file_operations nvram_fops = {
	.owner =	THIS_MODULE,
	.llseek =	dev_ppc64_nvram_llseek,
	.read =		dev_ppc64_read_nvram,
	.write =	dev_ppc64_write_nvram,
	.ioctl =	dev_ppc64_nvram_ioctl
};

static struct miscdevice nvram_dev = {
	NVRAM_MINOR,
	"nvram",
	&nvram_fops
};

ssize_t read_nvram(char *buf, size_t count, loff_t *index)
{
	unsigned long	s;
	ssize_t		rc;

	spin_lock_irqsave(&nvram_lock, s);
	rc = __read_nvram(buf, count, index);
	spin_unlock_irqrestore(&nvram_lock, s);

	return rc;
}
static ssize_t __read_nvram(char *buf, size_t count, loff_t *index)
{
	unsigned int i;
	unsigned long len;
	unsigned long remainder;
	char *p = buf;

	if (((*index + count) > rtas_nvram_size) || (count < 0))
		return 0;

	if (count <= NVRW_CNT) {
		remainder = count;
	} else {
		remainder = count % NVRW_CNT;
	}

	if (remainder) {
		if((rtas_call(nvram_fetch, 3, 2, &len, *index, __pa(nvram_buf),
			      remainder) != 0) || len != remainder) {
			return -EIO;
		}

		count -= remainder;
		memcpy(p, nvram_buf, remainder);
		p += remainder;
	}

	for (i = *index + remainder; count > 0 && i < rtas_nvram_size;
	     count -= NVRW_CNT) {
		if ((rtas_call(nvram_fetch, 3, 2, &len, i, __pa(nvram_buf),
			       NVRW_CNT) != 0) || len != NVRW_CNT) {
			return -EIO;
		}

		memcpy(p, nvram_buf, NVRW_CNT);

		p += NVRW_CNT;
		i += NVRW_CNT;
	}

	*index = i;
	return p - buf;
}

ssize_t write_nvram(char *buf, size_t count, loff_t *index)
{
	unsigned long	s;
	ssize_t		rc;

	spin_lock_irqsave(&nvram_lock, s);
	rc = __write_nvram(buf, count, index);
	spin_unlock_irqrestore(&nvram_lock, s);

	return rc;
}
static ssize_t __write_nvram(char *buf, size_t count, loff_t *index)
{
	unsigned int i;
	unsigned long len;
	const char *p = buf;
	unsigned long remainder;

	if (((*index + count) > rtas_nvram_size) || (count < 0))
		return 0;

	if (count <= NVRW_CNT) {
		remainder = count;
	} else {
		remainder = count % NVRW_CNT;
	}

	if (remainder) {
		memcpy(nvram_buf, p, remainder);

		if((rtas_call(nvram_store, 3, 2, &len, *index, __pa(nvram_buf),
			      remainder) != 0) || len != remainder) {
			return -EIO;
		}

		count -= remainder;
		p += remainder;
	}

	for (i = *index + remainder; count > 0 && i < rtas_nvram_size;
	     count -= NVRW_CNT) {

		memcpy(nvram_buf, p, NVRW_CNT);

		if ((rtas_call(nvram_store, 3, 2, &len, i, __pa(nvram_buf),
			       NVRW_CNT) != 0) || len != NVRW_CNT) {
			return -EIO;
		}

		p += NVRW_CNT;
		i += NVRW_CNT;
	}

	*index = i;
	return p - buf;
}

int __init nvram_init(void)
{
	struct device_node *nvram;
	unsigned int *nbytes_p, proplen;
	int error;
	int rc;

	if ((nvram = find_type_devices("nvram")) != NULL) {
		nbytes_p = (unsigned int *)get_property(nvram, "#bytes", &proplen);
		if (nbytes_p && proplen == sizeof(unsigned int)) {
			rtas_nvram_size = *nbytes_p;
		} else {
			return -EIO;
		}
	} else {
		/* If we don't know how big NVRAM is then we shouldn't touch
		   the nvram partitions */
		return -EIO;
	}

	nvram_fetch = rtas_token("nvram-fetch");
	if (nvram_fetch == RTAS_UNKNOWN_SERVICE) {
		printk("nvram_init: Does not support nvram-fetch\n");
		return -EIO;
	}

	nvram_store = rtas_token("nvram-store");
	if (nvram_store == RTAS_UNKNOWN_SERVICE) {
		printk("nvram_init: Does not support nvram-store\n");
		return -EIO;
	}
	printk(KERN_INFO "PPC64 nvram contains %d bytes\n", rtas_nvram_size);

	rc = misc_register(&nvram_dev);
	if (rc) {
		printk(KERN_ERR "nvram_init: Failed misc_register (%d)\n", rc);
		/* Going to continue to setup nvram for internal
		 * kernel services */
	}


	/* initialize our anchor for the nvram partition list */
	nvram_part = kmalloc(sizeof(struct nvram_partition), GFP_KERNEL);
	if (!nvram_part) {
		printk(KERN_ERR "nvram_init: Failed kmalloc\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&nvram_part->partition);

	/* Get all the NVRAM partitions */
	error = scan_nvram_partitions();
	if (error) {
		printk(KERN_ERR "nvram_init: Failed scan_nvram_partitions\n");
		return error;
	}

	error = setup_nvram_partition();
	if (error) {
		printk(KERN_WARNING "nvram_init: Could not find nvram partition"
		       " for nvram buffered error logging.\n");
		return error;
	}

#ifdef DEBUG_NVRAM
	print_nvram_partitions("NVRAM Partitions");
#endif

	return rc;
}

void __exit nvram_cleanup(void)
{
	misc_deregister( &nvram_dev );
}

static int scan_nvram_partitions(void)
{
	loff_t cur_index = 0;
	struct nvram_header phead;
	struct nvram_partition * tmp_part;
	unsigned char c_sum;
	long size;

	while (cur_index < rtas_nvram_size) {

		size = read_nvram((char *)&phead, NVRAM_HEADER_LEN, &cur_index);
		if (size != NVRAM_HEADER_LEN) {
			printk(KERN_ERR "scan_nvram_partitions: Error parsing "
			       "nvram partitions\n");
			return size;
		}

		cur_index -= NVRAM_HEADER_LEN; /* read_nvram will advance us */

		c_sum = nvram_checksum(&phead);
		if (c_sum != phead.checksum)
			printk(KERN_WARNING "WARNING: nvram partition checksum "
			       "was %02x, should be %02x!\n", phead.checksum, c_sum);

		tmp_part = kmalloc(sizeof(struct nvram_partition), GFP_KERNEL);
		if (!tmp_part) {
			printk(KERN_ERR "scan_nvram_partitions: kmalloc failed\n");
			return -ENOMEM;
		}

		memcpy(&tmp_part->header, &phead, NVRAM_HEADER_LEN);
		tmp_part->index = cur_index;
		list_add_tail(&tmp_part->partition, &nvram_part->partition);

		cur_index += phead.length * NVRAM_BLOCK_LEN;
	}

	return 0;
}

/* setup_nvram_partition
 *
 * This will setup the partition we need for buffering the
 * error logs and cleanup partitions if needed.
 *
 * The general strategy is the following:
 * 1.) If there is ppc64,linux partition large enough then use it.
 * 2.) If there is not a ppc64,linux partition large enough, search
 * for a free partition that is large enough.
 * 3.) If there is not a free partition large enough remove
 * _all_ OS partitions and consolidate the space.
 * 4.) Will first try getting a chunk that will satisfy the maximum
 * error log size (NVRAM_MAX_REQ).
 * 5.) If the max chunk cannot be allocated then try finding a chunk
 * that will satisfy the minum needed (NVRAM_MIN_REQ).
 */
static int setup_nvram_partition(void)
{
	struct list_head * p;
	struct nvram_partition * part;
	int rc;

	/* see if we have an OS partition that meets our needs.
	   will try getting the max we need.  If not we'll delete
	   partitions and try again. */
	list_for_each(p, &nvram_part->partition) {
		part = list_entry(p, struct nvram_partition, partition);
		if (part->header.signature != NVRAM_SIG_OS)
			continue;

		if (strcmp(part->header.name, "ppc64,linux"))
			continue;

		if (part->header.length >= NVRAM_MIN_REQ) {
			/* found our partition */
			error_log_nvram_index = part->index + NVRAM_HEADER_LEN;
			error_log_nvram_size = (part->header.length * NVRAM_BLOCK_LEN) -
						NVRAM_HEADER_LEN - sizeof(struct err_log_info);
			return 0;
		}
	}

	/* try creating a partition with the free space we have */
	rc = create_os_nvram_partition();
	if (!rc) {
		return 0;
	}

	/* need to free up some space */
	rc = remove_os_nvram_partition();
	if (rc) {
		return rc;
	}

	/* create a partition in this new space */
	rc = create_os_nvram_partition();
	if (rc) {
		printk(KERN_ERR "create_os_nvram_partition: Could not find a "
		       "NVRAM partition large enough (%d)\n", rc);
		return rc;
	}

	return 0;
}

static int remove_os_nvram_partition(void)
{
	struct list_head *i;
	struct list_head *j;
	struct nvram_partition * part;
	struct nvram_partition * cur_part;
	int rc;

	list_for_each(i, &nvram_part->partition) {
		part = list_entry(i, struct nvram_partition, partition);
		if (part->header.signature != NVRAM_SIG_OS)
			continue;

		/* Make os partition a free partition */
		part->header.signature = NVRAM_SIG_FREE;
		sprintf(part->header.name, "wwwwwwwwwwww");
		part->header.checksum = nvram_checksum(&part->header);

		/* Merge contiguous free partitions backwards */
		list_for_each_prev(j, &part->partition) {
			cur_part = list_entry(j, struct nvram_partition, partition);
			if (cur_part == nvram_part || cur_part->header.signature != NVRAM_SIG_FREE) {
				break;
			}

			part->header.length += cur_part->header.length;
			part->header.checksum = nvram_checksum(&part->header);
			part->index = cur_part->index;

			list_del(&cur_part->partition);
			kfree(cur_part);
			j = &part->partition; /* fixup our loop */
		}

		/* Merge contiguous free partitions forwards */
		list_for_each(j, &part->partition) {
			cur_part = list_entry(j, struct nvram_partition, partition);
			if (cur_part == nvram_part || cur_part->header.signature != NVRAM_SIG_FREE) {
				break;
			}

			part->header.length += cur_part->header.length;
			part->header.checksum = nvram_checksum(&part->header);

			list_del(&cur_part->partition);
			kfree(cur_part);
			j = &part->partition; /* fixup our loop */
		}

		rc = write_nvram_header(part);
		if (rc <= 0) {
			printk(KERN_ERR "remove_os_nvram_partition: write_nvram failed (%d)\n", rc);
			return rc;
		}

	}

	return 0;
}

/* create_os_nvram_partition
 *
 * Create a OS linux partition to buffer error logs.
 * Will create a partition starting at the first free
 * space found if space has enough room.
 */
static int create_os_nvram_partition(void)
{
	struct list_head * p;
	struct nvram_partition * part;
	struct nvram_partition * new_part = NULL;
	struct nvram_partition * free_part;
	struct err_log_info seq_init = { 0, 0 };
	loff_t tmp_index;
	long size = 0;
	int rc;

	/* Find a free partition that will give us the maximum needed size
	   If can't find one that will give us the minimum size needed */
	list_for_each(p, &nvram_part->partition) {
		part = list_entry(p, struct nvram_partition, partition);
		if (part->header.signature != NVRAM_SIG_FREE)
			continue;

		if (part->header.length >= NVRAM_MAX_REQ) {
			size = NVRAM_MAX_REQ;
			free_part = part;
			break;
		}
		if (!size && part->header.length >= NVRAM_MIN_REQ) {
			size = NVRAM_MIN_REQ;
			free_part = part;
		}
	}
	if (!size) {
		return -ENOSPC;
	}

	/* Create our OS partition */
	new_part = kmalloc(sizeof(struct nvram_partition), GFP_KERNEL);
	if (!new_part) {
		printk(KERN_ERR "create_os_nvram_partition: kmalloc failed\n");
		return -ENOMEM;
	}

	new_part->index = free_part->index;
	new_part->header.signature = NVRAM_SIG_OS;
	new_part->header.length = size;
	sprintf(new_part->header.name, "ppc64,linux");
	new_part->header.checksum = nvram_checksum(&new_part->header);

	rc = write_nvram_header(new_part);
	if (rc <= 0) {
		printk(KERN_ERR "create_os_nvram_partition: write_nvram_header \
				failed (%d)\n", rc);
		kfree(new_part);
		return rc;
	}

	/* make sure and initialize to zero the sequence number and the error
	   type logged */
	tmp_index = new_part->index + NVRAM_HEADER_LEN;
	rc = write_nvram((char *)&seq_init, sizeof(seq_init), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "create_os_nvram_partition: write_nvram failed (%d)\n", rc);
		kfree(new_part);
		return rc;
	}

	error_log_nvram_index = new_part->index + NVRAM_HEADER_LEN;
	error_log_nvram_size = (new_part->header.length * NVRAM_BLOCK_LEN) -
		NVRAM_HEADER_LEN - sizeof(struct err_log_info);

	list_add_tail(&new_part->partition, &free_part->partition);

	if (free_part->header.length <= size) {
		list_del(&free_part->partition);
		kfree(free_part);
		return 0;
	}

	/* Adjust the partition we stole the space from */
	free_part->index += size * NVRAM_BLOCK_LEN;
	free_part->header.length -= size;
	free_part->header.checksum = nvram_checksum(&free_part->header);

	rc = write_nvram_header(free_part);
	if (rc <= 0) {
		printk(KERN_ERR "create_os_nvram_partition: write_nvram_header "
		       "failed (%d)\n", rc);
		error_log_nvram_index = -1;
		error_log_nvram_size = 0;
		return rc;
	}

	return 0;
}


void print_nvram_partitions(char * label)
{
	struct list_head * p;
	struct nvram_partition * tmp_part;

	printk(KERN_WARNING "--------%s---------\n", label);
	printk(KERN_WARNING "indx\t\tsig\tchks\tlen\tname\n");
	list_for_each(p, &nvram_part->partition) {
		tmp_part = list_entry(p, struct nvram_partition, partition);
		printk(KERN_WARNING "%d    \t%02x\t%02x\t%d\t%s\n",
		       tmp_part->index, tmp_part->header.signature,
		       tmp_part->header.checksum, tmp_part->header.length,
		       tmp_part->header.name);
	}
}

/* write_error_log_nvram
 * In NVRAM the partition containing the error log buffer will looks like:
 * Header (in bytes):
 * +-----------+----------+--------+------------+------------------+
 * | signature | checksum | length | name       | data             |
 * |0          |1         |2      3|4         15|16        length-1|
 * +-----------+----------+--------+------------+------------------+
 * NOTE: length is in NVRAM_BLOCK_LEN
 *
 * The 'data' section would look like (in bytes):
 * +--------------+------------+-----------------------------------+
 * | event_logged | sequence # | error log                         |
 * |0            3|4          7|8            error_log_nvram_size-1|
 * +--------------+------------+-----------------------------------+
 *
 * event_logged: 0 if event has not been logged to syslog, 1 if it has
 * sequence #: The unique sequence # for each event. (until it wraps)
 * error log: The error log from event_scan
 */
int write_error_log_nvram(char * buff, int num_bytes, unsigned int err_type)
{
	int rc;
	loff_t tmp_index;
	struct err_log_info info;

	if (no_more_logging) {
		return -EPERM;
	}

	if (error_log_nvram_index == -1) {
		return -ESPIPE;
	}

	if (num_bytes > error_log_nvram_size) {
		num_bytes = error_log_nvram_size;
	}

	info.error_type = err_type;
	info.seq_num = error_log_cnt;

	tmp_index = error_log_nvram_index;

	rc = write_nvram((char *)&info, sizeof(struct err_log_info), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "write_error_log_nvram: Failed write_nvram (%d)\n", rc);
		return rc;
	}

	rc = write_nvram(buff, num_bytes, &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "write_error_log_nvram: Failed write_nvram (%d)\n", rc);
		return rc;
	}

	return 0;
}

/* read_error_log_nvram
 *
 * Reads nvram for error log for at most 'num_bytes'
 */
int read_error_log_nvram(char * buff, int num_bytes, unsigned int * err_type)
{
	int rc;
	loff_t tmp_index;
	struct err_log_info info;

	if (error_log_nvram_index == -1)
		return -1;

	if (num_bytes > error_log_nvram_size)
		num_bytes = error_log_nvram_size;

	tmp_index = error_log_nvram_index;

	rc = read_nvram((char *)&info, sizeof(struct err_log_info), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "read_error_log_nvram: Failed read_nvram (%d)\n", rc);
		return rc;
	}

	rc = read_nvram(buff, num_bytes, &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "read_error_log_nvram: Failed read_nvram (%d)\n", rc);
		return rc;
	}

	error_log_cnt = info.seq_num;
	*err_type = info.error_type;

	return 0;
}

/* This doesn't actually zero anything, but it sets the event_logged
 * word to tell that this event is safely in syslog.
 */
int clear_error_log_nvram()
{
	loff_t tmp_index;
	int clear_word = ERR_FLAG_ALREADY_LOGGED;
	int rc;

	if (error_log_nvram_index == -1) {
		return -ESPIPE;
	}

	tmp_index = error_log_nvram_index;

	rc = write_nvram((char *)&clear_word, sizeof(int), &tmp_index);
	if (rc <= 0) {
		printk(KERN_ERR "clear_error_log_nvram: Failed write_nvram (%d)\n", rc);
		return rc;
	}

	return 0;
}

static int write_nvram_header(struct nvram_partition * part)
{
	loff_t tmp_index;
	int rc;

	tmp_index = part->index;
	rc = write_nvram((char *)&part->header, NVRAM_HEADER_LEN, &tmp_index);

	return rc;
}

static unsigned char nvram_checksum(struct nvram_header *p)
{
	unsigned int c_sum, c_sum2;
	unsigned short *sp = (unsigned short *)p->name; /* assume 6 shorts */
	c_sum = p->signature + p->length + sp[0] + sp[1] + sp[2] + sp[3] + sp[4] + sp[5];

	/* The sum may have spilled into the 3rd byte.  Fold it back. */
	c_sum = ((c_sum & 0xffff) + (c_sum >> 16)) & 0xffff;
	/* The sum cannot exceed 2 bytes.  Fold it into a checksum */
	c_sum2 = (c_sum >> 8) + (c_sum << 8);
	c_sum = ((c_sum + c_sum2) >> 8) & 0xff;
	return c_sum;
}

module_init(nvram_init);
module_exit(nvram_cleanup);
MODULE_LICENSE("GPL");
