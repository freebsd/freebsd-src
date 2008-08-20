/******************************************************************************
 * xenbus_xs.c
 *
 * This is the kernel equivalent of the "xs" library.  We don't need everything
 * and we use xenbus_comms for communication.
 *
 * Copyright (C) 2005 Rusty Russell, IBM Corporation
 * 
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/sema.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>

#include <machine/xen/xen-os.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/xenbus.h>
#include <machine/stdarg.h>

#include <xen/xenbus/xenbus_comms.h>
static int xs_process_msg(enum xsd_sockmsg_type *type);

#define kmalloc(size, unused) malloc(size, M_DEVBUF, M_WAITOK)
#define BUG_ON        PANIC_IF
#define DEFINE_SPINLOCK(lock) struct mtx lock
#define u32           uint32_t
#define list_del(head, ent)      TAILQ_REMOVE(head, ent, list) 
#define simple_strtoul strtoul
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define list_empty    TAILQ_EMPTY

#define streq(a, b) (strcmp((a), (b)) == 0)
int xenwatch_running = 0;
int xenbus_running = 0;

struct kvec {
		const void *iov_base;
		size_t      iov_len;
};

struct xs_stored_msg {
		TAILQ_ENTRY(xs_stored_msg) list;

		struct xsd_sockmsg hdr;

		union {
				/* Queued replies. */
				struct {
						char *body;
				} reply;

				/* Queued watch events. */
				struct {
						struct xenbus_watch *handle;
						char **vec;
						unsigned int vec_size;
				} watch;
		} u;
};

struct xs_handle {
		/* A list of replies. Currently only one will ever be outstanding. */
		TAILQ_HEAD(xs_handle_list, xs_stored_msg) reply_list;
		struct mtx reply_lock;
		int reply_waitq;

		/* One request at a time. */
		struct sx request_mutex;

		/* Protect transactions against save/restore. */
		struct rw_semaphore suspend_mutex;
};

static struct xs_handle xs_state;

/* List of registered watches, and a lock to protect it. */
static LIST_HEAD(watch_list_head, xenbus_watch) watches;
static DEFINE_SPINLOCK(watches_lock);
/* List of pending watch callback events, and a lock to protect it. */
static TAILQ_HEAD(event_list_head, xs_stored_msg) watch_events;
static DEFINE_SPINLOCK(watch_events_lock);
/*
 * Details of the xenwatch callback kernel thread. The thread waits on the
 * watch_events_waitq for work to do (queued on watch_events list). When it
 * wakes up it acquires the xenwatch_mutex before reading the list and
 * carrying out work.
 */
static pid_t xenwatch_pid;
struct sx xenwatch_mutex;
static int watch_events_waitq;

static int get_error(const char *errorstring)
{
		unsigned int i;

		for (i = 0; !streq(errorstring, xsd_errors[i].errstring); i++) {
				if (i == ARRAY_SIZE(xsd_errors) - 1) {
						log(LOG_WARNING, "XENBUS xen store gave: unknown error %s",
							   errorstring);
						return EINVAL;
				}
		}
		return xsd_errors[i].errnum;
}

extern void idle_block(void);
extern void kdb_backtrace(void);

static void *read_reply(enum xsd_sockmsg_type *type, unsigned int *len)
{
		struct xs_stored_msg *msg;
		char *body;
		int i, err;
		enum xsd_sockmsg_type itype = *type;

		printf("read_reply ");
		if (xenbus_running == 0) {
				/*
				 * Give other domain time to run :-/
				 */
				for (i = 0; i < 1000000 && (xenbus_running == 0); i++) {
						err = xs_process_msg(type);
						
						if ((err == 0)
							&& (*type != XS_WATCH_EVENT))
								break;
							 
						HYPERVISOR_yield();
				}
				
				if (list_empty(&xs_state.reply_list)) {
						printf("giving up and returning an error type=%d\n",
								*type);
						kdb_backtrace();
 						return (ERR_PTR(-1));
				}
				
		}

		mtx_lock(&xs_state.reply_lock);
		if (xenbus_running) {
				while (list_empty(&xs_state.reply_list)) {
						mtx_unlock(&xs_state.reply_lock);
						wait_event_interruptible(&xs_state.reply_waitq,
												 !list_empty(&xs_state.reply_list));
				
						mtx_lock(&xs_state.reply_lock);
				}
		}
		
		msg = TAILQ_FIRST(&xs_state.reply_list);
		list_del(&xs_state.reply_list, msg);

		mtx_unlock(&xs_state.reply_lock);

		printf("itype=%d htype=%d ", itype, msg->hdr.type);
		*type = msg->hdr.type;
		if (len)
				*len = msg->hdr.len;
		body = msg->u.reply.body;

		kfree(msg);
		if (len)
				printf("len=%d\n", *len);
		else
				printf("len=NULL\n");
		return body;
}

