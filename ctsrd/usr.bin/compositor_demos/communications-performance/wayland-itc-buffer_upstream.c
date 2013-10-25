/*
 * Copyright © 2013 Philip Withnall
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "../config.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "wayland-capabilities.h"
#include "wayland-itc-buffer.h"
#include "wayland-util.h"


/**
 * Ring buffer to replace sendmsg()/recvmsg() calls between threads, with the
 * aim of eliminating context switches into/out of the kernel.
 *
 * This code is thread-safe.
 *
 * Currently a pthread condition variable is used for synchronisation between
 * readers and writers; however, this could potentially be replaced by a raw
 * futex if it's available on FreeBSD.
 *
 * TODO: Make the ITC buffer zero-copy.
 *
 * Capability design for the ITC buffer:
 *
 * The ITC buffer is self-contained, but many of its methods need to make system
 * calls (e.g. for pthreads), which would normally require ambient authority for
 * the entire address space. However, modifications have been made to the
 * cheribsd kernel to allow umtx syscalls to proceed even without ambient
 * authority, which should be enough to satisfy pthreads.
 *
 * On allocation (wl_itc_buffer_create()), one capability is produced for the
 * buffer's metadata (the struct wl_itc_buffer). This is not stored in the
 * buffer itself, but must be passed to all functions which operate on the
 * buffer.
 *
 * Each ITC buffer has its own malloc() arena, which it allocates all storage
 * messages in. This is necessary because messages may otherwise be allocated in
 * one thread-local arena, sent over the buffer, and then freed in another
 * thread-local arena, which would cause crashes and pain. Message allocations
 * are thus protected under the arena's capability.
 *
 * TODO: A massive HACK is needed at the moment to allow pthreads functions to
 * be called (specifically, mutex functions). They have to be called with full
 * ambient authority to allow them access to both the stack and the mutex,
 * which is allocated in a heap somewhere as part of the ITC buffer. Just
 * giving them access to the region protected by the ITC buffer capability is
 * not enough. A suggested future solution is call-gating all mutex operations.
 */


struct wl_itc_buffer_msg {
	void *data;
	uint8_t data_static[64];
	size_t data_length;
	void *control;
	size_t control_length;
	int control_level;
	int control_type;
};

struct wl_itc_buffer {
	struct wl_arena arena;
#ifdef CP2
	struct chericap arena_cap;
	struct chericap full_ambient_cap; /* HACK! See note above. */
#endif /* CP2 */
	unsigned int ref_count;

	pthread_cond_t cond;
	pthread_mutex_t mutex;

	unsigned int head; /* first used byte (and next to be popped) */
	unsigned int tail; /* first free byte (and next to be pushed) */
	unsigned int slots;
	struct wl_itc_buffer_msg msgs[0];
};


#define WL_ITC_BUFFER_DEFAULT_SLOTS 128

/* Debug messages. */
#ifdef DEBUG_ITC
#define ITC_DEBUG(M, ...) \
	fprintf(stderr, "itc: %li: " M, (long unsigned int) pthread_self(), \
	        __VA_ARGS__)
#else
#define ITC_DEBUG(...)
#endif

/* Macros to change the ambient authority to that given to the macro. These
 * provide a quick way to give permission to modify an ITC buffer.
 *
 * They clobber $c1. */
#ifdef CP2
#define ITC_AMBIENT_BEGIN(C) \
do { \
	/* Move the passed-in capability into $c1 for the call's duration. */ \
	CHERI_CLC(1, 0, C, 0); \
} while (0)
#define ITC_AMBIENT_END(C) \
do { \
	/* Nuke $c1. */ \
	CHERI_CCLEARTAG(1); \
} while (0)
#define PTHREADS_AMBIENT_BEGIN(B) \
do { \
	/* Save the current $c0 into $c2, and copy the full ambient authority \
	 * into $c0. */ \
	CHERI_CMOVE(2, 0); \
	CHERI_CLC(0, 0, &(B)->full_ambient_cap, 0); \
} while (0)
#define PTHREADS_AMBIENT_END(B) \
do { \
	/* Restore $c0 from $c2 and nuke $c2. */ \
	CHERI_CMOVE(0, 2); \
	CHERI_CCLEARTAG(2); \
} while (0)
#else /* if !CP2 */
#define ITC_AMBIENT_BEGIN(C) \
do { /* Do nothing. */ } while (0)
#define ITC_AMBIENT_END(C) \
do { /* Do nothing. */ } while (0)
#define PTHREADS_AMBIENT_BEGIN(B) \
do { /* Do nothing. */ } while (0)
#define PTHREADS_AMBIENT_END(B) \
do { /* Do nothing. */ } while (0)
#endif /* !CP2 */

