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
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/unistd.h>

#include <machine/xen/xen-os.h>
#include <xen/hypervisor.h>
#include <machine/stdarg.h>

#include <xen/xenbus/xenbusvar.h>
#include <xen/xenbus/xenbus_comms.h>
#include <xen/interface/hvm/params.h>

#include <vm/vm.h>
#include <vm/pmap.h>

static int xs_process_msg(enum xsd_sockmsg_type *type);

int xenwatch_running = 0;
int xenbus_running = 0;
int xen_store_evtchn;

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
	struct sx suspend_mutex;
};

static struct xs_handle xs_state;

/* List of registered watches, and a lock to protect it. */
static LIST_HEAD(watch_list_head, xenbus_watch) watches;
static struct mtx watches_lock;
/* List of pending watch callback events, and a lock to protect it. */
static TAILQ_HEAD(event_list_head, xs_stored_msg) watch_events;
static struct mtx watch_events_lock;

/*
 * Details of the xenwatch callback kernel thread. The thread waits on the
 * watch_events_waitq for work to do (queued on watch_events list). When it
 * wakes up it acquires the xenwatch_mutex before reading the list and
 * carrying out work.
 */
static pid_t xenwatch_pid;
struct sx xenwatch_mutex;
static int watch_events_waitq;

#define xsd_error_count	(sizeof(xsd_errors) / sizeof(xsd_errors[0]))

static int
xs_get_error(const char *errorstring)
{
	unsigned int i;

	for (i = 0; i < xsd_error_count; i++) {
		if (!strcmp(errorstring, xsd_errors[i].errstring))
			return (xsd_errors[i].errnum);
	}
	log(LOG_WARNING, "XENBUS xen store gave: unknown error %s",
	    errorstring);
	return (EINVAL);
}

extern void kdb_backtrace(void);