#if 0
/* Emergency write. UNUSED*/
void xenbus_debug_write(const char *str, unsigned int count)
{
		struct xsd_sockmsg msg = { 0 };

		msg.type = XS_DEBUG;
		msg.len = sizeof("print") + count + 1;

		sx_xlock(&xs_state.request_mutex);
		xb_write(&msg, sizeof(msg));
		xb_write("print", sizeof("print"));
		xb_write(str, count);
		xb_write("", 1);
		sx_xunlock(&xs_state.request_mutex);
}

#endif
void *xenbus_dev_request_and_reply(struct xsd_sockmsg *msg)
{
		void *ret;
		struct xsd_sockmsg req_msg = *msg;
		int err;

		if (req_msg.type == XS_TRANSACTION_START)
				down_read(&xs_state.suspend_mutex);

		sx_xlock(&xs_state.request_mutex);

		err = xb_write(msg, sizeof(*msg) + msg->len);
		if (err) {
				msg->type = XS_ERROR;
				ret = ERR_PTR(err);
		} else {
				ret = read_reply(&msg->type, &msg->len);
		}

		sx_xunlock(&xs_state.request_mutex);

		if ((msg->type == XS_TRANSACTION_END) ||
			((req_msg.type == XS_TRANSACTION_START) &&
			 (msg->type == XS_ERROR)))
				up_read(&xs_state.suspend_mutex);

		return ret;
}

/* Send message to xs, get kmalloc'ed reply.  ERR_PTR() on error. */
static void *xs_talkv(struct xenbus_transaction t,
					  enum xsd_sockmsg_type type,
					  const struct kvec *iovec,
					  unsigned int num_vecs,
					  unsigned int *len)
{
		struct xsd_sockmsg msg;
		void *ret = NULL;
		unsigned int i;
		int err;

		msg.tx_id = t.id;
		msg.req_id = 0;
		msg.type = type;
		msg.len = 0;
		for (i = 0; i < num_vecs; i++)
				msg.len += iovec[i].iov_len;

		printf("xs_talkv ");
		
		sx_xlock(&xs_state.request_mutex);

		err = xb_write(&msg, sizeof(msg));
		if (err) {
				sx_xunlock(&xs_state.request_mutex);
				printf("xs_talkv failed %d\n", err);
				return ERR_PTR(err);
		}

		for (i = 0; i < num_vecs; i++) {
				err = xb_write(iovec[i].iov_base, iovec[i].iov_len);;
				if (err) {		
						sx_xunlock(&xs_state.request_mutex);
						printf("xs_talkv failed %d\n", err);
						return ERR_PTR(err);
				}
		}

		ret = read_reply(&msg.type, len);

		sx_xunlock(&xs_state.request_mutex);

		if (IS_ERR(ret))
				return ret;

		if (msg.type == XS_ERROR) {
				err = get_error(ret);
				kfree(ret);
				return ERR_PTR(-err);
		}

		if (xenwatch_running == 0) {
				while (!TAILQ_EMPTY(&watch_events)) {
						struct xs_stored_msg *wmsg = TAILQ_FIRST(&watch_events);
						list_del(&watch_events, wmsg);
						wmsg->u.watch.handle->callback(
								wmsg->u.watch.handle,
								(const char **)wmsg->u.watch.vec,
								wmsg->u.watch.vec_size);
						kfree(wmsg->u.watch.vec);
						kfree(wmsg);
				}
		}
		BUG_ON(msg.type != type);		

		return ret;
}

/* Simplified version of xs_talkv: single message. */
static void *xs_single(struct xenbus_transaction t,
					   enum xsd_sockmsg_type type,
					   const char *string,
					   unsigned int *len)
{
		struct kvec iovec;

		printf("xs_single %s ", string);
		iovec.iov_base = (const void *)string;
		iovec.iov_len = strlen(string) + 1;
		return xs_talkv(t, type, &iovec, 1, len);
}