/* Macros to load and store members of a wl_itc_buffer using capabilities.
 * These will only work inside a ITC_AMBIENT_[BEGIN|END]() pair. */
#ifdef CP2
#define ITC_LOAD_MEMBER64(dest, buf, member) \
	CHERI_CLD(dest, 0, offsetof(struct wl_itc_buffer, member), 1)
#define ITC_LOAD_MEMBER32(dest, buf, member) \
	CHERI_CLW(dest, 0, offsetof(struct wl_itc_buffer, member), 1)
#define ITC_STORE_MEMBER64(val, buf, member) \
	CHERI_CSD(val, 0, offsetof(struct wl_itc_buffer, member), 1)
#define ITC_STORE_MEMBER32(val, buf, member) \
	CHERI_CSW(val, 0, offsetof(struct wl_itc_buffer, member), 1)
#define ITC_MSG_LOAD_MEMBER64(dest, buf, msg, member) \
	CHERI_CLD(dest, (uintptr_t) msg - (uintptr_t) buf, \
	          offsetof(struct wl_itc_buffer_msg, member), 1)
#define ITC_MSG_LOAD_MEMBER32(dest, buf, msg, member) \
	CHERI_CLW(dest, (uintptr_t) msg - (uintptr_t) buf, \
	          offsetof(struct wl_itc_buffer_msg, member), 1)
#define ITC_MSG_STORE_MEMBER64(val, buf, msg, member) \
	CHERI_CSD(val, (uintptr_t) msg - (uintptr_t) buf, \
	          offsetof(struct wl_itc_buffer_msg, member), 1)
#define ITC_MSG_STORE_MEMBER32(val, buf, msg, member) \
	CHERI_CSW(val, (uintptr_t) msg - (uintptr_t) buf, \
	          offsetof(struct wl_itc_buffer_msg, member), 1)
#else /* if !CP2 */
#define ITC_LOAD_MEMBER64(dest, buf, member) \
	dest = buf->member
#define ITC_LOAD_MEMBER32(dest, buf, member) \
	dest = buf->member
#define ITC_STORE_MEMBER64(val, buf, member) \
	buf->member = val
#define ITC_STORE_MEMBER32(val, buf, member) \
	buf->member = val
#define ITC_MSG_LOAD_MEMBER64(dest, buf, msg, member) \
	dest = msg->member
#define ITC_MSG_LOAD_MEMBER32(dest, buf, msg, member) \
	dest = msg->member
#define ITC_MSG_STORE_MEMBER64(val, buf, msg, member) \
	msg->member = val
#define ITC_MSG_STORE_MEMBER32(val, buf, msg, member) \
	msg->member = val
#endif /* !CP2 */

/* This requires ambient authority for Permit_Store_Capability in cap_out. It
 * also requires the ability to allocate memory.
 *
 * Note: cap_out must be 32-byte aligned.
 */
