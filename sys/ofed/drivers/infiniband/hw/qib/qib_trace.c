/*
 * Copyright (c) 2008, 2009 QLogic Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/hardirq.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>

#include "qib.h"

#define QIB_TRACE_FILE		"ipath_trace"
#define QIB_TRACE_MAXCACHENAME	32

enum qib_trace_cmd_type {
	IPATH_TRACE_CLEAR = 0,
	IPATH_TRACE_RESTART,
	IPATH_TRACE_DUMP,
	IPATH_TRACE_DUMP_AND_CLEAR,
	IPATH_TRACE_GETSIZE,
	IPATH_TRACE_INJECT
};

enum qib_evt_type {
	QIB_EVT_NONE = 0,
	QIB_EVT_U8,
	QIB_EVT_U16,
	QIB_EVT_U32,
	QIB_EVT_U64,
	QIB_EVT_STR,
	QIB_EVT_BLOB,
	QIB_EVT_EMPTY
};

enum qib_evt_blobtype {
	QIB_EVT_BLOB_QP = 0,
	QIB_EVT_BLOB_CQ,
	QIB_EVT_BLOB_SRQ
};

struct qib_trace_cmd {
	enum qib_trace_cmd_type type;

	union {
		/* event cursor where we should restart from */
		unsigned int cursor;

		/* user address for storing the size of the trace buffer */
		struct {
			unsigned int __user *bufsize;
			unsigned int __user *blobsize;
		};

		struct {
			void __user *buf;
			unsigned int size;
		};
	};
};

struct qib_evt_val {
	enum qib_evt_type type;
	size_t len;

	union {
		u8	d8;
		u16	d16;
		u32	d32;
		u64	d64;
		const void *blob;
	};
};

/*
 * Event holder
 */
struct qib_evt {
	u64 tsc;
	u32 dbgmask;
	u16 len;
	u16 cpu_type;
	u64 id;
	u64 data[0];
};

struct qib_evt_container {
	atomic_t refcnt;

	/* needs to be at the end of the struct as blob data is inlined here */
	struct qib_evt evt;
};

static inline void qib_evt_settsc(struct qib_evt *evt, u64 tsc)
{
	evt->tsc = tsc;
}

static inline void qib_evt_setdbgmask(struct qib_evt *evt, u32 dbgmask)
{
	evt->dbgmask = dbgmask;
}

static inline void qib_evt_setlen(struct qib_evt *evt, u16 len)
{
	evt->len = len;
}

static inline void qib_evt_setcpu(struct qib_evt *evt, int cpu)
{
	evt->cpu_type = (evt->cpu_type & 0xff) | ((cpu & 0xff) << 8);
}

static inline void qib_evt_setblobtype(struct qib_evt *evt,
				       enum qib_evt_blobtype bt)
{
	evt->cpu_type = (evt->cpu_type & 0xff07) | ((bt & 0x1f) << 3);
}

static inline void qib_evt_settype(struct qib_evt *evt, enum qib_evt_type type)
{
	evt->cpu_type = (evt->cpu_type & 0xfff8) | (type & 0x7);
}

static inline void qib_evt_setid(struct qib_evt *evt, u64 id)
{
	evt->id = id;
}

static inline u64 qib_evt_gettsc(struct qib_evt *evt)
{
	return evt->tsc;
}

static inline u32 qib_evt_getdbgmask(struct qib_evt *evt)
{
	return evt->dbgmask;
}

static inline u16 qib_evt_getlen(struct qib_evt *evt)
{
	return evt->len;
}

static inline size_t qib_evt_getsize(struct qib_evt *evt)
{
	return sizeof(struct qib_evt) + evt->len;
}

static inline int qib_evt_getcpu(struct qib_evt *evt)
{
	return evt->cpu_type >> 8;
}

static inline enum qib_evt_blobtype qib_evt_getblobtype(struct qib_evt *evt)
{
	return evt->cpu_type >> 3 & 0x1f;
}

static inline enum qib_evt_type qib_evt_gettype(struct qib_evt *evt)
{
	return evt->cpu_type & 0x7;
}

static inline u64 qib_evt_getid(struct qib_evt *evt)
{
	return evt->id;
}

/*
 * Event Ring Buffer
 */
struct qib_evt_buf {
	const char *name;

	/* ring buffer is stored here */
	struct qib_evt_container **array;

	u64 evt_id;

	/* index of the next available spot in the ring buffer */
	unsigned int cursor;

	/*
	 * spinlock protects all concurrent activity for the
	 * above fields
	 */
	spinlock_t lock;

	struct kmem_cache *evt_cache;
	char *cache_name;
};

/*
 * File descriptor specific tracking
 */
struct qib_evt_file {
	/* private cursor -- points to the next event to be read */
	unsigned int cursor;

	/*
	 * Bit that indicates whether there's any pending data to be read
	 */
#define EVT_FLAG_DATA_TO_READ   (1 << 0)

	int flags;

	/* statistic -- number of events returned for each read(2) */
	unsigned int nevents;
};