/* Many commands only need an ack, don't care what it says. */
static int xs_error(char *reply)
{
		if (IS_ERR(reply))
				return PTR_ERR(reply);
		kfree(reply);
		return 0;
}

static unsigned int count_strings(const char *strings, unsigned int len)
{
		unsigned int num;
		const char *p;

		for (p = strings, num = 0; p < strings + len; p += strlen(p) + 1)
				num++;

		return num;
}

/* Return the path to dir with /name appended. Buffer must be kfree()'ed. */ 
static char *join(const char *dir, const char *name)
{
		char *buffer;

		buffer = kmalloc(strlen(dir) + strlen("/") + strlen(name) + 1,
						 GFP_KERNEL);
		if (buffer == NULL)
				return ERR_PTR(-ENOMEM);

		strcpy(buffer, dir);
		if (!streq(name, "")) {
				strcat(buffer, "/");
				strcat(buffer, name);
		}

		return buffer;
}

static char **split(char *strings, unsigned int len, unsigned int *num)
{
		char *p, **ret;

		/* Count the strings. */
		*num = count_strings(strings, len) + 1;

		/* Transfer to one big alloc for easy freeing. */
		ret = kmalloc(*num * sizeof(char *) + len, GFP_KERNEL);
		if (!ret) {
				kfree(strings);
				return ERR_PTR(-ENOMEM);
		}
		memcpy(&ret[*num], strings, len);
		kfree(strings);

		strings = (char *)&ret[*num];
		for (p = strings, *num = 0; p < strings + len; p += strlen(p) + 1)
				ret[(*num)++] = p;

		ret[*num] = strings + len;
		
		return ret;
}

char **xenbus_directory(struct xenbus_transaction t,
						const char *dir, const char *node, unsigned int *num)
{
		char *strings, *path;
		unsigned int len = 0;

		path = join(dir, node);
		if (IS_ERR(path))
				return (char **)path;

		strings = xs_single(t, XS_DIRECTORY, path, &len);
		kfree(path);
		if (IS_ERR(strings))
				return (char **)strings;

		return split(strings, len, num);
}
EXPORT_SYMBOL(xenbus_directory);

/* Check if a path exists. Return 1 if it does. */
int xenbus_exists(struct xenbus_transaction t,
				  const char *dir, const char *node)
{
		char **d;
		int dir_n;

		d = xenbus_directory(t, dir, node, &dir_n);
		if (IS_ERR(d))
				return 0;
		kfree(d);
		return 1;
}
EXPORT_SYMBOL(xenbus_exists);

/* Get the value of a single file.
 * Returns a kmalloced value: call free() on it after use.
 * len indicates length in bytes.
 */
void *xenbus_read(struct xenbus_transaction t,
				  const char *dir, const char *node, unsigned int *len)
{
		char *path;
		void *ret;

		path = join(dir, node);
		if (IS_ERR(path))
				return (void *)path;

		printf("xs_read ");
		ret = xs_single(t, XS_READ, path, len);
		kfree(path);
		return ret;
}
EXPORT_SYMBOL(xenbus_read);

/* Write the value of a single file.
 * Returns -err on failure.
 */
int xenbus_write(struct xenbus_transaction t,
				 const char *dir, const char *node, const char *string)
{
		char *path;
		struct kvec iovec[2];
		int ret;

		path = join(dir, node);
		if (IS_ERR(path))
				return PTR_ERR(path);

		iovec[0].iov_base = path;
		iovec[0].iov_len = strlen(path) + 1;
		iovec[1].iov_base = string;
		iovec[1].iov_len = strlen(string);

		printf("xenbus_write dir=%s val=%s ", dir, string);
		ret = xs_error(xs_talkv(t, XS_WRITE, iovec, ARRAY_SIZE(iovec), NULL));
		kfree(path);
		return ret;
}
EXPORT_SYMBOL(xenbus_write);

/* Create a new directory. */
int xenbus_mkdir(struct xenbus_transaction t,
				 const char *dir, const char *node)
{
		char *path;
		int ret;

		path = join(dir, node);
		if (IS_ERR(path))
				return PTR_ERR(path);

		ret = xs_error(xs_single(t, XS_MKDIR, path, NULL));
		kfree(path);
		return ret;
}
EXPORT_SYMBOL(xenbus_mkdir);