#ifdef CP2
WL_EXPORT struct wl_itc_buffer *
wl_itc_buffer_create (unsigned int slots, struct chericap *cap_out)
#else /* if !CP2 */
WL_EXPORT struct wl_itc_buffer *
wl_itc_buffer_create (unsigned int slots)
#endif /* !CP2 */
{
	struct wl_itc_buffer *buf;
	size_t buf_size, arena_size;
	void *arena_base;

	if (slots == 0)
		slots = WL_ITC_BUFFER_DEFAULT_SLOTS;

	/* We need to temporarily grant ourselves Permit_Store_Capability so
	 * that we can write to buf->arena_cap, below. */
	buf_size = sizeof(struct wl_itc_buffer) +
	           slots * sizeof(struct wl_itc_buffer_msg);
	buf = WL_MALLOC_WITH_CAPABILITY(buf_size,
	                                CHERI_PERM_STORE | CHERI_PERM_LOAD,
	                                cap_out);

	if (buf == NULL)
		return NULL;

	ITC_DEBUG("wl_itc_buffer_create(%u) = %p\n", slots, buf);

	/* Initialise the buffer. */
	buf->head = 0;
	buf->tail = 0;
	buf->slots = slots;
	buf->ref_count = 2; /* one for each end of the pipe */

	/* Initialise the mutex & condition. */
	if (pthread_mutex_init(&buf->mutex, NULL) < 0) {
		fprintf(stderr, "failed to initialise ITC mutex: %s\n",
		        strerror(errno));
		goto error;
	}

	if (pthread_cond_init(&buf->cond, NULL) < 0) {
		fprintf(stderr, "failed to initialise ITC cond: %s\n",
		        strerror(errno));
		goto error;
	}

	/* Set up a buffer-local arena for the buffer. */
	arena_size = 4 * 1024 * 1024; /* arbitrary */
	arena_base =
		WL_MALLOC_WITH_CAPABILITY(arena_size,
		                          CHERI_PERM_STORE |
		                          CHERI_PERM_LOAD,
		                          &buf->arena.cap);
#ifndef CP2
	buf->arena.base = arena_base;
	buf->arena.size = arena_size;
#endif /* !CP2 */

	if (WL_ARENA_BASE(&buf->arena) == NULL) {
		fprintf(stderr, "failed to allocate ITC buffer arena.\n");
		goto error;
	}

#ifdef CP2
	/* Restrict cap_out to no longer allow Permit_Store_Capability, now that
	 * we've written out buf->arena_cap. */
	cheri_capability_drop_perms(cap_out,
	                            CHERI_PERM_STORE_CAP |
				    CHERI_PERM_STORE_EPHEM_CAP);

	/* HACK! Save full ambient authority in the buffer. See the note at
	 * the top of the file. */
	CHERI_CSC(0, 0, &buf->full_ambient_cap, 0);
#endif /* CP2 */

	/* Set up our buffer-local arena. */
	wl_malloc_setup(&buf->arena);

	return buf;

error:
	pthread_cond_destroy(&buf->cond);
	pthread_mutex_destroy(&buf->mutex);
	free(buf);
#ifdef CP2
	memset(cap_out, 0, sizeof *cap_out);
#endif /* CP2 */

	return NULL;
}

static void
buffer_msg_destroy (struct wl_itc_buffer *buf,
                    struct wl_itc_buffer_msg *buf_msg)
{
	void *buf_msg_data, *buf_msg_control;
	size_t buf_msg_data_length;

#ifdef CP2
	/* Preserve $c1 across function calls. */
	CHERI_CMOVE(16, 1);
#endif /* CP2 */

	ITC_MSG_LOAD_MEMBER64(buf_msg_data_length, buf, buf_msg, data_length);
	ITC_MSG_LOAD_MEMBER64(buf_msg_data, buf, buf_msg, data);
	ITC_MSG_LOAD_MEMBER64(buf_msg_control, buf, buf_msg, control);

	if (buf_msg_data_length > sizeof(buf_msg->data_static)) {
		wl_free_full(buf_msg_data, &buf->arena);
	}
	wl_free_full(buf_msg_control, &buf->arena);

	ITC_MSG_STORE_MEMBER64(0, buf, buf_msg, data_length);
	ITC_MSG_STORE_MEMBER64(0, buf, buf_msg, control_length);

#ifdef CP2
	CHERI_CMOVE(1, 16);
#endif /* CP2 */
}