struct evt_trace_device {
	struct cdev *cdev;
	struct device *device;
};

static int qib_trace_set_bufsize(const char *val, struct kernel_param *kp);
static int qib_trace_set_maxblobsize(const char *val, struct kernel_param *kp);
static int qib_trace_open(struct inode *inode, struct file *filp);
static int qib_trace_close(struct inode *inode, struct file *filp);
static ssize_t qib_trace_read(struct file *file, char __user *ubuf,
			      size_t nbytes, loff_t *ppos);
static ssize_t qib_trace_write(struct file *file, const char __user *ubuf,
			       size_t nbytes, loff_t *ppos);

/*
 * One global ring buffer for the driver.
 */
struct qib_evt_buf *qib_trace_buf;

static const struct file_operations qib_trace_fops = {
	.owner = THIS_MODULE,
	.open = qib_trace_open,
	.release = qib_trace_close,
	.read = qib_trace_read,
	.write = qib_trace_write
};

static struct evt_trace_device evt_dev;

/*
 * Sleeping queue when no events are available
 */
static DECLARE_WAIT_QUEUE_HEAD(read_wait);

static unsigned int qib_trace_bufsize = 512;
module_param_call(trace_bufsize, qib_trace_set_bufsize, param_get_uint,
		  &qib_trace_bufsize, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(trace_bufsize,
		 "size of the event trace ring buffer (default=512 events)");

static unsigned int maxblobsize = 256;
module_param_call(trace_maxblobsize, qib_trace_set_maxblobsize, param_get_uint,
		  &maxblobsize, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(trace_maxblobsize,
		 "max size of each blob (default=256 bytes)");

static unsigned int qib_trace_howmany;
module_param_named(trace_howmany, qib_trace_howmany, uint, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(trace_howmany, "number of events returned by read(2) "
		 "(default=0, 0 means unlimited)");

static unsigned int qib_trace_debug;
module_param_named(trace_debug, qib_trace_debug, uint, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(trace_debug, "enable debugging (default=0)");

#define EPRINTK(fmt, ...)						\
	printk(KERN_ERR QIB_DRV_NAME ": " fmt, ##__VA_ARGS__)

#define DPRINTK(fmt, ...)						\
	do {								\
		if (unlikely(qib_trace_debug != 0))			\
			printk(KERN_INFO "%s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

static const char *qib_evt_str(enum qib_evt_type type)
{
	switch (type) {
	case QIB_EVT_U8:
		return "u8";
	case QIB_EVT_U16:
		return "u16";
	case QIB_EVT_U32:
		return "u32";
	case QIB_EVT_U64:
		return "u64";
	case QIB_EVT_STR:
		return "str";
	case QIB_EVT_BLOB:
		return "blob";
	default:
		return "unknown";
	}
}

static void qib_evt_printk(struct qib_evt *evt)
{
	unsigned int i;

	switch (qib_evt_gettype(evt)) {
	case QIB_EVT_U8:
		printk(KERN_EMERG "0x%01x\n", *((u8 *) evt->data));
		break;
	case QIB_EVT_U16:
		printk(KERN_EMERG "0x%02x\n", *((u16 *) evt->data));
		break;
	case QIB_EVT_U32:
		printk(KERN_EMERG "0x%04x\n", *((u32 *) evt->data));
		break;
	case QIB_EVT_U64:
		printk(KERN_EMERG "0x%08llx\n",
			(unsigned long long) *((u64 *) evt->data));
		break;
	case QIB_EVT_STR:
		printk(KERN_EMERG "%s", (char *) evt->data);
		break;
	case QIB_EVT_BLOB: {
		u8 *c = (u8 *) evt->data;

		for (i = 0; i < evt->len; i++)
			printk(KERN_EMERG "0x%02x\n", *c);
		printk(KERN_EMERG "\n");
		break;
	}
	default:
		break;
	}
}

/*
 * Allocate an event container
 */
static struct qib_evt_container *qib_evt_alloc(struct qib_evt_buf *buf)
{
	struct qib_evt_container *c;

	c = kmem_cache_alloc(buf->evt_cache, GFP_ATOMIC);
	if (c) {
		atomic_set(&c->refcnt, 1);
		qib_evt_settype(&c->evt, QIB_EVT_NONE);
		qib_evt_setlen(&c->evt, 0);
	}

	return c;
}

/*
 * Get a reference to an event
 */
static inline void qib_evt_get(struct qib_evt_container *c)
{
	atomic_inc(&c->refcnt);
}

/*
 * Release a reference on an event
 */
static inline void qib_evt_put(struct qib_evt_buf *buf,
			       struct qib_evt_container *c, int shutdown)
{
	/* no-op if there's no container */
	if (!c)
		return;

	if (atomic_dec_and_test(&c->refcnt))
		kmem_cache_free(buf->evt_cache, c);
	else if (shutdown) {
		static int cnt;

		EPRINTK("PML leaked %s at shutdown %d refcnt=%ld\n",
			qib_evt_str(qib_evt_gettype(&c->evt)), cnt++,
			(long) atomic_read(&c->refcnt));
	}
}

static int qib_evt_buf_init(struct qib_evt_buf *buf, const char *name,
			    unsigned int size, unsigned int blobsize)
{
	unsigned int i;

	buf->array = vmalloc(size * sizeof(struct qib_evt_container *));
	if (!buf->array) {
		EPRINTK("failed to allocate ring buffer name=%s\n", name);
		goto bail;
	}

	for (i = 0; i < size; i++)
		buf->array[i] = NULL;

	buf->evt_id = 0;
	buf->cursor = 0;
	buf->name = name;

	buf->cache_name = kmalloc(QIB_TRACE_MAXCACHENAME + 1, GFP_KERNEL);
	if (!buf->cache_name) {
		EPRINTK("failed to allocate cache for buffer name=%s\n", name);
		goto bail_array;
	}
	snprintf(buf->cache_name, QIB_TRACE_MAXCACHENAME + 1, "%s-%u",
		 name, blobsize);

	buf->evt_cache = kmem_cache_create(buf->cache_name,
		sizeof(struct qib_evt_container) + maxblobsize, 0, 0, NULL);
	if (!buf->evt_cache) {
		EPRINTK("failed to create trace blob cache name=%s\n", name);
		goto bail_name;
	}

	spin_lock_init(&buf->lock);
	return 0;

bail_name:
	kfree(buf->cache_name);
bail_array:
	vfree(buf->array);
bail:
	return -ENOMEM;
}

/*
 * Even though we should never be called from interrupt level, we don't want
 * to block while doing this, so always use ATOMIC.
 */
static struct qib_evt_buf *
qib_evt_buf_create(const char *name, size_t size, size_t blobsize)
{
	struct qib_evt_buf *buf;
	int ret;

	buf = kmalloc(sizeof(struct qib_evt_buf), GFP_ATOMIC);
	if (!buf) {
		EPRINTK("failed to allocate memory\n");
		goto bail;
	}

	ret = qib_evt_buf_init(buf, name, size, blobsize);
	if (ret) {
		kfree(buf);
		buf = NULL;
	}

bail:
	return buf;
}

static void qib_evt_buf_destroy(struct qib_evt_buf *buf)
{
	unsigned int i;

	if (buf) {
		for (i = 0; i < qib_trace_bufsize; i++) {
			struct qib_evt_container *c = buf->array[i];

			qib_evt_put(buf, c, 1);
		}

		kmem_cache_destroy(buf->evt_cache);
		kfree(buf->cache_name);
		vfree(buf->array);
		kfree(buf);
		buf = NULL;
	}
}

/*
 * Reset the ring buffer to its initial state
 *
 * Assumes the caller is holding the spinlock
 */
static void qib_evt_buf_reset(struct qib_evt_buf *buf)
{
	unsigned int i;

	for (i = 0; i < qib_trace_bufsize; i++) {
		struct qib_evt_container *c = buf->array[i];

		qib_evt_put(buf, c, 0);
		buf->array[i] = NULL;
	}

	buf->cursor = 0;
}

/*
 * Reset the file pointer to its default state
 */
static void qib_evt_file_reset(struct qib_evt_file *f)
{
	f->cursor = 0;
	f->nevents = 0;
	f->flags &= ~EVT_FLAG_DATA_TO_READ;
}

static int qib_trace_set_bufsize(const char *val, struct kernel_param *kp)
{
	struct qib_evt_buf *buf = qib_trace_buf;
	unsigned int i, newsize;
	struct qib_evt_container **new = NULL;
	struct qib_evt_container **old;
	unsigned long flags;
	int ret = 0;

	newsize = simple_strtoul(val, NULL, 0);
	if (newsize == 0)
		EPRINTK("set bufsize to 0, trace disabled\n");

	if (newsize == qib_trace_bufsize) {
		DPRINTK("bufsize unchanged (old=%u, new=%u)\n",
			qib_trace_bufsize, newsize);
		goto bail;
	}

	/*
	 * Skip the buffer allocation if the facility
	 * hasn't been initialized yet.
	 */
	if (!buf) {
		DPRINTK("buf creation deferred (old=%u new=%u)\n",
			qib_trace_bufsize, newsize);
		qib_trace_bufsize = newsize;
		goto bail;
	}

	if (newsize) {
		new = vmalloc(newsize * sizeof(struct qib_evt_container *));
		if (!new) {
			EPRINTK("failed to allocate new ring buffer\n");
			ret = -ENOMEM;
			goto bail;
		}
	}

	spin_lock_irqsave(&buf->lock, flags);
	if (newsize > qib_trace_bufsize) {
		for (i = 0; i < qib_trace_bufsize; i++)
			new[i] = buf->array[i];
		for (; i < newsize; i++)
			new[i] = NULL;
	} else {
		/* newsize < qib_trace_bufsize */
		for (i = 0; i < newsize; i++)
			new[i] = buf->array[i];
		for (; i < qib_trace_bufsize; i++) {
			struct qib_evt_container *c = buf->array[i];

			qib_evt_put(buf, c, 0);
		}
	}

	DPRINTK("set bufsize (old=%u, new=%u) cursor=%u\n",
		qib_trace_bufsize, newsize, buf->cursor);
	old = buf->array;
	buf->array = new;
	qib_trace_bufsize = newsize;
	spin_unlock_irqrestore(&buf->lock, flags);
	vfree(old);
bail:
	return ret;
}

static int qib_trace_set_maxblobsize(const char *val, struct kernel_param *kp)
{
	struct qib_evt_buf *buf = qib_trace_buf;
	char *cache_name = NULL;
	unsigned int i, newsize;
	struct kmem_cache *new = NULL;
	unsigned long flags;
	int ret = 0;

	newsize = simple_strtoul(val, NULL, 0);
	if (newsize == 0) {
		EPRINTK("set maxblobsize to 0, invalid\n");
		ret = -EINVAL;
		goto bail;
	}

	if (newsize == maxblobsize) {
		DPRINTK("maxblobsize unchanged (old=%u, new=%u)\n",
			maxblobsize, newsize);
		goto bail;
	}

	/*
	 * Skip the buffer allocation if the facility
	 * hasn't been initialized yet.
	 */
	if (!buf) {
		DPRINTK("blob cache creation deferred (old=%u new=%u)\n",
			maxblobsize, newsize);
		maxblobsize = newsize;
		goto bail;
	}

	if (newsize) {
		cache_name = kmalloc(QIB_TRACE_MAXCACHENAME + 1, GFP_KERNEL);
		if (!cache_name) {
			EPRINTK("failed to allocate cache_name\n");
			ret = -ENOMEM;
			goto bail;
		}

		snprintf(cache_name, QIB_TRACE_MAXCACHENAME + 1, "%s-%u",
			 buf->name, newsize);
		new = kmem_cache_create(cache_name,
			sizeof(struct qib_evt_container) + newsize, 0, 0, NULL);
		if (!new) {
			EPRINTK("failed to create blob cache name=%s\n",
				cache_name);
			ret = -ENOMEM;
			goto bail;
		}
	}

	spin_lock_irqsave(&buf->lock, flags);
	if (newsize > maxblobsize) {
		for (i = 0; i < qib_trace_bufsize; i++) {
			struct qib_evt_container *old = buf->array[i];
			struct qib_evt_container *c;

			if (!old)
				continue;

			c = kmem_cache_alloc(new, GFP_ATOMIC);
			if (!c) {
				EPRINTK("failed to allocate evt container "
					"from cache name=%s\n", buf->name);
				ret = -ENOMEM;
				goto bail_unlock;
			}

			*c = *old;
			atomic_set(&c->refcnt, 1);
			memcpy(c->evt.data, old->evt.data,
			       qib_evt_getlen(&old->evt));
			buf->array[i] = c;
			qib_evt_put(buf, old, 0);
		}
	} else { /* newsize < maxblobsize */
		for (i = 0; i < qib_trace_bufsize; i++) {
			struct qib_evt_container *old = buf->array[i];
			struct qib_evt_container *c;
			unsigned int actual;

			if (!old)
				continue;

			c = kmem_cache_alloc(new, GFP_ATOMIC);
			if (!c) {
				EPRINTK("failed to allocate evt container "
					"from cache name=%s\n", buf->name);
				ret = -ENOMEM;
				goto bail_unlock;
			}

			*c = *old;
			atomic_set(&c->refcnt, 1);
			actual = min(qib_evt_getlen(&old->evt), (u16) newsize);
			memcpy(c->evt.data, old->evt.data, actual);
			qib_evt_setlen(&c->evt, actual);
			buf->array[i] = c;
			qib_evt_put(buf, old, 0);
		}
	}

	DPRINTK("set maxblobsize (old=%u, new=%u) cursor=%u\n",
		maxblobsize, newsize, buf->cursor);

	/* only update the ring buffer if everything went well */
	kmem_cache_destroy(buf->evt_cache);
	buf->evt_cache = new;

	kfree(buf->cache_name);
	buf->cache_name = cache_name;
	maxblobsize = newsize;
	spin_unlock_irqrestore(&buf->lock, flags);

	return ret;

bail_unlock:
	spin_unlock_irqrestore(&buf->lock, flags);
bail:
	kfree(cache_name);
	return ret;
}

static void qib_evt_buf_restart(struct qib_evt_buf *buf,
				struct qib_evt_file *file, unsigned int cursor)
{
	DPRINTK("restarting from cursor %u (buf_cursor=%u)\n",
		cursor, buf->cursor);
}

static int qib_trace_put(struct qib_evt_buf *buf, int cpu, u64 tsc,
			 u32 dbgmask, struct qib_evt_val *val)
{
	struct qib_evt_container *new, *old;
	unsigned long flags;
	int ret = 0;

	new = qib_evt_alloc(buf);
	if (!new) {
		EPRINTK("failed to allocate trace event\n");
		goto bail;
	}

	qib_evt_setcpu(&new->evt, cpu);
	qib_evt_settsc(&new->evt, tsc);
	qib_evt_setdbgmask(&new->evt, dbgmask);
	qib_evt_settype(&new->evt, val->type);
	qib_evt_setlen(&new->evt, val->len);

	switch (val->type) {
	case QIB_EVT_EMPTY:
		/* do nothing */
		break;
	case QIB_EVT_U8:
		*((u8 *) new->evt.data) = val->d8;
		break;
	case QIB_EVT_U16:
		*((u16 *) new->evt.data) = val->d16;
		break;
	case QIB_EVT_U32:
		*((u32 *) new->evt.data) = val->d32;
		break;
	case QIB_EVT_U64:
		*((u64 *) new->evt.data) = val->d64;
		break;
	case QIB_EVT_STR:
	case QIB_EVT_BLOB:
		if (val->len > maxblobsize) {
			EPRINTK("truncation [%s] len=%lu maxblobsize=%u\n",
				qib_evt_str(val->type), val->len, maxblobsize);
			val->len = maxblobsize;
		}
		memcpy(new->evt.data, val->blob, val->len);
		break;
	default:
		EPRINTK("unsupported event type\n");
		ret = -EINVAL;
		break;
	}

	if (!ret) {
		spin_lock_irqsave(&buf->lock, flags);
		DPRINTK("using cursor %u (0..%d) evt_id=%llu\n", buf->cursor,
			qib_trace_bufsize - 1,
			(unsigned long long) buf->evt_id);

		qib_evt_setid(&new->evt, buf->evt_id);
		buf->evt_id++;

		old = buf->array[buf->cursor];
		buf->array[buf->cursor] = new;
		/* means we successfully copied a datatype */
		if (buf->cursor + 1 >= qib_trace_bufsize)
			buf->cursor = 0;
		else
			buf->cursor++;

		/* notify the observers */
		wake_up_interruptible(&read_wait);
		spin_unlock_irqrestore(&buf->lock, flags);
		qib_evt_put(buf, old, 0);
	} else
		qib_evt_put(buf, new, 0);

bail:
	return ret;
}

void qib_trace_put8(struct qib_evt_buf *buf, int cpu, u64 tsc,
		    u32 dbgmask, u8 val)
{
	struct qib_evt_val eval;

	eval.type = QIB_EVT_U8;
	eval.d8 = val;
	eval.len = sizeof(u8);
	qib_trace_put(buf, cpu, tsc, dbgmask, &eval);
}

void qib_trace_put16(struct qib_evt_buf *buf, int cpu, u64 tsc,
		     u32 dbgmask, u16 val)
{
	struct qib_evt_val eval;

	eval.type = QIB_EVT_U16;
	eval.d16 = val;
	eval.len = sizeof(u16);
	qib_trace_put(buf, cpu, tsc, dbgmask, &eval);
}

void qib_trace_put32(struct qib_evt_buf *buf, int cpu, u64 tsc,
		     u32 dbgmask, u32 val)
{
	struct qib_evt_val eval;

	eval.type = QIB_EVT_U32;
	eval.d32 = val;
	eval.len = sizeof(u32);
	qib_trace_put(buf, cpu, tsc, dbgmask, &eval);
}

void qib_trace_put64(struct qib_evt_buf *buf, int cpu, u64 tsc,
		     u32 dbgmask, u64 val)
{
	struct qib_evt_val eval;

	eval.type = QIB_EVT_U64;
	eval.d64 = val;
	eval.len = sizeof(u64);
	qib_trace_put(buf, cpu, tsc, dbgmask, &eval);
}

void qib_trace_putblob(struct qib_evt_buf *buf, int cpu, u64 tsc,
		       u32 dbgmask, void *blob, u16 len)
{
	struct qib_evt_val eval;

	eval.type = QIB_EVT_BLOB;
	eval.blob = blob;
	eval.len = len;
	qib_trace_put(buf, cpu, tsc, dbgmask, &eval);
}

void qib_trace_putstr(struct qib_evt_buf *buf, int cpu, u64 tsc,
		      u32 dbgmask, const char *str)
{
	struct qib_evt_val eval;

	eval.type = QIB_EVT_STR;
	eval.blob = str;
	eval.len = strlen(str) + 1;
	qib_trace_put(buf, cpu, tsc, dbgmask, &eval);
}

void qib_trace_vputstr(struct qib_evt_buf *buf, int cpu, u64 tsc,
		       u32 dbgmask, const char *fmt, ...)
{
	struct qib_evt_container *new, *old;
	unsigned long flags;
	va_list ap;
	int n;

	new = qib_evt_alloc(buf);
	if (!new) {
		EPRINTK("failed to allocate trace event\n");
		return;
	}

	va_start(ap, fmt);
	n = vscnprintf((void *)new->evt.data, maxblobsize, fmt, ap);
	va_end(ap);

	qib_evt_settsc(&new->evt, tsc);
	qib_evt_setdbgmask(&new->evt, dbgmask);
	qib_evt_setcpu(&new->evt, cpu);
	qib_evt_settype(&new->evt, QIB_EVT_STR);
	qib_evt_setlen(&new->evt, n + 1);

	spin_lock_irqsave(&buf->lock, flags);

	DPRINTK("using cursor %u (0..%d) evt_id=%llu\n", buf->cursor,
		qib_trace_bufsize - 1, (unsigned long long) buf->evt_id);
	qib_evt_setid(&new->evt, buf->evt_id);
	buf->evt_id++;

	old = buf->array[buf->cursor];
	buf->array[buf->cursor] = new;

	if (buf->cursor + 1 >= qib_trace_bufsize)
		buf->cursor = 0;
	else
		buf->cursor++;

	/* notify the observers */
	wake_up_interruptible(&read_wait);
	spin_unlock_irqrestore(&buf->lock, flags);
	qib_evt_put(buf, old, 0);
}

static void qib_trace_putempty(struct qib_evt_buf *buf, int cpu, u64 tsc,
			       u32 dbgmask)
{
	struct qib_evt_val eval;

	eval.type = QIB_EVT_EMPTY;
	eval.len = 0;
	qib_trace_put(buf, cpu, tsc, dbgmask, &eval);
}

static int qib_trace_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	const int minor = iminor(inode);
	struct qib_evt_file *f;

	if (minor != QIB_TRACE_MINOR)
		return -ENODEV;

	f = kzalloc(sizeof(struct qib_evt_file), GFP_KERNEL);
	if (!f) {
		EPRINTK("failed to allocate memory\n");
		ret = -ENOMEM;
		goto bail;
	}

	/* take a snapshot of where the cursor is */
	f->cursor = qib_trace_buf->cursor;

	filp->private_data = f;
	DPRINTK("opened %s successfully pid=%u\n",
		QIB_TRACE_FILE, current->tgid);
bail:
	return ret;
}

static int qib_trace_close(struct inode *inode, struct file *filp)
{
	struct qib_evt_file *f = filp->private_data;

	kfree(f);
	DPRINTK("closed %s successfully pid=%u\n",
		QIB_TRACE_FILE, current->tgid);
	return 0;
}

static ssize_t qib_trace_read(struct file *file, char __user *ubuf,
			      size_t nbytes, loff_t *ppos)
{
	struct qib_evt_file *f = (struct qib_evt_file *) file->private_data;
	struct qib_evt_container *c;
	struct qib_evt sentinel = {0};
	ssize_t ret = 0;
	u8 __user *to;
	size_t actual, left;
	size_t howmany = qib_trace_howmany;
	int err;

	if (nbytes == 0) {
		DPRINTK("Tried to read 0 bytes pid=%u\n", current->tgid);
		goto bail;
	}

	if (f->cursor == qib_trace_buf->cursor) {
		if (!(f->flags & EVT_FLAG_DATA_TO_READ) &&
		    file->f_flags & O_NONBLOCK) {
			DPRINTK("no data to read, would block pid=%u\n",
				current->tgid);
			ret = -EAGAIN;
			goto bail;
		}

		if (f->flags & EVT_FLAG_DATA_TO_READ) {
			DPRINTK("returned %u events for pid=%u\n",
				f->nevents, current->tgid);
			f->flags &= ~EVT_FLAG_DATA_TO_READ;
			f->nevents = 0;
			goto bail;
		}

		DPRINTK("going to sleep pid=%u\n", current->tgid);
		ret = wait_event_interruptible(read_wait,
				f->cursor != qib_trace_buf->cursor);
		if (ret) {
			if (ret == -ERESTARTSYS)
				ret = -EINTR;
			DPRINTK("failed to wake up pid=%u ret=%ld\n",
				current->tgid, (long) ret);
			return ret;
		}

		DPRINTK("woken up pid=%u f->cursor=%u buf_cursor=%u\n",
			current->tgid, f->cursor, qib_trace_buf->cursor);
	}

	to = ubuf;
	left = nbytes;
	spin_lock_irq(&qib_trace_buf->lock);
	do {
		/* keep a local copy of an event */
		c = qib_trace_buf->array[f->cursor];
		if (!c) {
			/*
			 * Someone has cleared the buffer,
			 * so start reading again from 0.
			 */
			DPRINTK("trace buffer was cleared f->cursor=%u "
				"buf_cursor=%u pid=%u\n",
				f->cursor, qib_trace_buf->cursor,
				current->tgid);
			f->cursor = 0;
			continue;
		}
		if (left < qib_evt_getsize(&c->evt)) {
			/* the user buffer is full, give up */
			EPRINTK("not enough space in user buffer pid=%u\n",
				current->tgid);
			break;
		}
		qib_evt_get(c);
		if (f->cursor + 1 >= qib_trace_bufsize)
			f->cursor = 0;
		else
			f->cursor++;
		actual = qib_evt_getsize(&c->evt);
		spin_unlock_irq(&qib_trace_buf->lock);

		DPRINTK("about to copy event type=%s len=%lu sizeof=%lu "
			"refcnt=%ld total_left=%lu pid=%u\n",
			qib_evt_str(qib_evt_gettype(&c->evt)),
			(unsigned long) sizeof(struct qib_evt) +
				qib_evt_getlen(&c->evt),
			(unsigned long) sizeof(struct qib_evt),
			(long) atomic_read(&c->refcnt),
			(unsigned long) left, current->tgid);

		err = copy_to_user(to, &c->evt, actual);
		/* done with the event so release it */
		qib_evt_put(qib_trace_buf, c, 0);

		if (err) {
			EPRINTK("failed to copy_out event pid=%u\n",
				current->tgid);
			ret = -EFAULT;
			goto bail;
		}

		to += actual;
		left -= actual;
		ret += actual;

		f->nevents++;
		spin_lock_irq(&qib_trace_buf->lock);

		if (howmany) {
			if (--howmany == 0)
				break;
		}
	} while (f->cursor != qib_trace_buf->cursor);
	spin_unlock_irq(&qib_trace_buf->lock);

	if (copy_to_user(to, &sentinel, sizeof(sentinel))) {
		EPRINTK("failed to copy_out sentinel pid=%u\n", current->tgid);
		ret = -EFAULT;
		goto bail;
	}

	file_accessed(file);
	f->flags |= EVT_FLAG_DATA_TO_READ;
	DPRINTK("copied successfully %ld bytes nevents=%u pid=%u\n",
		ret, f->nevents, current->tgid);
bail:
	return ret;
}

static ssize_t qib_trace_write(struct file *file, const char __user *ubuf,
			       size_t nbytes, loff_t *ppos)
{
	struct qib_evt_file *f = (struct qib_evt_file *) file->private_data;
	struct qib_trace_cmd cmd;
	ssize_t ret = 0;
	struct qib_evt sentinel = {0};

	if (nbytes == 0) {
		DPRINTK("Tried to write 0 bytes pid=%u\n", current->tgid);
		goto bail;
	}

	if (copy_from_user(&cmd, ubuf, sizeof(cmd))) {
		EPRINTK("failed to copy trace command pid=%u\n", current->tgid);
		ret = -EFAULT;
		goto bail;
	}

	ret = sizeof(cmd);
	switch (cmd.type) {
	case IPATH_TRACE_CLEAR:
		DPRINTK("got trace_clear pid=%u\n", current->tgid);

		spin_lock_irq(&qib_trace_buf->lock);
		qib_evt_buf_reset(qib_trace_buf);
		spin_unlock_irq(&qib_trace_buf->lock);

		qib_evt_file_reset(f);
		DPRINTK("buffer cleared successfully pid=%u\n", current->tgid);
		break;

	case IPATH_TRACE_RESTART:
		DPRINTK("got trace_restart pid=%u\n", current->tgid);
		spin_lock_irq(&qib_trace_buf->lock);
		if (cmd.cursor >= qib_trace_bufsize) {
			spin_unlock_irq(&qib_trace_buf->lock);
			DPRINTK("found invalid cursor %u (0..%u)\n",
				cmd.cursor, qib_trace_bufsize - 1);
			ret = -EINVAL;
			goto bail;
		}

		qib_evt_buf_restart(qib_trace_buf, f, cmd.cursor);
		spin_unlock_irq(&qib_trace_buf->lock);
		DPRINTK("cursor restarted successfully pid=%u\n",
			current->tgid);
		break;

	case IPATH_TRACE_DUMP:
	case IPATH_TRACE_DUMP_AND_CLEAR: {
		int i;
		void __user *to = cmd.buf;
		unsigned long left = (unsigned long) cmd.size;
		unsigned int read_cursor;

		if (cmd.type == IPATH_TRACE_DUMP)
			DPRINTK("got trace_dump len %lu pid=%u\n",
				left, current->tgid);
		else
			DPRINTK("got trace_dump_and_clear len %lu pid=%u\n",
				left, current->tgid);

		if (left < sizeof(sentinel)) {
			ret = -EINVAL;
			goto bail;
		}
		left -= sizeof(sentinel);

		spin_lock_irq(&qib_trace_buf->lock);
		read_cursor = f->cursor;
		for (i = 0; i < qib_trace_bufsize; i++) {
			struct qib_evt_container *c;
			unsigned long actual;
			int err;

			c = qib_trace_buf->array[(read_cursor+i) %
						 qib_trace_bufsize];
			if (!c)
				continue;

			actual = qib_evt_getsize(&c->evt);
			if (left < actual)
				break;

			qib_evt_get(c);
			spin_unlock_irq(&qib_trace_buf->lock);

			DPRINTK("about to copy event type=%s len=%lu "
				"sizeof=%lu refcnt=%ld total_left=%lu\n",
				qib_evt_str(qib_evt_gettype(&c->evt)),
				(unsigned long) sizeof(struct qib_evt) +
					qib_evt_getlen(&c->evt),
				(unsigned long) sizeof(struct qib_evt),
				(long) atomic_read(&c->refcnt),
				(unsigned long) left);

			err = copy_to_user(to, &c->evt, actual);
			qib_evt_put(qib_trace_buf, c, 0);

			if (err) {
				EPRINTK("failed to copy_out event "
					"pid=%u\n", current->tgid);
				ret = -EFAULT;
				goto bail;
			}
			to += actual;
			left -= actual;
			spin_lock_irq(&qib_trace_buf->lock);
		}
		if (cmd.type == IPATH_TRACE_DUMP_AND_CLEAR)
			qib_evt_buf_reset(qib_trace_buf);
		spin_unlock_irq(&qib_trace_buf->lock);

		if (copy_to_user(to, &sentinel, sizeof(sentinel))) {
			EPRINTK("failed to copy_out sentinel pid=%u\n",
				current->tgid);
			ret = -EFAULT;
			goto bail;
		}

		if (cmd.type == IPATH_TRACE_DUMP_AND_CLEAR)
			qib_evt_file_reset(f);
		break;
	}

	case IPATH_TRACE_GETSIZE:
		DPRINTK("got trace_getsize pid=%u\n", current->tgid);

		if (copy_to_user(cmd.bufsize, &qib_trace_bufsize,
				 sizeof(qib_trace_bufsize))) {
			EPRINTK("failed to copy_out bufsize pid=%u\n",
				current->tgid);
			ret = -EFAULT;
			goto bail;
		}

		if (copy_to_user(cmd.blobsize, &maxblobsize,
				 sizeof(maxblobsize))) {
			EPRINTK("failed to copy_out blobsize pid=%u\n",
				current->tgid);
			ret = -EFAULT;
			goto bail;
		}

		DPRINTK("trace_getsize copied successfully pid=%u\n",
			current->tgid);
		break;

	case IPATH_TRACE_INJECT: {
		int cpu = smp_processor_id();
		cycles_t now = get_cycles();

		DPRINTK("got trace_inject pid=%u\n", current->tgid);
		qib_trace_putempty(qib_trace_buf, cpu, now, 0);
		break;
	}

	default:
		EPRINTK("Unsupported cmd type (%d) pid=%u\n",
			cmd.type, current->tgid);
		ret = -ENOSYS;
		break;
	}

bail:
	return ret;
}

#ifndef ATOMIC_NOTIFIER_INIT
/*
 * Some backports don't have this, so define here for now.
 * We may not want to keep this for upstream.  If we do, we'll
 * do this via the backports at that time.
 */
#define atomic_notifier_chain_register notifier_chain_register
#define atomic_notifier_chain_unregister notifier_chain_unregister
#endif

/*
 * qibtrace_panic_event() is called by the panic handler.
 * We want to dump our trace buffer on panic, so we can
 * get useful info when the panic is caused by us.
 * No newline in printk, normally part of trace buffer.
 */
static int qibtrace_panic_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	int i;

	/* bust_spinlocks(1); */
	printk(KERN_EMERG "Dumping qib trace buffer from panic\n");
	for (i = 0; i < qib_trace_bufsize; i++) {
		struct qib_evt_container *c = qib_trace_buf->array[i];

		if (c)
			qib_evt_printk(&c->evt);
	}
	printk(KERN_EMERG "Done dumping qib trace buffer\n");
	/* bust_spinlocks(0); */
	return NOTIFY_DONE;
}

static struct notifier_block qibtrace_panic_block = {
	.notifier_call	= qibtrace_panic_event,
	.priority	= 50, /* seems to be about middle */
};

/*
 * Called when the qib module is loaded
 */
int __init qib_trace_init(void)
{
	int ret = 0;

	if (!qib_trace_bufsize) {
		EPRINTK("tracing disabled, size is 0\n");
		goto bail;
	}

	qib_trace_buf = qib_evt_buf_create("qib_trace", qib_trace_bufsize,
					   maxblobsize);
	if (!qib_trace_buf) {
		EPRINTK("failed to create event trace ring buffer, "
			"tracing disabled\n");
		qib_trace_bufsize = 0;
		goto bail;
	}

	ret = qib_cdev_init(QIB_TRACE_MINOR, QIB_TRACE_FILE, &qib_trace_fops,
			    &evt_dev.cdev, &evt_dev.device);
	if (ret)
		goto bail_buf;

	/* Register a call for panic conditions. */
	atomic_notifier_chain_register(&panic_notifier_list,
				       &qibtrace_panic_block);

	DPRINTK("event trace init successful\n");
	goto bail;

bail_buf:
	qib_evt_buf_destroy(qib_trace_buf);
bail:
	return ret;
}

/*
 * Called when the qib module is unloaded
 */
void qib_trace_fini(void)
{
	if (qib_trace_buf) {
		qib_cdev_cleanup(&evt_dev.cdev, &evt_dev.device);
		atomic_notifier_chain_unregister(&panic_notifier_list,
						 &qibtrace_panic_block);
		qib_evt_buf_destroy(qib_trace_buf);
	}
	DPRINTK("event trace fini successful\n");
}