/* Destroy a file or directory (directories must be empty). */
int xenbus_rm(struct xenbus_transaction t, const char *dir, const char *node)
{
		char *path;
		int ret;

		path = join(dir, node);
		if (IS_ERR(path))
				return PTR_ERR(path);

		ret = xs_error(xs_single(t, XS_RM, path, NULL));
		kfree(path);
		return ret;
}
EXPORT_SYMBOL(xenbus_rm);

/* Start a transaction: changes by others will not be seen during this
 * transaction, and changes will not be visible to others until end.
 */
int xenbus_transaction_start(struct xenbus_transaction *t)
{
		char *id_str;

		down_read(&xs_state.suspend_mutex);

		id_str = xs_single(XBT_NIL, XS_TRANSACTION_START, "", NULL);
		if (IS_ERR(id_str)) {
				up_read(&xs_state.suspend_mutex);
				return PTR_ERR(id_str);
		}

		t->id = simple_strtoul(id_str, NULL, 0);
		kfree(id_str);

		return 0;
}
EXPORT_SYMBOL(xenbus_transaction_start);

/* End a transaction.
 * If abandon is true, transaction is discarded instead of committed.
 */
int xenbus_transaction_end(struct xenbus_transaction t, int abort)
{
		char abortstr[2];
		int err;

		if (abort)
				strcpy(abortstr, "F");
		else
				strcpy(abortstr, "T");

		printf("xenbus_transaction_end ");
		err = xs_error(xs_single(t, XS_TRANSACTION_END, abortstr, NULL));

		up_read(&xs_state.suspend_mutex);

		return err;
}
EXPORT_SYMBOL(xenbus_transaction_end);

/* Single read and scanf: returns -errno or num scanned. */
int xenbus_scanf(struct xenbus_transaction t,
				 const char *dir, const char *node, const char *fmt, ...)
{
		va_list ap;
		int ret;
		char *val;

		val = xenbus_read(t, dir, node, NULL);
		if (IS_ERR(val))
				return PTR_ERR(val);

		va_start(ap, fmt);
		ret = vsscanf(val, fmt, ap);
		va_end(ap);
		kfree(val);
		/* Distinctive errno. */
		if (ret == 0)
				return -ERANGE;
		return ret;
}
EXPORT_SYMBOL(xenbus_scanf);

/* Single printf and write: returns -errno or 0. */
int xenbus_printf(struct xenbus_transaction t,
				  const char *dir, const char *node, const char *fmt, ...)
{
		va_list ap;
		int ret;
#define PRINTF_BUFFER_SIZE 4096
		char *printf_buffer;

		printf_buffer = kmalloc(PRINTF_BUFFER_SIZE, GFP_KERNEL);
		if (printf_buffer == NULL)
				return -ENOMEM;

		va_start(ap, fmt);
		ret = vsnprintf(printf_buffer, PRINTF_BUFFER_SIZE, fmt, ap);
		va_end(ap);

		BUG_ON(ret > PRINTF_BUFFER_SIZE-1);
		ret = xenbus_write(t, dir, node, printf_buffer);

		kfree(printf_buffer);

		return ret;
}
EXPORT_SYMBOL(xenbus_printf);

/* Takes tuples of names, scanf-style args, and void **, NULL terminated. */
int xenbus_gather(struct xenbus_transaction t, const char *dir, ...)
{
		va_list ap;
		const char *name;
		int i, ret = 0;

		for (i = 0; i < 10000; i++)
				HYPERVISOR_yield();
		
		printf("gather ");
		va_start(ap, dir);
		while (ret == 0 && (name = va_arg(ap, char *)) != NULL) {
				const char *fmt = va_arg(ap, char *);
				void *result = va_arg(ap, void *);
				char *p;

				p = xenbus_read(t, dir, name, NULL);
				if (IS_ERR(p)) {
						ret = PTR_ERR(p);
						break;
				}
				printf(" %s ", p);
				if (fmt) {
						if (sscanf(p, fmt, result) == 0)
								ret = -EINVAL;
						kfree(p);
				} else
						*(char **)result = p;
		}
		va_end(ap);
		printf("\n");
		return ret;
}
EXPORT_SYMBOL(xenbus_gather);

static int xs_watch(const char *path, const char *token)
{
		struct kvec iov[2];

		iov[0].iov_base = path;
		iov[0].iov_len = strlen(path) + 1;
		iov[1].iov_base = token;
		iov[1].iov_len = strlen(token) + 1;

		return xs_error(xs_talkv(XBT_NIL, XS_WATCH, iov,
								 ARRAY_SIZE(iov), NULL));
}