/* This clobbers $c1. cap must be 32-byte aligned. */
#ifdef CP2
WL_EXPORT void
wl_itc_buffer_destroy (struct wl_itc_buffer *buf, struct chericap *cap)
#else /* if !CP2 */
WL_EXPORT void
wl_itc_buffer_destroy (struct wl_itc_buffer *buf)
#endif /* !CP2 */
{
	unsigned int i, buf_slots, buf_ref_count, buf_head, buf_tail;
	void *arena_base;

	ITC_AMBIENT_BEGIN(cap);

	/* Already destroyed? */
	/* TODO */
#ifdef CP2
	CHERI_CLW(buf_slots, (uintptr_t) &buf->slots - (uintptr_t) buf, 0, 1);
#else
	ITC_LOAD_MEMBER32(buf_slots, buf, slots);
#endif
	ITC_LOAD_MEMBER32(buf_ref_count, buf, ref_count);

	if (buf_slots == 0 || buf_ref_count == 0)
		goto done;

	ITC_DEBUG("wl_itc_buffer_destroy(%p)\n", buf);

	/* Lock, decrease the reference count (with 0 signalling destruction),
	 * then broadcast to wake all threads blocked on recvmsg(). */
	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_lock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	buf_ref_count--;
	ITC_STORE_MEMBER32(buf_ref_count, buf, ref_count);

	if (buf_ref_count > 0) {
		/* Not destroyed yet. */
		PTHREADS_AMBIENT_BEGIN(buf);
		pthread_mutex_unlock(&buf->mutex);
		PTHREADS_AMBIENT_END(buf);

		goto done;
	}

	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_unlock(&buf->mutex);

	pthread_cond_broadcast(&buf->cond);

	/* TODO: The locking here is a little dodgy. */
	pthread_mutex_lock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	/* Free any remaining messages. */
	ITC_LOAD_MEMBER32(buf_head, buf, head);
	ITC_LOAD_MEMBER32(buf_tail, buf, tail);

	for (i = buf_head; i < buf_tail; i = (i + 1) % buf_slots) {
		buffer_msg_destroy(buf, &buf->msgs[i]);
	}

	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_unlock(&buf->mutex);

	/* Free the mutex & condition. */
	pthread_cond_destroy(&buf->cond);
	pthread_mutex_destroy(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	arena_base = WL_ARENA_BASE(&buf->arena);

	ITC_AMBIENT_END(cap);

	/* Free the buffer-local arena. */
	free(arena_base);

	free(buf);

	return;

done:
	ITC_AMBIENT_END(cap);
}

/* NOTE: This must be called with the mutex locked and inside an
 * ITC_AMBIENT_*() block. */
static size_t
calculate_free_slots (struct wl_itc_buffer *buf)
{
	unsigned int buf_head, buf_tail, buf_slots;
	
	ITC_LOAD_MEMBER32(buf_head, buf, head);
	ITC_LOAD_MEMBER32(buf_tail, buf, tail);
#ifdef CP2
	CHERI_CLW(buf_slots, (uintptr_t) &buf->slots - (uintptr_t) buf, 0, 1);
#else
	ITC_LOAD_MEMBER32(buf_slots, buf, slots);
#endif

	/* Subtract 1 from each due to buf_tail pointing to the next *free*
	 * byte. */
	if (buf_head == buf_tail)
		return buf_slots - 1;
	else if (buf_head < buf_tail)
		return buf_slots - (buf_tail - buf_head) - 1;
	else
		return buf_head - buf_tail - 1;
}

/* NOTE: This must be called with the mutex locked and inside an
 * ITC_AMBIENT*() block. */
static int
#ifdef CP2
buffer_is_empty (struct wl_itc_buffer *buf __unused)
#else
buffer_is_empty (struct wl_itc_buffer *buf)
#endif
{
	unsigned int buf_head, buf_tail;

	ITC_LOAD_MEMBER32(buf_head, buf, head);
	ITC_LOAD_MEMBER32(buf_tail, buf, tail);

	return (buf_head == buf_tail);
}

/* Drop-in replacement for sendmsg() with the same semantics. */
#ifdef CP2
WL_EXPORT ssize_t
wl_itc_buffer_sendmsg(struct wl_itc_buffer *buf, struct chericap *buf_cap,
                      const struct msghdr *msg, int flags)
#else /* if !CP2 */
WL_EXPORT ssize_t
wl_itc_buffer_sendmsg (struct wl_itc_buffer *buf, const struct msghdr *msg,
                       int flags)
#endif /* !CP2 */
{
	size_t msg_size, msg_controlsize, j;
	unsigned int i, buf_ref_count, buf_tail, buf_slots;
	struct wl_itc_buffer_msg *buf_msg;
	ssize_t retval = -1;
	void *buf_msg_data;

	ITC_DEBUG("wl_itc_buffer_sendmsg(%p)\n", buf);

	ITC_AMBIENT_BEGIN(buf_cap);

	/* Only support these flags for the moment. */
	if (flags != (MSG_NOSIGNAL | MSG_DONTWAIT)) {
		errno = EINVAL;
		fprintf(stderr, "invalid flags\n");
		goto done_unlocked;
	}

	/* Calculate the message size. The cast is necessary because Linux
	 * and FreeBSD arbitrarily differ in msg_iovlen signedness. Sigh. */
	msg_size = 0;
	for (i = 0; i < (unsigned int) msg->msg_iovlen; i++) {
		msg_size += msg->msg_iov[i].iov_len;
	}

	msg_controlsize = 0;
	if (msg->msg_control != NULL)
		msg_controlsize = msg->msg_controllen;

	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_lock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	/* Buffer being destroyed? */
	ITC_LOAD_MEMBER32(buf_ref_count, buf, ref_count);

	if (buf_ref_count == 0) {
		errno = EPIPE;
		goto done;
	}

	/* Not enough space free in the buffer? */
	if (calculate_free_slots(buf) < 1) {
		errno = EWOULDBLOCK;
		ITC_DEBUG("no free slots in buf %p\n", buf);
		goto done;
	}

	/* Allocate a message in the buffer. */
	ITC_LOAD_MEMBER32(buf_tail, buf, tail);
#ifdef CP2
	CHERI_CLW(buf_slots, (uintptr_t) &buf->slots - (uintptr_t) buf, 0, 1);
#else
	ITC_LOAD_MEMBER32(buf_slots, buf, slots);
#endif
	buf_msg = &buf->msgs[buf_tail];
	ITC_MSG_STORE_MEMBER64(msg_size, buf, buf_msg, data_length);

	if (msg_size > sizeof(buf_msg->data_static)) {
		buf_msg_data = wl_malloc_full(msg_size, &buf->arena);
		ITC_AMBIENT_BEGIN(buf_cap); /* refresh $c1 */

		if (buf_msg_data == NULL) {
			errno = ENOMEM;
			fprintf(stderr, "failed to allocate msg %lu\n",
			        msg_size);
			goto done;
		}
	} else if (msg_size > 0) {
		buf_msg_data = buf_msg->data_static;
	} else {
		buf_msg_data = NULL;
	}

	ITC_MSG_STORE_MEMBER64(buf_msg_data, buf, buf_msg, data);

	ITC_MSG_STORE_MEMBER64(msg_controlsize, buf, buf_msg, control_length);
	if (msg_controlsize > 0) {
		void *buf_msg_control;

		buf_msg_control = wl_malloc_full(msg_controlsize, &buf->arena);
		ITC_AMBIENT_BEGIN(buf_cap); /* refresh $c1 */
		ITC_MSG_STORE_MEMBER64(buf_msg_control, buf, buf_msg, control);

		if (buf_msg_control == NULL) {
			if (msg_size > sizeof(buf_msg->data_static)) {
				wl_free_full(buf_msg_data, &buf->arena);
				ITC_AMBIENT_BEGIN(buf_cap); /* refresh $c1 */
			}

			errno = ENOMEM;
			fprintf(stderr, "failed to allocate msg_control %lu\n",
			        msg_controlsize);
			goto done;
		}
	} else {
		ITC_MSG_STORE_MEMBER64(NULL, buf, buf_msg, control);
	}

	buf_tail = (buf_tail + 1) % buf_slots;
	ITC_STORE_MEMBER32(buf_tail, buf, tail);

	/* Process the message. We don't need to do any error checking on buffer
	 * space, since that's been done already. The cast is needed because
	 * Linux and FreeBSD arbitrarily differ in signedness. */
	j = 0;
	for (i = 0; i < (unsigned int) msg->msg_iovlen; i++) {
		memcpy((void *) ((uintptr_t) buf_msg_data + j),
		       msg->msg_iov[i].iov_base,
		       msg->msg_iov[i].iov_len);
		j += msg->msg_iov[i].iov_len;
	}

	/* Ancillary data. */
	if (msg_controlsize > 0) {
		struct cmsghdr *cmsg = msg->msg_control;
		memcpy(buf_msg->control, CMSG_DATA(cmsg), cmsg->cmsg_len);
		ITC_MSG_STORE_MEMBER32(cmsg->cmsg_level, buf, buf_msg,
		                       control_level);
		ITC_MSG_STORE_MEMBER32(cmsg->cmsg_type, buf, buf_msg,
		                       control_type);
	}

	/* Success! */
	retval = msg_size;

	/* Signal any waiting recvmsg() calls. */
	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_cond_broadcast(&buf->cond);
	PTHREADS_AMBIENT_END(buf);

done:
	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_unlock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

done_unlocked:
	ITC_AMBIENT_END(buf_cap);

	ITC_DEBUG("wl_itc_buffer_sendmsg(%p) finished = %li\n", buf, retval);

	return retval;
}

/* Drop-in replacement for recvmsg() with the same semantics. */
#ifdef CP2
WL_EXPORT ssize_t
wl_itc_buffer_recvmsg(struct wl_itc_buffer *buf, struct chericap *buf_cap,
                      struct msghdr *msg, int flags __unused)
#else /* if !CP2 */
WL_EXPORT ssize_t
wl_itc_buffer_recvmsg(struct wl_itc_buffer *buf, struct msghdr *msg,
                      int flags __unused)
#endif /* !CP2 */
{
	struct wl_itc_buffer_msg *buf_msg;
	size_t j, data_remaining;
	unsigned int i, buf_ref_count, buf_head, buf_slots;
	ssize_t retval = -1;
	void *buf_msg_data;
	size_t buf_msg_data_length;
	void *buf_msg_control;
	size_t buf_msg_control_length;
	int buf_msg_control_level;
	int buf_msg_control_type;

	ITC_DEBUG("wl_itc_buffer_recvmsg(%p)\n", buf);

	ITC_AMBIENT_BEGIN(buf_cap);

	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_lock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	/* Block on receiving data if there's none in the buffer. */
	ITC_LOAD_MEMBER32(buf_ref_count, buf, ref_count);

	while (buffer_is_empty(buf) && buf_ref_count > 0) {
		PTHREADS_AMBIENT_BEGIN(buf);
		pthread_cond_wait(&buf->cond, &buf->mutex);
		PTHREADS_AMBIENT_END(buf);

		ITC_LOAD_MEMBER32(buf_ref_count, buf, ref_count);
	}

	/* Is the buffer being destroyed? */
	if (buf_ref_count == 0) {
		retval = 0; /* orderly shutdown */
		goto done;
	}

	/* Read out the message. */
	ITC_LOAD_MEMBER32(buf_head, buf, head);
#ifdef CP2
	CHERI_CLW(buf_slots, (uintptr_t) &buf->slots - (uintptr_t) buf, 0, 1);
#else
	ITC_LOAD_MEMBER32(buf_slots, buf, slots);
#endif
	buf_msg = &buf->msgs[buf_head];
	buf_head = (buf_head + 1) % buf_slots;
	ITC_STORE_MEMBER32(buf_head, buf, head);

	/* Copy the data out. The cast is necessary because Linux and FreeBSD
	 * arbitrarily differ in signedness of msg_iovlen. */
	j = 0;
	ITC_MSG_LOAD_MEMBER64(data_remaining, buf, buf_msg, data_length);
	ITC_MSG_LOAD_MEMBER64(buf_msg_data, buf, buf_msg, data);

	for (i = 0; i < (unsigned int) msg->msg_iovlen &&
	            data_remaining > 0; i++) {
		size_t iov_len = MIN(msg->msg_iov[i].iov_len, data_remaining);
		memcpy(msg->msg_iov[i].iov_base,
		       (void *) ((uintptr_t) buf_msg_data + j), iov_len);
		j += iov_len;
		data_remaining -= iov_len;
	}

	/* Data truncated? */
	if (data_remaining > 0)
		msg->msg_flags |= MSG_TRUNC;

	/* Copy the control data out. */
	ITC_MSG_LOAD_MEMBER64(buf_msg_control_length, buf, buf_msg,
	                      control_length);
	ITC_MSG_LOAD_MEMBER64(buf_msg_control, buf, buf_msg, control);
	ITC_MSG_LOAD_MEMBER32(buf_msg_control_level, buf, buf_msg,
	                      control_level);
	ITC_MSG_LOAD_MEMBER32(buf_msg_control_type, buf, buf_msg, control_type);
	ITC_MSG_LOAD_MEMBER64(buf_msg_data_length, buf, buf_msg, data_length);

	if (buf_msg_control_length > 0) {
		struct cmsghdr *cmsg = msg->msg_control;

		if (cmsg != NULL && msg->msg_controllen > 0) {
			cmsg->cmsg_len = MIN(msg->msg_controllen,
			                     buf_msg_control_length);
			memcpy(CMSG_DATA(cmsg), buf_msg_control,
			       cmsg->cmsg_len);
			cmsg->cmsg_level = buf_msg_control_level;
			cmsg->cmsg_type = buf_msg_control_type;
		}

		/* Control truncated? */
		if (cmsg == NULL ||
		    buf_msg_control_length > msg->msg_controllen)
			msg->msg_flags |= MSG_CTRUNC;
	} else {
		/* Unset the caller's control buffer. */
		msg->msg_control = NULL;
		msg->msg_controllen = 0;
	}

	/* Free the message. */
	buffer_msg_destroy(buf, buf_msg);

	retval = (buf_msg_data_length - data_remaining);

done:
	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_unlock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	ITC_AMBIENT_END(buf_cap);

	ITC_DEBUG("wl_itc_buffer_recvmsg(%p) finished = %li\n", buf, retval);

	return retval;
}

/* Drop-in replacement for read() with the same semantics. */
#ifdef CP2
WL_EXPORT ssize_t
wl_itc_buffer_read(struct wl_itc_buffer *buf, struct chericap *buf_cap,
                   void *data_buf, size_t count)
#else /* if !CP2 */
WL_EXPORT ssize_t
wl_itc_buffer_read(struct wl_itc_buffer *buf, void *data_buf, size_t count)
#endif /* !CP2 */
{
	struct msghdr msg;
	struct iovec iov;

	iov.iov_base = data_buf;
	iov.iov_len = count;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

#ifdef CP2
	return wl_itc_buffer_recvmsg(buf, buf_cap, &msg, 0);
#else /* if !CP2 */
	return wl_itc_buffer_recvmsg(buf, &msg, 0);
#endif /* !CP2 */
}

/* Drop-in replacement for write() with the same semantics. */
#ifdef CP2
WL_EXPORT ssize_t
wl_itc_buffer_write(struct wl_itc_buffer *buf, struct chericap *buf_cap,
                    const void *data_buf, size_t count)
#else /* if !CP2 */
WL_EXPORT ssize_t
wl_itc_buffer_write(struct wl_itc_buffer *buf, const void *data_buf,
                    size_t count)
#endif /* !CP2 */
{
	struct msghdr msg;
	struct iovec iov;
	union {
		const void *const_data_buf;
		void *unconst_data_buf;
	} unconst_data_buf; /* sigh */

	unconst_data_buf.const_data_buf = data_buf;
	iov.iov_base = unconst_data_buf.unconst_data_buf;
	iov.iov_len = count;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

#ifdef CP2
	return wl_itc_buffer_sendmsg(buf, buf_cap, &msg,
	                             MSG_NOSIGNAL | MSG_DONTWAIT);
#else /* if !CP2 */
	return wl_itc_buffer_sendmsg(buf, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
#endif /* !CP2 */
}

/* TODO: Eliminate this! It's a horrible hack! How about creating a
 * wl_itc_buffer_recvmsg_non_blocking() variant? */
#ifdef CP2
WL_EXPORT unsigned int
wl_itc_buffer_is_empty(struct wl_itc_buffer *buf, struct chericap *buf_cap)
#else /* if !CP2 */
WL_EXPORT unsigned int
wl_itc_buffer_is_empty(struct wl_itc_buffer *buf)
#endif /* !CP2 */
{
	unsigned int retval, buf_head, buf_tail;

	ITC_AMBIENT_BEGIN(buf_cap);

	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_lock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	ITC_LOAD_MEMBER32(buf_head, buf, head);
	ITC_LOAD_MEMBER32(buf_tail, buf, tail);
	retval = (buf_head == buf_tail) ? 1 : 0;

	PTHREADS_AMBIENT_BEGIN(buf);
	pthread_mutex_unlock(&buf->mutex);
	PTHREADS_AMBIENT_END(buf);

	ITC_AMBIENT_END(buf_cap);

	return retval;
}