static int
xs_read_reply(enum xsd_sockmsg_type *type, unsigned int *len, void **result)
{
	struct xs_stored_msg *msg;
	char *body;
	int error;

	mtx_lock(&xs_state.reply_lock);

	while (TAILQ_EMPTY(&xs_state.reply_list)) {
			while (TAILQ_EMPTY(&xs_state.reply_list)) {
				error = mtx_sleep(&xs_state.reply_waitq,
				    &xs_state.reply_lock,
				    PCATCH, "xswait", hz/10);
				if (error && error != EWOULDBLOCK) {
					mtx_unlock(&xs_state.reply_lock);
					return (error);
				}
				
			}
				
			
		}

		
	msg = TAILQ_FIRST(&xs_state.reply_list);
	TAILQ_REMOVE(&xs_state.reply_list, msg, list);

	mtx_unlock(&xs_state.reply_lock);

	*type = msg->hdr.type;
	if (len)
		*len = msg->hdr.len;
	body = msg->u.reply.body;

	free(msg, M_DEVBUF);
	*result = body;
	return (0);
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

int
xenbus_dev_request_and_reply(struct xsd_sockmsg *msg, void **result)
{
	struct xsd_sockmsg req_msg = *msg;
	int error;

	if (req_msg.type == XS_TRANSACTION_START)
		sx_slock(&xs_state.suspend_mutex);

	sx_xlock(&xs_state.request_mutex);

	error = xb_write(msg, sizeof(*msg) + msg->len, &xs_state.request_mutex.lock_object);
	if (error) {
		msg->type = XS_ERROR;
	} else {
		error = xs_read_reply(&msg->type, &msg->len, result);
	}

	sx_xunlock(&xs_state.request_mutex);

	if ((msg->type == XS_TRANSACTION_END) ||
	    ((req_msg.type == XS_TRANSACTION_START) &&
		(msg->type == XS_ERROR)))
		sx_sunlock(&xs_state.suspend_mutex);

	return (error);
}

/*
 * Send message to xs. The reply is returned in *result and should be
 * fred with free(*result, M_DEVBUF). Return zero on success or an
 * error code on failure.
 */
static int
xs_talkv(struct xenbus_transaction t, enum xsd_sockmsg_type type,
    const struct iovec *iovec, unsigned int num_vecs,
    unsigned int *len, void **result)
{
	struct xsd_sockmsg msg;
	void *ret = NULL;
	unsigned int i;
	int error;

	msg.tx_id = t.id;
	msg.req_id = 0;
	msg.type = type;
	msg.len = 0;
	for (i = 0; i < num_vecs; i++)
		msg.len += iovec[i].iov_len;

	sx_xlock(&xs_state.request_mutex);

	error = xb_write(&msg, sizeof(msg), &xs_state.request_mutex.lock_object);
	if (error) {
		sx_xunlock(&xs_state.request_mutex);
		printf("xs_talkv failed %d\n", error);
		return (error);
	}

	for (i = 0; i < num_vecs; i++) {
		error = xb_write(iovec[i].iov_base, iovec[i].iov_len, &xs_state.request_mutex.lock_object);
		if (error) {		
			sx_xunlock(&xs_state.request_mutex);
			printf("xs_talkv failed %d\n", error);
			return (error);
		}
	}

	error = xs_read_reply(&msg.type, len, &ret);

	sx_xunlock(&xs_state.request_mutex);

	if (error)
		return (error);

	if (msg.type == XS_ERROR) {
		error = xs_get_error(ret);
		free(ret, M_DEVBUF);
		return (error);
	}

#if 0
	if ((xenwatch_running == 0) && (xenwatch_inline == 0)) {
		xenwatch_inline = 1;
		while (!TAILQ_EMPTY(&watch_events) 
		    && xenwatch_running == 0) {
						
			struct xs_stored_msg *wmsg = TAILQ_FIRST(&watch_events);
			TAILQ_REMOVE(&watch_events, wmsg, list);
						
			wmsg->u.watch.handle->callback(
				wmsg->u.watch.handle,
				(const char **)wmsg->u.watch.vec,
				wmsg->u.watch.vec_size);
			free(wmsg->u.watch.vec, M_DEVBUF);
			free(wmsg, M_DEVBUF);
		}
		xenwatch_inline = 0;
	}
#endif
	KASSERT(msg.type == type, ("bad xenstore message type"));

	if (result)
		*result = ret;
	else
		free(ret, M_DEVBUF);

	return (0);
}

/* Simplified version of xs_talkv: single message. */
static int
xs_single(struct xenbus_transaction t, enum xsd_sockmsg_type type,
    const char *string, unsigned int *len, void **result)
{
	struct iovec iovec;

	iovec.iov_base = (void *)(uintptr_t) string;
	iovec.iov_len = strlen(string) + 1;

	return (xs_talkv(t, type, &iovec, 1, len, result));
}

static unsigned int
count_strings(const char *strings, unsigned int len)
{
	unsigned int num;
	const char *p;

	for (p = strings, num = 0; p < strings + len; p += strlen(p) + 1)
		num++;

	return num;
}

/* Return the path to dir with /name appended. Buffer must be kfree()'ed. */ 
static char *
join(const char *dir, const char *name)
{
	char *buffer;

	buffer = malloc(strlen(dir) + strlen("/") + strlen(name) + 1,
	    M_DEVBUF, M_WAITOK);

	strcpy(buffer, dir);
	if (strcmp(name, "")) {
		strcat(buffer, "/");
		strcat(buffer, name);
	}

	return (buffer);
}

static char **
split(char *strings, unsigned int len, unsigned int *num)
{
	char *p, **ret;

	/* Count the strings. */
	*num = count_strings(strings, len) + 1;

	/* Transfer to one big alloc for easy freeing. */
	ret = malloc(*num * sizeof(char *) + len, M_DEVBUF, M_WAITOK);
	memcpy(&ret[*num], strings, len);
	free(strings, M_DEVBUF);

	strings = (char *)&ret[*num];
	for (p = strings, *num = 0; p < strings + len; p += strlen(p) + 1)
		ret[(*num)++] = p;

	ret[*num] = strings + len;
		
	return ret;
}

/*
 * Return the contents of a directory in *result which should be freed
 * with free(*result, M_DEVBUF).
 */
int
xenbus_directory(struct xenbus_transaction t, const char *dir,
    const char *node, unsigned int *num, char ***result)
{
	char *strings, *path;
	unsigned int len = 0;
	int error;

	path = join(dir, node);
	error = xs_single(t, XS_DIRECTORY, path, &len, (void **) &strings);
	free(path, M_DEVBUF);
	if (error)
		return (error);

	*result = split(strings, len, num);
	return (0);
}

/*
 * Check if a path exists. Return 1 if it does.
 */
int
xenbus_exists(struct xenbus_transaction t, const char *dir, const char *node)
{
	char **d;
	int error, dir_n;

	error = xenbus_directory(t, dir, node, &dir_n, &d);
	if (error)
		return (0);
	free(d, M_DEVBUF);
	return (1);
}

/*
 * Get the value of a single file.  Returns the contents in *result
 * which should be freed with free(*result, M_DEVBUF) after use.
 * The length of the value in bytes is returned in *len.
 */
int
xenbus_read(struct xenbus_transaction t, const char *dir, const char *node,
    unsigned int *len, void **result)
{
	char *path;
	void *ret;
	int error;

	path = join(dir, node);
	error = xs_single(t, XS_READ, path, len, &ret);
	free(path, M_DEVBUF);
	if (error)
		return (error);
	*result = ret;
	return (0);
}

/*
 * Write the value of a single file.  Returns error on failure.
 */
int
xenbus_write(struct xenbus_transaction t, const char *dir, const char *node,
    const char *string)
{
	char *path;
	struct iovec iovec[2];
	int error;

	path = join(dir, node);

	iovec[0].iov_base = (void *)(uintptr_t) path;
	iovec[0].iov_len = strlen(path) + 1;
	iovec[1].iov_base = (void *)(uintptr_t) string;
	iovec[1].iov_len = strlen(string);

	error = xs_talkv(t, XS_WRITE, iovec, 2, NULL, NULL);
	free(path, M_DEVBUF);

	return (error);
}

/*
 * Create a new directory.
 */
int
xenbus_mkdir(struct xenbus_transaction t, const char *dir, const char *node)
{
	char *path;
	int ret;

	path = join(dir, node);
	ret = xs_single(t, XS_MKDIR, path, NULL, NULL);
	free(path, M_DEVBUF);

	return (ret);
}

/*
 * Destroy a file or directory (directories must be empty).
 */
int
xenbus_rm(struct xenbus_transaction t, const char *dir, const char *node)
{
	char *path;
	int ret;

	path = join(dir, node);
	ret = xs_single(t, XS_RM, path, NULL, NULL);
	free(path, M_DEVBUF);

	return (ret);
}

/*
 * Start a transaction: changes by others will not be seen during this
 * transaction, and changes will not be visible to others until end.
 */
int
xenbus_transaction_start(struct xenbus_transaction *t)
{
	char *id_str;
	int error;

	sx_slock(&xs_state.suspend_mutex);
	error = xs_single(XBT_NIL, XS_TRANSACTION_START, "", NULL,
	    (void **) &id_str);
	if (error) {
		sx_sunlock(&xs_state.suspend_mutex);
		return (error);
	}

	t->id = strtoul(id_str, NULL, 0);
	free(id_str, M_DEVBUF);

	return (0);
}

/*
 * End a transaction.  If abandon is true, transaction is discarded
 * instead of committed.
 */
int xenbus_transaction_end(struct xenbus_transaction t, int abort)
{
	char abortstr[2];
	int error;

	if (abort)
		strcpy(abortstr, "F");
	else
		strcpy(abortstr, "T");

	error = xs_single(t, XS_TRANSACTION_END, abortstr, NULL, NULL);
		
	sx_sunlock(&xs_state.suspend_mutex);

	return (error);
}

/* Single read and scanf: returns zero or errno. */
int
xenbus_scanf(struct xenbus_transaction t,
    const char *dir, const char *node, int *scancountp, const char *fmt, ...)
{
	va_list ap;
	int error, ns;
	char *val;

	error = xenbus_read(t, dir, node, NULL, (void **) &val);
	if (error)
		return (error);

	va_start(ap, fmt);
	ns = vsscanf(val, fmt, ap);
	va_end(ap);
	free(val, M_DEVBUF);
	/* Distinctive errno. */
	if (ns == 0)
		return (ERANGE);
	if (scancountp)
		*scancountp = ns;
	return (0);
}

/* Single printf and write: returns zero or errno. */
int
xenbus_printf(struct xenbus_transaction t,
    const char *dir, const char *node, const char *fmt, ...)
{
	va_list ap;
	int error, ret;
#define PRINTF_BUFFER_SIZE 4096
	char *printf_buffer;

	printf_buffer = malloc(PRINTF_BUFFER_SIZE, M_DEVBUF, M_WAITOK);

	va_start(ap, fmt);
	ret = vsnprintf(printf_buffer, PRINTF_BUFFER_SIZE, fmt, ap);
	va_end(ap);

	KASSERT(ret <= PRINTF_BUFFER_SIZE-1, ("xenbus_printf: message too large"));
	error = xenbus_write(t, dir, node, printf_buffer);

	free(printf_buffer, M_DEVBUF);

	return (error);
}

/* Takes tuples of names, scanf-style args, and void **, NULL terminated. */
int
xenbus_gather(struct xenbus_transaction t, const char *dir, ...)
{
	va_list ap;
	const char *name;
	int error, i;

	for (i = 0; i < 10000; i++)
		HYPERVISOR_yield();
		
	va_start(ap, dir);
	error = 0;
	while (error == 0 && (name = va_arg(ap, char *)) != NULL) {
		const char *fmt = va_arg(ap, char *);
		void *result = va_arg(ap, void *);
		char *p;

		error = xenbus_read(t, dir, name, NULL, (void **) &p);
		if (error)
			break;

		if (fmt) {
			if (sscanf(p, fmt, result) == 0)
				error = EINVAL;
			free(p, M_DEVBUF);
		} else
			*(char **)result = p;
	}
	va_end(ap);

	return (error);
}

static int
xs_watch(const char *path, const char *token)
{
	struct iovec iov[2];

	iov[0].iov_base = (void *)(uintptr_t) path;
	iov[0].iov_len = strlen(path) + 1;
	iov[1].iov_base = (void *)(uintptr_t) token;
	iov[1].iov_len = strlen(token) + 1;

	return (xs_talkv(XBT_NIL, XS_WATCH, iov, 2, NULL, NULL));
}

static int
xs_unwatch(const char *path, const char *token)
{
	struct iovec iov[2];

	iov[0].iov_base = (void *)(uintptr_t) path;
	iov[0].iov_len = strlen(path) + 1;
	iov[1].iov_base = (void *)(uintptr_t) token;
	iov[1].iov_len = strlen(token) + 1;

	return (xs_talkv(XBT_NIL, XS_UNWATCH, iov, 2, NULL, NULL));
}

static struct xenbus_watch *
find_watch(const char *token)
{
	struct xenbus_watch *i, *cmp;

	cmp = (void *)strtoul(token, NULL, 16);

	LIST_FOREACH(i, &watches, list)
		if (i == cmp)
			return (i);

	return (NULL);
}

/* Register callback to watch this node. */
int
register_xenbus_watch(struct xenbus_watch *watch)
{
	/* Pointer in ascii is the token. */
	char token[sizeof(watch) * 2 + 1];
	int error;

	sprintf(token, "%lX", (long)watch);

	sx_slock(&xs_state.suspend_mutex);

	mtx_lock(&watches_lock);
	KASSERT(find_watch(token) == NULL, ("watch already registered"));
	LIST_INSERT_HEAD(&watches, watch, list);
	mtx_unlock(&watches_lock);

	error = xs_watch(watch->node, token);
		
	/* Ignore errors due to multiple registration. */
	if (error == EEXIST) {
		mtx_lock(&watches_lock);
		LIST_REMOVE(watch, list);
		mtx_unlock(&watches_lock);
	}

	sx_sunlock(&xs_state.suspend_mutex);

	return (error);
}

void
unregister_xenbus_watch(struct xenbus_watch *watch)
{
	struct xs_stored_msg *msg, *tmp;
	char token[sizeof(watch) * 2 + 1];
	int error;

	sprintf(token, "%lX", (long)watch);
		
	sx_slock(&xs_state.suspend_mutex);

	mtx_lock(&watches_lock);
	KASSERT(find_watch(token), ("watch not registered"));
	LIST_REMOVE(watch, list);
	mtx_unlock(&watches_lock);

	error = xs_unwatch(watch->node, token);
	if (error)
		log(LOG_WARNING, "XENBUS Failed to release watch %s: %i\n",
		    watch->node, error);

	sx_sunlock(&xs_state.suspend_mutex);

	/* Cancel pending watch events. */
	mtx_lock(&watch_events_lock);
	TAILQ_FOREACH_SAFE(msg, &watch_events, list, tmp) {
		if (msg->u.watch.handle != watch)
			continue;
		TAILQ_REMOVE(&watch_events, msg, list);
		free(msg->u.watch.vec, M_DEVBUF);
		free(msg, M_DEVBUF);
	}
	mtx_unlock(&watch_events_lock);

	/* Flush any currently-executing callback, unless we are it. :-) */
	if (curproc->p_pid != xenwatch_pid) {
		sx_xlock(&xenwatch_mutex);
		sx_xunlock(&xenwatch_mutex);
	}
}

void
xs_suspend(void)
{	

	sx_xlock(&xs_state.suspend_mutex);
	sx_xlock(&xs_state.request_mutex);
}

void
xs_resume(void)
{
	struct xenbus_watch *watch;
	char token[sizeof(watch) * 2 + 1];

	sx_xunlock(&xs_state.request_mutex);

	/* No need for watches_lock: the suspend_mutex is sufficient. */
	LIST_FOREACH(watch, &watches, list) {
		sprintf(token, "%lX", (long)watch);
		xs_watch(watch->node, token);
	}

	sx_xunlock(&xs_state.suspend_mutex);
}

static void
xenwatch_thread(void *unused)
{
	struct xs_stored_msg *msg;

	for (;;) {

		mtx_lock(&watch_events_lock);
		while (TAILQ_EMPTY(&watch_events))
			mtx_sleep(&watch_events_waitq,
			    &watch_events_lock,
			    PWAIT | PCATCH, "waitev", hz/10);

		mtx_unlock(&watch_events_lock);
		sx_xlock(&xenwatch_mutex);

		mtx_lock(&watch_events_lock);
		msg = TAILQ_FIRST(&watch_events);
		if (msg)
			TAILQ_REMOVE(&watch_events, msg, list);
		mtx_unlock(&watch_events_lock);

		if (msg != NULL) {
			msg->u.watch.handle->callback(
				msg->u.watch.handle,
				(const char **)msg->u.watch.vec,
				msg->u.watch.vec_size);
			free(msg->u.watch.vec, M_DEVBUF);
			free(msg, M_DEVBUF);
		}

		sx_xunlock(&xenwatch_mutex);
	}
}

static int
xs_process_msg(enum xsd_sockmsg_type *type)
{
	struct xs_stored_msg *msg;
	char *body;
	int error;
		
	msg = malloc(sizeof(*msg), M_DEVBUF, M_WAITOK);
	mtx_lock(&xs_state.reply_lock);
	error = xb_read(&msg->hdr, sizeof(msg->hdr), &xs_state.reply_lock.lock_object);
	mtx_unlock(&xs_state.reply_lock);
	if (error) {
		free(msg, M_DEVBUF);
		return (error);
	}

	body = malloc(msg->hdr.len + 1, M_DEVBUF, M_WAITOK);
	mtx_lock(&xs_state.reply_lock);
	error = xb_read(body, msg->hdr.len, &xs_state.reply_lock.lock_object); 
	mtx_unlock(&xs_state.reply_lock);
	if (error) {
		free(body, M_DEVBUF);
		free(msg, M_DEVBUF);
		return (error);
	}
	body[msg->hdr.len] = '\0';

	*type = msg->hdr.type;
	if (msg->hdr.type == XS_WATCH_EVENT) {
		msg->u.watch.vec = split(body, msg->hdr.len,
		    &msg->u.watch.vec_size);
				
		mtx_lock(&watches_lock);
		msg->u.watch.handle = find_watch(
			msg->u.watch.vec[XS_WATCH_TOKEN]);
		if (msg->u.watch.handle != NULL) {
			mtx_lock(&watch_events_lock);
			TAILQ_INSERT_TAIL(&watch_events, msg, list);
			wakeup(&watch_events_waitq);
			mtx_unlock(&watch_events_lock);
		} else {
			free(msg->u.watch.vec, M_DEVBUF);
			free(msg, M_DEVBUF);
		}
		mtx_unlock(&watches_lock);
	} else {
		msg->u.reply.body = body;
		mtx_lock(&xs_state.reply_lock);
		TAILQ_INSERT_TAIL(&xs_state.reply_list, msg, list);
		wakeup(&xs_state.reply_waitq);
		mtx_unlock(&xs_state.reply_lock);
	}
		
	return 0;
}

static void
xenbus_thread(void *unused)
{
	int error;
	enum xsd_sockmsg_type type;
	xenbus_running = 1;

	for (;;) {
		error = xs_process_msg(&type);
		if (error) 
			printf("XENBUS error %d while reading message\n",
			    error);
	}
}

#ifdef XENHVM
static unsigned long xen_store_mfn;
char *xen_store;

static inline unsigned long
hvm_get_parameter(int index)
{
	struct xen_hvm_param xhv;
	int error;
	
	xhv.domid = DOMID_SELF;
	xhv.index = index;
	error = HYPERVISOR_hvm_op(HVMOP_get_param, &xhv);
	if (error) {
		printf("hvm_get_parameter: failed to get %d, error %d\n",
		    index, error);
		return (0);
	}
	return (xhv.value);
}

#endif

int
xs_init(void)
{
	int error;
	struct proc *p;

#ifdef XENHVM
	xen_store_evtchn = hvm_get_parameter(HVM_PARAM_STORE_EVTCHN);
	xen_store_mfn = hvm_get_parameter(HVM_PARAM_STORE_PFN);
	xen_store = pmap_mapdev(xen_store_mfn * PAGE_SIZE, PAGE_SIZE);
#else
	xen_store_evtchn = xen_start_info->store_evtchn;
#endif

	TAILQ_INIT(&xs_state.reply_list);
	TAILQ_INIT(&watch_events);
	sx_init(&xenwatch_mutex, "xenwatch");

		
	mtx_init(&xs_state.reply_lock, "state reply", NULL, MTX_DEF);
	sx_init(&xs_state.request_mutex, "xenstore request");
	sx_init(&xs_state.suspend_mutex, "xenstore suspend");

		
#if 0
	mtx_init(&xs_state.suspend_mutex, "xenstore suspend", NULL, MTX_DEF);
	sema_init(&xs_state.request_mutex, 1, "xenstore request");
	sema_init(&xenwatch_mutex, 1, "xenwatch");
#endif
	mtx_init(&watches_lock, "watches", NULL, MTX_DEF);
	mtx_init(&watch_events_lock, "watch events", NULL, MTX_DEF);
   
	/* Initialize the shared memory rings to talk to xenstored */
	error = xb_init_comms();
	if (error)
		return (error);

	xenwatch_running = 1;
	error = kproc_create(xenwatch_thread, NULL, &p,
	    RFHIGHPID, 0, "xenwatch");
	if (error)
		return (error);
	xenwatch_pid = p->p_pid;

	error = kproc_create(xenbus_thread, NULL, NULL, 
	    RFHIGHPID, 0, "xenbus");
	
	return (error);
}