static int xs_unwatch(const char *path, const char *token)
{
		struct kvec iov[2];

		iov[0].iov_base = path;
		iov[0].iov_len = strlen(path) + 1;
		iov[1].iov_base = token;
		iov[1].iov_len = strlen(token) + 1;

		return xs_error(xs_talkv(XBT_NIL, XS_UNWATCH, iov,
								 ARRAY_SIZE(iov), NULL));
}

static struct xenbus_watch *find_watch(const char *token)
{
		struct xenbus_watch *i, *cmp;

		cmp = (void *)simple_strtoul(token, NULL, 16);

		LIST_FOREACH(i, &watches, list)
				if (i == cmp)
						return i;

		return NULL;
}

/* Register callback to watch this node. */
int register_xenbus_watch(struct xenbus_watch *watch)
{
		/* Pointer in ascii is the token. */
		char token[sizeof(watch) * 2 + 1];
		int err;

		sprintf(token, "%lX", (long)watch);

		down_read(&xs_state.suspend_mutex);

		mtx_lock(&watches_lock);
		BUG_ON(find_watch(token) != NULL);
		LIST_INSERT_HEAD(&watches, watch, list);
		mtx_unlock(&watches_lock);

		err = xs_watch(watch->node, token);

		/* Ignore errors due to multiple registration. */
		if ((err != 0) && (err != -EEXIST)) {
				mtx_lock(&watches_lock);
				LIST_REMOVE(watch, list);
				mtx_unlock(&watches_lock);
		}

		up_read(&xs_state.suspend_mutex);

		return err;
}
EXPORT_SYMBOL(register_xenbus_watch);

void unregister_xenbus_watch(struct xenbus_watch *watch)
{
		struct xs_stored_msg *msg, *tmp;
		char token[sizeof(watch) * 2 + 1];
		int err;

		sprintf(token, "%lX", (long)watch);

		down_read(&xs_state.suspend_mutex);

		mtx_lock(&watches_lock);
		BUG_ON(!find_watch(token));
		LIST_REMOVE(watch, list);
		mtx_unlock(&watches_lock);

		err = xs_unwatch(watch->node, token);
		if (err)
				log(LOG_WARNING, "XENBUS Failed to release watch %s: %i\n",
					   watch->node, err);

		up_read(&xs_state.suspend_mutex);

		/* Cancel pending watch events. */
		mtx_lock(&watch_events_lock);
		TAILQ_FOREACH_SAFE(msg, &watch_events, list, tmp) {
				if (msg->u.watch.handle != watch)
						continue;
				list_del(&watch_events, msg);
				kfree(msg->u.watch.vec);
				kfree(msg);
		}
		mtx_unlock(&watch_events_lock);

		/* Flush any currently-executing callback, unless we are it. :-) */
		if (curproc->p_pid != xenwatch_pid) {
				sx_xlock(&xenwatch_mutex);
				sx_xunlock(&xenwatch_mutex);
		}
}
EXPORT_SYMBOL(unregister_xenbus_watch);

void xs_suspend(void)
{
		down_write(&xs_state.suspend_mutex);
		sx_xlock(&xs_state.request_mutex);
}

void xs_resume(void)
{
		struct xenbus_watch *watch;
		char token[sizeof(watch) * 2 + 1];

		sx_xunlock(&xs_state.request_mutex);

		/* No need for watches_lock: the suspend_mutex is sufficient. */
		LIST_FOREACH(watch, &watches, list) {
				sprintf(token, "%lX", (long)watch);
				xs_watch(watch->node, token);
		}

		up_write(&xs_state.suspend_mutex);
}

static void xenwatch_thread(void *unused)
{
		struct xs_stored_msg *msg;

		DELAY(10000);
		xenwatch_running = 1;
		for (;;) {

				while (list_empty(&watch_events))
						tsleep(&watch_events_waitq,
							   PWAIT | PCATCH, "waitev", hz/10);
				
				sx_xlock(&xenwatch_mutex);

				mtx_lock(&watch_events_lock);
				msg = TAILQ_FIRST(&watch_events);
				if (msg)
						list_del(&watch_events, msg);
				mtx_unlock(&watch_events_lock);

				if (msg != NULL) {
						printf("handling watch\n");
						msg->u.watch.handle->callback(
								msg->u.watch.handle,
								(const char **)msg->u.watch.vec,
								msg->u.watch.vec_size);
						kfree(msg->u.watch.vec);
						kfree(msg);
				}

				sx_xunlock(&xenwatch_mutex);
		}
}

static int xs_process_msg(enum xsd_sockmsg_type *type)
{
		struct xs_stored_msg *msg;
		char *body;
		int err;
		
		msg = kmalloc(sizeof(*msg), GFP_KERNEL);
		if (msg == NULL)
				return -ENOMEM;
		
		err = xb_read(&msg->hdr, sizeof(msg->hdr));
		if (err) {
				kfree(msg);
				return err;
		}

		body = kmalloc(msg->hdr.len + 1, GFP_KERNEL);
		if (body == NULL) {
				kfree(msg);
				return -ENOMEM;
		}
		
		err = xb_read(body, msg->hdr.len);
		if (err) {
				kfree(body);
				kfree(msg);
				return err;
		}
		body[msg->hdr.len] = '\0';

		*type = msg->hdr.type;
		if (msg->hdr.type == XS_WATCH_EVENT) {
				msg->u.watch.vec = split(body, msg->hdr.len,
										 &msg->u.watch.vec_size);
				if (IS_ERR(msg->u.watch.vec)) {
						kfree(msg);
						return PTR_ERR(msg->u.watch.vec);
				}
				
				mtx_lock(&watches_lock);
				msg->u.watch.handle = find_watch(
						msg->u.watch.vec[XS_WATCH_TOKEN]);
				if (msg->u.watch.handle != NULL) {
						mtx_lock(&watch_events_lock);
						TAILQ_INSERT_TAIL(&watch_events, msg, list);
						wakeup(&watch_events_waitq);
						mtx_unlock(&watch_events_lock);
				} else {
						kfree(msg->u.watch.vec);
						kfree(msg);
				}
				mtx_unlock(&watches_lock);
		} else {
				printf("event=%d ", *type);
				msg->u.reply.body = body;
				mtx_lock(&xs_state.reply_lock);
				TAILQ_INSERT_TAIL(&xs_state.reply_list, msg, list);
				wakeup(&xs_state.reply_waitq);
				mtx_unlock(&xs_state.reply_lock);
		}
		if (*type == XS_WATCH_EVENT)
				printf("\n");
		
		return 0;
}

static void xenbus_thread(void *unused)
{
		int err;
		enum xsd_sockmsg_type type;

		DELAY(10000);
		xenbus_running = 1;
		pause("xenbus", hz/10);

		for (;;) {
				err = xs_process_msg(&type);
				if (err) 
						printf("XENBUS error %d while reading "
							   "message\n", err);

		}
}

int xs_init(void)
{
		int err;
		struct proc *p;

		TAILQ_INIT(&xs_state.reply_list);
		TAILQ_INIT(&watch_events);
		sx_init(&xenwatch_mutex, "xenwatch");

		
		mtx_init(&xs_state.reply_lock, "state reply", NULL, MTX_DEF);
		sx_init(&xs_state.request_mutex, "xenstore request");
		sema_init(&xs_state.suspend_mutex, 1, "xenstore suspend");

		
#if 0
		mtx_init(&xs_state.suspend_mutex, "xenstore suspend", NULL, MTX_DEF);
		sema_init(&xs_state.request_mutex, 1, "xenstore request");
		sema_init(&xenwatch_mutex, 1, "xenwatch");
#endif
		mtx_init(&watches_lock, "watches", NULL, MTX_DEF);
		mtx_init(&watch_events_lock, "watch events", NULL, MTX_DEF);
   
		/* Initialize the shared memory rings to talk to xenstored */
		err = xb_init_comms();
		if (err)
				return err;

		err = kproc_create(xenwatch_thread, NULL, &p,
							 RFHIGHPID, 0, "xenwatch");
		if (err)
				return err;
		xenwatch_pid = p->p_pid;

		err = kproc_create(xenbus_thread, NULL, NULL, 
							 RFHIGHPID, 0, "xenbus");
	
		return err;
}


/*
 * Local variables:
 *  c-file-style: "bsd"
 *  indent-tabs-mode: t
 *  c-indent-level: 4
 *  c-basic-offset: 8
 *  tab-width: 4
 * End:
 */
