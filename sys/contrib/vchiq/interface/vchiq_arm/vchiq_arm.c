/**
 * Copyright (c) 2014 Raspberry Pi (Trading) Ltd. All rights reserved.
 * Copyright (c) 2010-2012 Broadcom. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2, as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "vchiq_core.h"
#include "vchiq_ioctl.h"
#include "vchiq_arm.h"

#define DEVICE_NAME "vchiq"

/* Override the default prefix, which would be vchiq_arm (from the filename) */
#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX DEVICE_NAME "."

#define VCHIQ_MINOR 0

/* Some per-instance constants */
#define MAX_COMPLETIONS 128
#define MAX_SERVICES 64
#define MAX_ELEMENTS 8
#define MSG_QUEUE_SIZE 128

#define KEEPALIVE_VER 1
#define KEEPALIVE_VER_MIN KEEPALIVE_VER

/* Run time control of log level, based on KERN_XXX level. */
int vchiq_arm_log_level = VCHIQ_LOG_DEFAULT;
int vchiq_susp_log_level = VCHIQ_LOG_ERROR;

#define SUSPEND_TIMER_TIMEOUT_MS 100
#define SUSPEND_RETRY_TIMER_TIMEOUT_MS 1000

#define VC_SUSPEND_NUM_OFFSET 3 /* number of values before idle which are -ve */
static const char *const suspend_state_names[] = {
	"VC_SUSPEND_FORCE_CANCELED",
	"VC_SUSPEND_REJECTED",
	"VC_SUSPEND_FAILED",
	"VC_SUSPEND_IDLE",
	"VC_SUSPEND_REQUESTED",
	"VC_SUSPEND_IN_PROGRESS",
	"VC_SUSPEND_SUSPENDED"
};
#define VC_RESUME_NUM_OFFSET 1 /* number of values before idle which are -ve */
static const char *const resume_state_names[] = {
	"VC_RESUME_FAILED",
	"VC_RESUME_IDLE",
	"VC_RESUME_REQUESTED",
	"VC_RESUME_IN_PROGRESS",
	"VC_RESUME_RESUMED"
};
/* The number of times we allow force suspend to timeout before actually
** _forcing_ suspend.  This is to cater for SW which fails to release vchiq
** correctly - we don't want to prevent ARM suspend indefinitely in this case.
*/
#define FORCE_SUSPEND_FAIL_MAX 8

/* The time in ms allowed for videocore to go idle when force suspend has been
 * requested */
#define FORCE_SUSPEND_TIMEOUT_MS 200


static void suspend_timer_callback(unsigned long context);
#ifdef notyet
static int vchiq_proc_add_instance(VCHIQ_INSTANCE_T instance);
static void vchiq_proc_remove_instance(VCHIQ_INSTANCE_T instance);
#endif


typedef struct user_service_struct {
	VCHIQ_SERVICE_T *service;
	void *userdata;
	VCHIQ_INSTANCE_T instance;
	char is_vchi;
	char dequeue_pending;
	char close_pending;
	int message_available_pos;
	int msg_insert;
	int msg_remove;
	struct semaphore insert_event;
	struct semaphore remove_event;
	struct semaphore close_event;
	VCHIQ_HEADER_T * msg_queue[MSG_QUEUE_SIZE];
} USER_SERVICE_T;

struct bulk_waiter_node {
	struct bulk_waiter bulk_waiter;
	int pid;
	struct list_head list;
};

struct vchiq_instance_struct {
	VCHIQ_STATE_T *state;
	VCHIQ_COMPLETION_DATA_T completions[MAX_COMPLETIONS];
	int completion_insert;
	int completion_remove;
	struct semaphore insert_event;
	struct semaphore remove_event;
	struct mutex completion_mutex;

	int connected;
	int closing;
	int pid;
	int mark;
	int use_close_delivered;
	int trace;

	struct list_head bulk_waiter_list;
	struct mutex bulk_waiter_list_mutex;

#ifdef notyet
	VCHIQ_DEBUGFS_NODE_T proc_entry;
#endif
};

typedef struct dump_context_struct {
	char __user *buf;
	size_t actual;
	size_t space;
	loff_t offset;
} DUMP_CONTEXT_T;

static struct cdev *  vchiq_cdev;
VCHIQ_STATE_T g_state;
static DEFINE_SPINLOCK(msg_queue_spinlock);

static const char *const ioctl_names[] = {
	"CONNECT",
	"SHUTDOWN",
	"CREATE_SERVICE",
	"REMOVE_SERVICE",
	"QUEUE_MESSAGE",
	"QUEUE_BULK_TRANSMIT",
	"QUEUE_BULK_RECEIVE",
	"AWAIT_COMPLETION",
	"DEQUEUE_MESSAGE",
	"GET_CLIENT_ID",
	"GET_CONFIG",
	"CLOSE_SERVICE",
	"USE_SERVICE",
	"RELEASE_SERVICE",
	"SET_SERVICE_OPTION",
	"DUMP_PHYS_MEM",
	"LIB_VERSION",
	"CLOSE_DELIVERED"
};

vchiq_static_assert((sizeof(ioctl_names)/sizeof(ioctl_names[0])) ==
	(VCHIQ_IOC_MAX + 1));

static d_open_t		vchiq_open;
static d_close_t	vchiq_close;
static d_ioctl_t	vchiq_ioctl;

static struct cdevsw vchiq_cdevsw = {
	.d_version	= D_VERSION,
	.d_ioctl	= vchiq_ioctl,
	.d_open		= vchiq_open,
	.d_close	= vchiq_close,
	.d_name		= DEVICE_NAME,
};

#if 0
static void
dump_phys_mem(void *virt_addr, uint32_t num_bytes);
#endif

/****************************************************************************
*
*   add_completion
*
***************************************************************************/

static VCHIQ_STATUS_T
add_completion(VCHIQ_INSTANCE_T instance, VCHIQ_REASON_T reason,
	VCHIQ_HEADER_T *header, USER_SERVICE_T *user_service,
	void *bulk_userdata)
{
	VCHIQ_COMPLETION_DATA_T *completion;
	int insert;
	DEBUG_INITIALISE(g_state.local)

	insert = instance->completion_insert;
	while ((insert - instance->completion_remove) >= MAX_COMPLETIONS) {
		/* Out of space - wait for the client */
		DEBUG_TRACE(SERVICE_CALLBACK_LINE);
		vchiq_log_trace(vchiq_arm_log_level,
			"add_completion - completion queue full");
		DEBUG_COUNT(COMPLETION_QUEUE_FULL_COUNT);

		if (down_interruptible(&instance->remove_event) != 0) {
			vchiq_log_info(vchiq_arm_log_level,
				"service_callback interrupted");
			return VCHIQ_RETRY;
		}

		if (instance->closing) {
			vchiq_log_info(vchiq_arm_log_level,
				"service_callback closing");
			return VCHIQ_SUCCESS;
		}
		DEBUG_TRACE(SERVICE_CALLBACK_LINE);
	}

	completion = &instance->completions[insert & (MAX_COMPLETIONS - 1)];

	completion->header = header;
	completion->reason = reason;
	/* N.B. service_userdata is updated while processing AWAIT_COMPLETION */
	completion->service_userdata = user_service->service;
	completion->bulk_userdata = bulk_userdata;

	if (reason == VCHIQ_SERVICE_CLOSED) {
		/* Take an extra reference, to be held until
		   this CLOSED notification is delivered. */
		lock_service(user_service->service);
		if (instance->use_close_delivered)
			user_service->close_pending = 1;
	}

	/* A write barrier is needed here to ensure that the entire completion
		record is written out before the insert point. */
	wmb();

	if (reason == VCHIQ_MESSAGE_AVAILABLE)
		user_service->message_available_pos = insert;

	instance->completion_insert = ++insert;

	up(&instance->insert_event);

	return VCHIQ_SUCCESS;
}

/****************************************************************************
*
*   service_callback
*
***************************************************************************/

static VCHIQ_STATUS_T
service_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
	VCHIQ_SERVICE_HANDLE_T handle, void *bulk_userdata)
{
	/* How do we ensure the callback goes to the right client?
	** The service_user data points to a USER_SERVICE_T record containing
	** the original callback and the user state structure, which contains a
	** circular buffer for completion records.
	*/
	USER_SERVICE_T *user_service;
	VCHIQ_SERVICE_T *service;
	VCHIQ_INSTANCE_T instance;
	int skip_completion = 0;
	DEBUG_INITIALISE(g_state.local)

	DEBUG_TRACE(SERVICE_CALLBACK_LINE);

	service = handle_to_service(handle);
	BUG_ON(!service);
	user_service = (USER_SERVICE_T *)service->base.userdata;
	instance = user_service->instance;

	if (!instance || instance->closing)
		return VCHIQ_SUCCESS;

	vchiq_log_trace(vchiq_arm_log_level,
		"service_callback - service %lx(%d,%p), reason %d, header %lx, "
		"instance %lx, bulk_userdata %lx",
		(unsigned long)user_service,
		service->localport, user_service->userdata,
		reason, (unsigned long)header,
		(unsigned long)instance, (unsigned long)bulk_userdata);

	if (header && user_service->is_vchi) {
		spin_lock(&msg_queue_spinlock);
		while (user_service->msg_insert ==
			(user_service->msg_remove + MSG_QUEUE_SIZE)) {
			spin_unlock(&msg_queue_spinlock);
			DEBUG_TRACE(SERVICE_CALLBACK_LINE);
			DEBUG_COUNT(MSG_QUEUE_FULL_COUNT);
			vchiq_log_trace(vchiq_arm_log_level,
				"service_callback - msg queue full");
			/* If there is no MESSAGE_AVAILABLE in the completion
			** queue, add one
			*/
			if ((user_service->message_available_pos -
				instance->completion_remove) < 0) {
				VCHIQ_STATUS_T status;
				vchiq_log_info(vchiq_arm_log_level,
					"Inserting extra MESSAGE_AVAILABLE");
				DEBUG_TRACE(SERVICE_CALLBACK_LINE);
				status = add_completion(instance, reason,
					NULL, user_service, bulk_userdata);
				if (status != VCHIQ_SUCCESS) {
					DEBUG_TRACE(SERVICE_CALLBACK_LINE);
					return status;
				}
			}

			DEBUG_TRACE(SERVICE_CALLBACK_LINE);
			if (down_interruptible(&user_service->remove_event)
				!= 0) {
				vchiq_log_info(vchiq_arm_log_level,
					"service_callback interrupted");
				DEBUG_TRACE(SERVICE_CALLBACK_LINE);
				return VCHIQ_RETRY;
			} else if (instance->closing) {
				vchiq_log_info(vchiq_arm_log_level,
					"service_callback closing");
				DEBUG_TRACE(SERVICE_CALLBACK_LINE);
				return VCHIQ_ERROR;
			}
			DEBUG_TRACE(SERVICE_CALLBACK_LINE);
			spin_lock(&msg_queue_spinlock);
		}

		user_service->msg_queue[user_service->msg_insert &
			(MSG_QUEUE_SIZE - 1)] = header;
		user_service->msg_insert++;

		/* If there is a thread waiting in DEQUEUE_MESSAGE, or if
		** there is a MESSAGE_AVAILABLE in the completion queue then
		** bypass the completion queue.
		*/
		if (((user_service->message_available_pos -
			instance->completion_remove) >= 0) ||
			user_service->dequeue_pending) {
			user_service->dequeue_pending = 0;
			skip_completion = 1;
		}

		spin_unlock(&msg_queue_spinlock);

		up(&user_service->insert_event);

		header = NULL;
	}

	if (skip_completion) {
		DEBUG_TRACE(SERVICE_CALLBACK_LINE);
		return VCHIQ_SUCCESS;
	}

	DEBUG_TRACE(SERVICE_CALLBACK_LINE);

	return add_completion(instance, reason, header, user_service,
		bulk_userdata);
}

/****************************************************************************
*
*   user_service_free
*
***************************************************************************/
static void
user_service_free(void *userdata)
{
	USER_SERVICE_T *user_service = userdata;
	
	_sema_destroy(&user_service->insert_event);
	_sema_destroy(&user_service->remove_event);

	kfree(user_service);
}

/****************************************************************************
*
*   close_delivered
*
***************************************************************************/
static void close_delivered(USER_SERVICE_T *user_service)
{
	vchiq_log_info(vchiq_arm_log_level,
		"close_delivered(handle=%x)",
		user_service->service->handle);

	if (user_service->close_pending) {
		/* Allow the underlying service to be culled */
		unlock_service(user_service->service);

		/* Wake the user-thread blocked in close_ or remove_service */
		up(&user_service->close_event);
 
		user_service->close_pending = 0;
	}
}

/****************************************************************************
*
*   vchiq_ioctl
*
***************************************************************************/

static int
vchiq_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int fflag,
   struct thread *td)
{
	VCHIQ_INSTANCE_T instance;
	VCHIQ_STATUS_T status = VCHIQ_SUCCESS;
	VCHIQ_SERVICE_T *service = NULL;
	int ret = 0;
	int i, rc;
	DEBUG_INITIALISE(g_state.local)

	if ((ret = devfs_get_cdevpriv((void**)&instance))) {
		printf("vchiq_ioctl: devfs_get_cdevpriv failed: error %d\n", ret);
		return (ret);
	}

/* XXXBSD: HACK! */
#define _IOC_NR(x) ((x) & 0xff)
#define	_IOC_TYPE(x)	IOCGROUP(x)

	vchiq_log_trace(vchiq_arm_log_level,
		 "vchiq_ioctl - instance %x, cmd %s, arg %p",
		(unsigned int)instance,
		((_IOC_TYPE(cmd) == VCHIQ_IOC_MAGIC) &&
		(_IOC_NR(cmd) <= VCHIQ_IOC_MAX)) ?
		ioctl_names[_IOC_NR(cmd)] : "<invalid>", arg);

	switch (cmd) {
	case VCHIQ_IOC_SHUTDOWN:
		if (!instance->connected)
			break;

		/* Remove all services */
		i = 0;
		while ((service = next_service_by_instance(instance->state,
			instance, &i)) != NULL) {
			status = vchiq_remove_service(service->handle);
			unlock_service(service);
			if (status != VCHIQ_SUCCESS)
				break;
		}
		service = NULL;

		if (status == VCHIQ_SUCCESS) {
			/* Wake the completion thread and ask it to exit */
			instance->closing = 1;
			up(&instance->insert_event);
		}

		break;

	case VCHIQ_IOC_CONNECT:
		if (instance->connected) {
			ret = -EINVAL;
			break;
		}
		rc = lmutex_lock_interruptible(&instance->state->mutex);
		if (rc != 0) {
			vchiq_log_error(vchiq_arm_log_level,
				"vchiq: connect: could not lock mutex for "
				"state %d: %d",
				instance->state->id, rc);
			ret = -EINTR;
			break;
		}
		status = vchiq_connect_internal(instance->state, instance);
		lmutex_unlock(&instance->state->mutex);

		if (status == VCHIQ_SUCCESS)
			instance->connected = 1;
		else
			vchiq_log_error(vchiq_arm_log_level,
				"vchiq: could not connect: %d", status);
		break;

	case VCHIQ_IOC_CREATE_SERVICE: {
		VCHIQ_CREATE_SERVICE_T args;
		USER_SERVICE_T *user_service = NULL;
		void *userdata;
		int srvstate;

		memcpy(&args, (const void*)arg, sizeof(args));

		user_service = kmalloc(sizeof(USER_SERVICE_T), GFP_KERNEL);
		if (!user_service) {
			ret = -ENOMEM;
			break;
		}

		if (args.is_open) {
			if (!instance->connected) {
				ret = -ENOTCONN;
				kfree(user_service);
				break;
			}
			srvstate = VCHIQ_SRVSTATE_OPENING;
		} else {
			srvstate =
				 instance->connected ?
				 VCHIQ_SRVSTATE_LISTENING :
				 VCHIQ_SRVSTATE_HIDDEN;
		}

		userdata = args.params.userdata;
		args.params.callback = service_callback;
		args.params.userdata = user_service;
		service = vchiq_add_service_internal(
				instance->state,
				&args.params, srvstate,
				instance, user_service_free);

		if (service != NULL) {
			user_service->service = service;
			user_service->userdata = userdata;
			user_service->instance = instance;
			user_service->is_vchi = (args.is_vchi != 0);
			user_service->dequeue_pending = 0;
			user_service->close_pending = 0;
			user_service->message_available_pos =
				instance->completion_remove - 1;
			user_service->msg_insert = 0;
			user_service->msg_remove = 0;
			_sema_init(&user_service->insert_event, 0);
			_sema_init(&user_service->remove_event, 0);
			_sema_init(&user_service->close_event, 0);

			if (args.is_open) {
				status = vchiq_open_service_internal
					(service, instance->pid);
				if (status != VCHIQ_SUCCESS) {
					vchiq_remove_service(service->handle);
					service = NULL;
					ret = (status == VCHIQ_RETRY) ?
						-EINTR : -EIO;
					break;
				}
			}

#ifdef VCHIQ_IOCTL_DEBUG
			printf("%s: [CREATE SERVICE] handle = %08x\n", __func__, service->handle);
#endif
			memcpy((void *)
				&(((VCHIQ_CREATE_SERVICE_T*)
					arg)->handle),
				(const void *)&service->handle,
				sizeof(service->handle));

			service = NULL;
		} else {
			ret = -EEXIST;
			kfree(user_service);
		}
	} break;

	case VCHIQ_IOC_CLOSE_SERVICE: {
		VCHIQ_SERVICE_HANDLE_T handle;

		memcpy(&handle, (const void*)arg, sizeof(handle));

#ifdef VCHIQ_IOCTL_DEBUG
		printf("%s: [CLOSE SERVICE] handle = %08x\n", __func__, handle);
#endif

		service = find_service_for_instance(instance, handle);
		if (service != NULL) {
			USER_SERVICE_T *user_service =
				(USER_SERVICE_T *)service->base.userdata;
			/* close_pending is false on first entry, and when the
                           wait in vchiq_close_service has been interrupted. */
			if (!user_service->close_pending) {
				status = vchiq_close_service(service->handle);
				if (status != VCHIQ_SUCCESS)
					break;
			}

			/* close_pending is true once the underlying service
			   has been closed until the client library calls the
			   CLOSE_DELIVERED ioctl, signalling close_event. */
			if (user_service->close_pending &&
				down_interruptible(&user_service->close_event))
				status = VCHIQ_RETRY;
		}
		else
			ret = -EINVAL;
	} break;

	case VCHIQ_IOC_REMOVE_SERVICE: {
		VCHIQ_SERVICE_HANDLE_T handle;

		memcpy(&handle, (const void*)arg, sizeof(handle));

#ifdef VCHIQ_IOCTL_DEBUG
		printf("%s: [REMOVE SERVICE] handle = %08x\n", __func__, handle);
#endif

		service = find_service_for_instance(instance, handle);
		if (service != NULL) {
			USER_SERVICE_T *user_service =
				(USER_SERVICE_T *)service->base.userdata;
			/* close_pending is false on first entry, and when the
                           wait in vchiq_close_service has been interrupted. */
			if (!user_service->close_pending) {
				status = vchiq_remove_service(service->handle);
				if (status != VCHIQ_SUCCESS)
					break;
			}

			/* close_pending is true once the underlying service
			   has been closed until the client library calls the
			   CLOSE_DELIVERED ioctl, signalling close_event. */
			if (user_service->close_pending &&
				down_interruptible(&user_service->close_event))
				status = VCHIQ_RETRY;
		}
		else
			ret = -EINVAL;
	} break;

	case VCHIQ_IOC_USE_SERVICE:
	case VCHIQ_IOC_RELEASE_SERVICE:	{
		VCHIQ_SERVICE_HANDLE_T handle;

		memcpy(&handle, (const void*)arg, sizeof(handle));

#ifdef VCHIQ_IOCTL_DEBUG
		printf("%s: [%s SERVICE] handle = %08x\n", __func__,
		    cmd == VCHIQ_IOC_USE_SERVICE ? "USE" : "RELEASE", handle);
#endif

		service = find_service_for_instance(instance, handle);
		if (service != NULL) {
			status = (cmd == VCHIQ_IOC_USE_SERVICE)	?
				vchiq_use_service_internal(service) :
				vchiq_release_service_internal(service);
			if (status != VCHIQ_SUCCESS) {
				vchiq_log_error(vchiq_susp_log_level,
					"%s: cmd %s returned error %d for "
					"service %c%c%c%c:%8x",
					__func__,
					(cmd == VCHIQ_IOC_USE_SERVICE) ?
						"VCHIQ_IOC_USE_SERVICE" :
						"VCHIQ_IOC_RELEASE_SERVICE",
					status,
					VCHIQ_FOURCC_AS_4CHARS(
						service->base.fourcc),
					service->client_id);
				ret = -EINVAL;
			}
		} else
			ret = -EINVAL;
	} break;

	case VCHIQ_IOC_QUEUE_MESSAGE: {
		VCHIQ_QUEUE_MESSAGE_T args;
		memcpy(&args, (const void*)arg, sizeof(args));

#ifdef VCHIQ_IOCTL_DEBUG
		printf("%s: [QUEUE MESSAGE] handle = %08x\n", __func__, args.handle);
#endif

		service = find_service_for_instance(instance, args.handle);

		if ((service != NULL) && (args.count <= MAX_ELEMENTS)) {
			/* Copy elements into kernel space */
			VCHIQ_ELEMENT_T elements[MAX_ELEMENTS];
			if (copy_from_user(elements, args.elements,
				args.count * sizeof(VCHIQ_ELEMENT_T)) == 0)
				status = vchiq_queue_message
					(args.handle,
					elements, args.count);
			else
				ret = -EFAULT;
		} else {
			ret = -EINVAL;
		}
	} break;

	case VCHIQ_IOC_QUEUE_BULK_TRANSMIT:
	case VCHIQ_IOC_QUEUE_BULK_RECEIVE: {
		VCHIQ_QUEUE_BULK_TRANSFER_T args;
		struct bulk_waiter_node *waiter = NULL;
		VCHIQ_BULK_DIR_T dir =
			(cmd == VCHIQ_IOC_QUEUE_BULK_TRANSMIT) ?
			VCHIQ_BULK_TRANSMIT : VCHIQ_BULK_RECEIVE;

		memcpy(&args, (const void*)arg, sizeof(args));

		service = find_service_for_instance(instance, args.handle);
		if (!service) {
			ret = -EINVAL;
			break;
		}

		if (args.mode == VCHIQ_BULK_MODE_BLOCKING) {
			waiter = kzalloc(sizeof(struct bulk_waiter_node),
				GFP_KERNEL);
			if (!waiter) {
				ret = -ENOMEM;
				break;
			}
			args.userdata = &waiter->bulk_waiter;
		} else if (args.mode == VCHIQ_BULK_MODE_WAITING) {
			struct list_head *pos;
			lmutex_lock(&instance->bulk_waiter_list_mutex);
			list_for_each(pos, &instance->bulk_waiter_list) {
				if (list_entry(pos, struct bulk_waiter_node,
					list)->pid == current->p_pid) {
					waiter = list_entry(pos,
						struct bulk_waiter_node,
						list);
					list_del(pos);
					break;
				}

			}
			lmutex_unlock(&instance->bulk_waiter_list_mutex);
			if (!waiter) {
				vchiq_log_error(vchiq_arm_log_level,
					"no bulk_waiter found for pid %d",
					current->p_pid);
				ret = -ESRCH;
				break;
			}
			vchiq_log_info(vchiq_arm_log_level,
				"found bulk_waiter %x for pid %d",
				(unsigned int)waiter, current->p_pid);
			args.userdata = &waiter->bulk_waiter;
		}
		status = vchiq_bulk_transfer
			(args.handle,
			 VCHI_MEM_HANDLE_INVALID,
			 args.data, args.size,
			 args.userdata, args.mode,
			 dir);
		if (!waiter)
			break;
		if ((status != VCHIQ_RETRY) || fatal_signal_pending(current) ||
			!waiter->bulk_waiter.bulk) {
			if (waiter->bulk_waiter.bulk) {
				/* Cancel the signal when the transfer
				** completes. */
				spin_lock(&bulk_waiter_spinlock);
				waiter->bulk_waiter.bulk->userdata = NULL;
				spin_unlock(&bulk_waiter_spinlock);
			}
			_sema_destroy(&waiter->bulk_waiter.event);
			kfree(waiter);
		} else {
			const VCHIQ_BULK_MODE_T mode_waiting =
				VCHIQ_BULK_MODE_WAITING;
			waiter->pid = current->p_pid;
			lmutex_lock(&instance->bulk_waiter_list_mutex);
			list_add(&waiter->list, &instance->bulk_waiter_list);
			lmutex_unlock(&instance->bulk_waiter_list_mutex);
			vchiq_log_info(vchiq_arm_log_level,
				"saved bulk_waiter %x for pid %d",
				(unsigned int)waiter, current->p_pid);

			memcpy((void *)
				&(((VCHIQ_QUEUE_BULK_TRANSFER_T *)
					arg)->mode),
				(const void *)&mode_waiting,
				sizeof(mode_waiting));
		}
	} break;

	case VCHIQ_IOC_AWAIT_COMPLETION: {
		VCHIQ_AWAIT_COMPLETION_T args;
		int count = 0;

		DEBUG_TRACE(AWAIT_COMPLETION_LINE);
		if (!instance->connected) {
			ret = -ENOTCONN;
			break;
		}

                memcpy(&args, (const void*)arg, sizeof(args));

		lmutex_lock(&instance->completion_mutex);

		DEBUG_TRACE(AWAIT_COMPLETION_LINE);
		while ((instance->completion_remove ==
			instance->completion_insert)
			&& !instance->closing) {

			DEBUG_TRACE(AWAIT_COMPLETION_LINE);
			lmutex_unlock(&instance->completion_mutex);
			rc = down_interruptible(&instance->insert_event);
			lmutex_lock(&instance->completion_mutex);
			if (rc != 0) {
				DEBUG_TRACE(AWAIT_COMPLETION_LINE);
				vchiq_log_info(vchiq_arm_log_level,
					"AWAIT_COMPLETION interrupted");
				ret = -EINTR;
				break;
			}
		}
		DEBUG_TRACE(AWAIT_COMPLETION_LINE);

		if (ret == 0) {
			int msgbufcount = args.msgbufcount;
			int remove;

			remove = instance->completion_remove;

			for (count = 0; count < args.count; count++) {
				VCHIQ_COMPLETION_DATA_T *completion;
				VCHIQ_SERVICE_T *service1;
				USER_SERVICE_T *user_service;
				VCHIQ_HEADER_T *header;

				if (remove == instance->completion_insert)
					break;

				completion = &instance->completions[
					remove & (MAX_COMPLETIONS - 1)];


				/* A read memory barrier is needed to prevent
				** the prefetch of a stale completion record
				*/
				rmb();

				service1 = completion->service_userdata;
				user_service = service1->base.userdata;
				completion->service_userdata =
					user_service->userdata;

				header = completion->header;
				if (header) {
					void __user *msgbuf;
					int msglen;

					msglen = header->size +
						sizeof(VCHIQ_HEADER_T);
					/* This must be a VCHIQ-style service */
					if (args.msgbufsize < msglen) {
						vchiq_log_error(
							vchiq_arm_log_level,
							"header %x: msgbufsize"
							" %x < msglen %x",
							(unsigned int)header,
							args.msgbufsize,
							msglen);
						WARN(1, "invalid message "
							"size\n");
						if (count == 0)
							ret = -EMSGSIZE;
						break;
					}
					if (msgbufcount <= 0)
						/* Stall here for lack of a
						** buffer for the message. */
						break;
					/* Get the pointer from user space */
					msgbufcount--;
					if (copy_from_user(&msgbuf,
						(const void __user *)
						&args.msgbufs[msgbufcount],
						sizeof(msgbuf)) != 0) {
						if (count == 0)
							ret = -EFAULT;
						break;
					}

					/* Copy the message to user space */
					if (copy_to_user(msgbuf, header,
						msglen) != 0) {
						if (count == 0)
							ret = -EFAULT;
						break;
					}

					/* Now it has been copied, the message
					** can be released. */
					vchiq_release_message(service1->handle,
						header);

					/* The completion must point to the
					** msgbuf. */
					completion->header = msgbuf;
				}

				if ((completion->reason ==
					VCHIQ_SERVICE_CLOSED) &&
					!instance->use_close_delivered)
					unlock_service(service1);

				if (copy_to_user((void __user *)(
					(size_t)args.buf +
					count * sizeof(VCHIQ_COMPLETION_DATA_T)),
					completion,
					sizeof(VCHIQ_COMPLETION_DATA_T)) != 0) {
						if (ret == 0)
							ret = -EFAULT;
					break;
				}

				/* Ensure that the above copy has completed
				** before advancing the remove pointer. */
				mb();

				instance->completion_remove = ++remove;
			}

			if (msgbufcount != args.msgbufcount) {
				memcpy((void __user *)
					&((VCHIQ_AWAIT_COMPLETION_T *)arg)->
						msgbufcount,
					&msgbufcount,
					sizeof(msgbufcount));
			}

			 if (count != args.count)
			 {
				memcpy((void __user *)
					&((VCHIQ_AWAIT_COMPLETION_T *)arg)->count,
					&count, sizeof(count));
			}
		}

		if (count != 0)
			up(&instance->remove_event);

		if ((ret == 0) && instance->closing)
			ret = -ENOTCONN;
		/* 
		 * XXXBSD: ioctl return codes are not negative as in linux, so
		 * we can not indicate success with positive number of passed 
		 * messages
		 */
		if (ret > 0)
			ret = 0;

		lmutex_unlock(&instance->completion_mutex);
		DEBUG_TRACE(AWAIT_COMPLETION_LINE);
	} break;

	case VCHIQ_IOC_DEQUEUE_MESSAGE: {
		VCHIQ_DEQUEUE_MESSAGE_T args;
		USER_SERVICE_T *user_service;
		VCHIQ_HEADER_T *header;

		DEBUG_TRACE(DEQUEUE_MESSAGE_LINE);
		memcpy(&args, (const void*)arg, sizeof(args));
		service = find_service_for_instance(instance, args.handle);
		if (!service) {
			ret = -EINVAL;
			break;
		}
		user_service = (USER_SERVICE_T *)service->base.userdata;
		if (user_service->is_vchi == 0) {
			ret = -EINVAL;
			break;
		}

		spin_lock(&msg_queue_spinlock);
		if (user_service->msg_remove == user_service->msg_insert) {
			if (!args.blocking) {
				spin_unlock(&msg_queue_spinlock);
				DEBUG_TRACE(DEQUEUE_MESSAGE_LINE);
				ret = -EWOULDBLOCK;
				break;
			}
			user_service->dequeue_pending = 1;
			do {
				spin_unlock(&msg_queue_spinlock);
				DEBUG_TRACE(DEQUEUE_MESSAGE_LINE);
				if (down_interruptible(
					&user_service->insert_event) != 0) {
					vchiq_log_info(vchiq_arm_log_level,
						"DEQUEUE_MESSAGE interrupted");
					ret = -EINTR;
					break;
				}
				spin_lock(&msg_queue_spinlock);
			} while (user_service->msg_remove ==
				user_service->msg_insert);

			if (ret)
				break;
		}

		BUG_ON((int)(user_service->msg_insert -
			user_service->msg_remove) < 0);

		header = user_service->msg_queue[user_service->msg_remove &
			(MSG_QUEUE_SIZE - 1)];
		user_service->msg_remove++;
		spin_unlock(&msg_queue_spinlock);

		up(&user_service->remove_event);
		if (header == NULL)
			ret = -ENOTCONN;
		else if (header->size <= args.bufsize) {
			/* Copy to user space if msgbuf is not NULL */
			if ((args.buf == NULL) ||
				(copy_to_user((void __user *)args.buf,
				header->data,
				header->size) == 0)) {
				args.bufsize = header->size;
				memcpy((void *)arg, &args,
				    sizeof(args));
				vchiq_release_message(
					service->handle,
					header);
			} else
				ret = -EFAULT;
		} else {
			vchiq_log_error(vchiq_arm_log_level,
				"header %x: bufsize %x < size %x",
				(unsigned int)header, args.bufsize,
				header->size);
			WARN(1, "invalid size\n");
			ret = -EMSGSIZE;
		}
		DEBUG_TRACE(DEQUEUE_MESSAGE_LINE);
	} break;

	case VCHIQ_IOC_GET_CLIENT_ID: {
		VCHIQ_SERVICE_HANDLE_T handle;

		memcpy(&handle, (const void*)arg, sizeof(handle));

		ret = vchiq_get_client_id(handle);
	} break;

	case VCHIQ_IOC_GET_CONFIG: {
		VCHIQ_GET_CONFIG_T args;
		VCHIQ_CONFIG_T config;

		memcpy(&args, (const void*)arg, sizeof(args));
		if (args.config_size > sizeof(config)) {
			ret = -EINVAL;
			break;
		}
		status = vchiq_get_config(instance, args.config_size, &config);
		if (status == VCHIQ_SUCCESS) {
			if (copy_to_user((void __user *)args.pconfig,
				    &config, args.config_size) != 0) {
				ret = -EFAULT;
				break;
			}
		}
	} break;

	case VCHIQ_IOC_SET_SERVICE_OPTION: {
		VCHIQ_SET_SERVICE_OPTION_T args;

		memcpy(&args, (const void*)arg, sizeof(args));

		service = find_service_for_instance(instance, args.handle);
		if (!service) {
			ret = -EINVAL;
			break;
		}

		status = vchiq_set_service_option(
				args.handle, args.option, args.value);
	} break;

	case VCHIQ_IOC_DUMP_PHYS_MEM: {
		VCHIQ_DUMP_MEM_T  args;

		memcpy(&args, (const void*)arg, sizeof(args));
		printf("IMPLEMENT ME: %s:%d\n", __FILE__, __LINE__);
#if 0
		dump_phys_mem(args.virt_addr, args.num_bytes);
#endif
	} break;

	case VCHIQ_IOC_LIB_VERSION: {
		unsigned int lib_version = (unsigned int)arg;

		if (lib_version < VCHIQ_VERSION_MIN)
			ret = -EINVAL;
		else if (lib_version >= VCHIQ_VERSION_CLOSE_DELIVERED)
			instance->use_close_delivered = 1;
	} break;

	case VCHIQ_IOC_CLOSE_DELIVERED: {
		VCHIQ_SERVICE_HANDLE_T handle;
		memcpy(&handle, (const void*)arg, sizeof(handle));

		service = find_closed_service_for_instance(instance, handle);
		if (service != NULL) {
			USER_SERVICE_T *user_service =
				(USER_SERVICE_T *)service->base.userdata;
			close_delivered(user_service);
		}
		else
			ret = -EINVAL;
	} break;

	default:
		ret = -ENOTTY;
		break;
	}

	if (service)
		unlock_service(service);

	if (ret == 0) {
		if (status == VCHIQ_ERROR)
			ret = -EIO;
		else if (status == VCHIQ_RETRY)
			ret = -EINTR;
	}

	if ((status == VCHIQ_SUCCESS) && (ret < 0) && (ret != -EINTR) &&
		(ret != -EWOULDBLOCK))
		vchiq_log_info(vchiq_arm_log_level,
			"  ioctl instance %lx, cmd %s -> status %d, %d",
			(unsigned long)instance,
			(_IOC_NR(cmd) <= VCHIQ_IOC_MAX) ?
				ioctl_names[_IOC_NR(cmd)] :
				"<invalid>",
			status, ret);
	else
		vchiq_log_trace(vchiq_arm_log_level,
			"  ioctl instance %lx, cmd %s -> status %d, %d",
			(unsigned long)instance,
			(_IOC_NR(cmd) <= VCHIQ_IOC_MAX) ?
				ioctl_names[_IOC_NR(cmd)] :
				"<invalid>",
			status, ret);

	/* XXXBSD: report BSD-style error to userland */
	if (ret < 0)
		ret = -ret;

	return ret;
}

static void
instance_dtr(void *data)
{

	kfree(data);
}

/****************************************************************************
*
*   vchiq_open
*
***************************************************************************/

static int
vchiq_open(struct cdev *dev, int flags, int fmt __unused, struct thread *td)
{
	vchiq_log_info(vchiq_arm_log_level, "vchiq_open");
	/* XXXBSD: do we really need this check? */
	if (1) {
		VCHIQ_STATE_T *state = vchiq_get_state();
		VCHIQ_INSTANCE_T instance;

		if (!state) {
			vchiq_log_error(vchiq_arm_log_level,
				"vchiq has no connection to VideoCore");
			return -ENOTCONN;
		}

		instance = kmalloc(sizeof(*instance), GFP_KERNEL);
		if (!instance)
			return -ENOMEM;

		instance->state = state;
		/* XXXBSD: PID or thread ID? */
		instance->pid = td->td_proc->p_pid;

#ifdef notyet
		ret = vchiq_proc_add_instance(instance);
		if (ret != 0) {
			kfree(instance);
			return ret;
		}
#endif

		_sema_init(&instance->insert_event, 0);
		_sema_init(&instance->remove_event, 0);
		lmutex_init(&instance->completion_mutex);
		lmutex_init(&instance->bulk_waiter_list_mutex);
		INIT_LIST_HEAD(&instance->bulk_waiter_list);

		devfs_set_cdevpriv(instance, instance_dtr);
	} 
	else {
		vchiq_log_error(vchiq_arm_log_level,
			"Unknown minor device");
		return -ENXIO;
	}

	return 0;
}

/****************************************************************************
*
*   vchiq_release
*
***************************************************************************/

static int
vchiq_close(struct cdev *dev, int flags __unused, int fmt __unused,
                struct thread *td)
{
	int ret = 0;
	if (1) {
		VCHIQ_INSTANCE_T instance;
		VCHIQ_STATE_T *state = vchiq_get_state();
		VCHIQ_SERVICE_T *service;
		int i;

		if ((ret = devfs_get_cdevpriv((void**)&instance))) {
			printf("devfs_get_cdevpriv failed: error %d\n", ret);
			return (ret);
		}

		vchiq_log_info(vchiq_arm_log_level,
			"vchiq_release: instance=%lx",
			(unsigned long)instance);

		if (!state) {
			ret = -EPERM;
			goto out;
		}

		/* Ensure videocore is awake to allow termination. */
		vchiq_use_internal(instance->state, NULL,
				USE_TYPE_VCHIQ);

		lmutex_lock(&instance->completion_mutex);

		/* Wake the completion thread and ask it to exit */
		instance->closing = 1;
		up(&instance->insert_event);

		lmutex_unlock(&instance->completion_mutex);

		/* Wake the slot handler if the completion queue is full. */
		up(&instance->remove_event);

		/* Mark all services for termination... */
		i = 0;
		while ((service = next_service_by_instance(state, instance,
			&i)) !=	NULL) {
			USER_SERVICE_T *user_service = service->base.userdata;

			/* Wake the slot handler if the msg queue is full. */
			up(&user_service->remove_event);

			vchiq_terminate_service_internal(service);
			unlock_service(service);
		}

		/* ...and wait for them to die */
		i = 0;
		while ((service = next_service_by_instance(state, instance, &i))
			!= NULL) {
			USER_SERVICE_T *user_service = service->base.userdata;

			down(&service->remove_event);

			BUG_ON(service->srvstate != VCHIQ_SRVSTATE_FREE);

			spin_lock(&msg_queue_spinlock);

			while (user_service->msg_remove !=
				user_service->msg_insert) {
				VCHIQ_HEADER_T *header = user_service->
					msg_queue[user_service->msg_remove &
						(MSG_QUEUE_SIZE - 1)];
				user_service->msg_remove++;
				spin_unlock(&msg_queue_spinlock);

				if (header)
					vchiq_release_message(
						service->handle,
						header);
				spin_lock(&msg_queue_spinlock);
			}

			spin_unlock(&msg_queue_spinlock);

			unlock_service(service);
		}

		/* Release any closed services */
		while (instance->completion_remove !=
			instance->completion_insert) {
			VCHIQ_COMPLETION_DATA_T *completion;
			VCHIQ_SERVICE_T *service1;
			completion = &instance->completions[
				instance->completion_remove &
				(MAX_COMPLETIONS - 1)];
			service1 = completion->service_userdata;
			if (completion->reason == VCHIQ_SERVICE_CLOSED)
			{
				USER_SERVICE_T *user_service =
					service->base.userdata;

				/* Wake any blocked user-thread */
				if (instance->use_close_delivered)
					up(&user_service->close_event);
				unlock_service(service1);
			}
			instance->completion_remove++;
		}

		/* Release the PEER service count. */
		vchiq_release_internal(instance->state, NULL);

		{
			struct list_head *pos, *next;
			list_for_each_safe(pos, next,
				&instance->bulk_waiter_list) {
				struct bulk_waiter_node *waiter;
				waiter = list_entry(pos,
					struct bulk_waiter_node,
					list);
				list_del(pos);
				vchiq_log_info(vchiq_arm_log_level,
					"bulk_waiter - cleaned up %x "
					"for pid %d",
					(unsigned int)waiter, waiter->pid);
		                _sema_destroy(&waiter->bulk_waiter.event);
				kfree(waiter);
			}
		}

	}
	else {
		vchiq_log_error(vchiq_arm_log_level,
			"Unknown minor device");
		ret = -ENXIO;
	}

out:
	return ret;
}

/****************************************************************************
*
*   vchiq_dump
*
***************************************************************************/

void
vchiq_dump(void *dump_context, const char *str, int len)
{
	DUMP_CONTEXT_T *context = (DUMP_CONTEXT_T *)dump_context;

	if (context->actual < context->space) {
		int copy_bytes;
		if (context->offset > 0) {
			int skip_bytes = min(len, (int)context->offset);
			str += skip_bytes;
			len -= skip_bytes;
			context->offset -= skip_bytes;
			if (context->offset > 0)
				return;
		}
		copy_bytes = min(len, (int)(context->space - context->actual));
		if (copy_bytes == 0)
			return;
		memcpy(context->buf + context->actual, str, copy_bytes);
		context->actual += copy_bytes;
		len -= copy_bytes;

		/* If tne terminating NUL is included in the length, then it
		** marks the end of a line and should be replaced with a
		** carriage return. */
		if ((len == 0) && (str[copy_bytes - 1] == '\0')) {
			char cr = '\n';
			memcpy(context->buf + context->actual - 1, &cr, 1);
		}
	}
}

/****************************************************************************
*
*   vchiq_dump_platform_instance_state
*
***************************************************************************/

void
vchiq_dump_platform_instances(void *dump_context)
{
	VCHIQ_STATE_T *state = vchiq_get_state();
	char buf[80];
	int len;
	int i;

	/* There is no list of instances, so instead scan all services,
		marking those that have been dumped. */

	for (i = 0; i < state->unused_service; i++) {
		VCHIQ_SERVICE_T *service = state->services[i];
		VCHIQ_INSTANCE_T instance;

		if (service && (service->base.callback == service_callback)) {
			instance = service->instance;
			if (instance)
				instance->mark = 0;
		}
	}

	for (i = 0; i < state->unused_service; i++) {
		VCHIQ_SERVICE_T *service = state->services[i];
		VCHIQ_INSTANCE_T instance;

		if (service && (service->base.callback == service_callback)) {
			instance = service->instance;
			if (instance && !instance->mark) {
				len = snprintf(buf, sizeof(buf),
					"Instance %x: pid %d,%s completions "
						"%d/%d",
					(unsigned int)instance, instance->pid,
					instance->connected ? " connected, " :
						"",
					instance->completion_insert -
						instance->completion_remove,
					MAX_COMPLETIONS);

				vchiq_dump(dump_context, buf, len + 1);

				instance->mark = 1;
			}
		}
	}
}

/****************************************************************************
*
*   vchiq_dump_platform_service_state
*
***************************************************************************/

void
vchiq_dump_platform_service_state(void *dump_context, VCHIQ_SERVICE_T *service)
{
	USER_SERVICE_T *user_service = (USER_SERVICE_T *)service->base.userdata;
	char buf[80];
	int len;

	len = snprintf(buf, sizeof(buf), "  instance %x",
		(unsigned int)service->instance);

	if ((service->base.callback == service_callback) &&
		user_service->is_vchi) {
		len += snprintf(buf + len, sizeof(buf) - len,
			", %d/%d messages",
			user_service->msg_insert - user_service->msg_remove,
			MSG_QUEUE_SIZE);

		if (user_service->dequeue_pending)
			len += snprintf(buf + len, sizeof(buf) - len,
				" (dequeue pending)");
	}

	vchiq_dump(dump_context, buf, len + 1);
}

#ifdef notyet
/****************************************************************************
*
*   dump_user_mem
*
***************************************************************************/

static void
dump_phys_mem(void *virt_addr, uint32_t num_bytes)
{
	int            rc;
	uint8_t       *end_virt_addr = virt_addr + num_bytes;
	int            num_pages;
	int            offset;
	int            end_offset;
	int            page_idx;
	int            prev_idx;
	struct page   *page;
	struct page  **pages;
	uint8_t       *kmapped_virt_ptr;

	/* Align virtAddr and endVirtAddr to 16 byte boundaries. */

	virt_addr = (void *)((unsigned long)virt_addr & ~0x0fuL);
	end_virt_addr = (void *)(((unsigned long)end_virt_addr + 15uL) &
		~0x0fuL);

	offset = (int)(long)virt_addr & (PAGE_SIZE - 1);
	end_offset = (int)(long)end_virt_addr & (PAGE_SIZE - 1);

	num_pages = (offset + num_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

	pages = kmalloc(sizeof(struct page *) * num_pages, GFP_KERNEL);
	if (pages == NULL) {
		vchiq_log_error(vchiq_arm_log_level,
			"Unable to allocation memory for %d pages\n",
			num_pages);
		return;
	}

	down_read(&current->mm->mmap_sem);
	rc = get_user_pages(current,      /* task */
		current->mm,              /* mm */
		(unsigned long)virt_addr, /* start */
		num_pages,                /* len */
		0,                        /* write */
		0,                        /* force */
		pages,                    /* pages (array of page pointers) */
		NULL);                    /* vmas */
	up_read(&current->mm->mmap_sem);

	prev_idx = -1;
	page = NULL;

	while (offset < end_offset) {

		int page_offset = offset % PAGE_SIZE;
		page_idx = offset / PAGE_SIZE;

		if (page_idx != prev_idx) {

			if (page != NULL)
				kunmap(page);
			page = pages[page_idx];
			kmapped_virt_ptr = kmap(page);

			prev_idx = page_idx;
		}

		if (vchiq_arm_log_level >= VCHIQ_LOG_TRACE)
			vchiq_log_dump_mem("ph",
				(uint32_t)(unsigned long)&kmapped_virt_ptr[
					page_offset],
				&kmapped_virt_ptr[page_offset], 16);

		offset += 16;
	}
	if (page != NULL)
		kunmap(page);

	for (page_idx = 0; page_idx < num_pages; page_idx++)
		page_cache_release(pages[page_idx]);

	kfree(pages);
}

/****************************************************************************
*
*   vchiq_read
*
***************************************************************************/

static ssize_t
vchiq_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	DUMP_CONTEXT_T context;
	context.buf = buf;
	context.actual = 0;
	context.space = count;
	context.offset = *ppos;

	vchiq_dump_state(&context, &g_state);

	*ppos += context.actual;

	return context.actual;
}
#endif

VCHIQ_STATE_T *
vchiq_get_state(void)
{

	if (g_state.remote == NULL)
		printk(KERN_ERR "%s: g_state.remote == NULL\n", __func__);
	else if (g_state.remote->initialised != 1)
		printk(KERN_NOTICE "%s: g_state.remote->initialised != 1 (%d)\n",
			__func__, g_state.remote->initialised);

	return ((g_state.remote != NULL) &&
		(g_state.remote->initialised == 1)) ? &g_state : NULL;
}

/*
 * Autosuspend related functionality
 */

int
vchiq_videocore_wanted(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	if (!arm_state)
		/* autosuspend not supported - always return wanted */
		return 1;
	else if (arm_state->blocked_count)
		return 1;
	else if (!arm_state->videocore_use_count)
		/* usage count zero - check for override unless we're forcing */
		if (arm_state->resume_blocked)
			return 0;
		else
			return vchiq_platform_videocore_wanted(state);
	else
		/* non-zero usage count - videocore still required */
		return 1;
}

static VCHIQ_STATUS_T
vchiq_keepalive_vchiq_callback(VCHIQ_REASON_T reason,
	VCHIQ_HEADER_T *header,
	VCHIQ_SERVICE_HANDLE_T service_user,
	void *bulk_user)
{
	vchiq_log_error(vchiq_susp_log_level,
		"%s callback reason %d", __func__, reason);
	return 0;
}

static int
vchiq_keepalive_thread_func(void *v)
{
	VCHIQ_STATE_T *state = (VCHIQ_STATE_T *) v;
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);

	VCHIQ_STATUS_T status;
	VCHIQ_INSTANCE_T instance;
	VCHIQ_SERVICE_HANDLE_T ka_handle;

	VCHIQ_SERVICE_PARAMS_T params = {
		.fourcc      = VCHIQ_MAKE_FOURCC('K', 'E', 'E', 'P'),
		.callback    = vchiq_keepalive_vchiq_callback,
		.version     = KEEPALIVE_VER,
		.version_min = KEEPALIVE_VER_MIN
	};

	status = vchiq_initialise(&instance);
	if (status != VCHIQ_SUCCESS) {
		vchiq_log_error(vchiq_susp_log_level,
			"%s vchiq_initialise failed %d", __func__, status);
		goto exit;
	}

	status = vchiq_connect(instance);
	if (status != VCHIQ_SUCCESS) {
		vchiq_log_error(vchiq_susp_log_level,
			"%s vchiq_connect failed %d", __func__, status);
		goto shutdown;
	}

	status = vchiq_add_service(instance, &params, &ka_handle);
	if (status != VCHIQ_SUCCESS) {
		vchiq_log_error(vchiq_susp_log_level,
			"%s vchiq_open_service failed %d", __func__, status);
		goto shutdown;
	}

	while (1) {
		long rc = 0, uc = 0;
		if (wait_for_completion_interruptible(&arm_state->ka_evt)
				!= 0) {
			vchiq_log_error(vchiq_susp_log_level,
				"%s interrupted", __func__);
			flush_signals(current);
			continue;
		}

		/* read and clear counters.  Do release_count then use_count to
		 * prevent getting more releases than uses */
		rc = atomic_xchg(&arm_state->ka_release_count, 0);
		uc = atomic_xchg(&arm_state->ka_use_count, 0);

		/* Call use/release service the requisite number of times.
		 * Process use before release so use counts don't go negative */
		while (uc--) {
			atomic_inc(&arm_state->ka_use_ack_count);
			status = vchiq_use_service(ka_handle);
			if (status != VCHIQ_SUCCESS) {
				vchiq_log_error(vchiq_susp_log_level,
					"%s vchiq_use_service error %d",
					__func__, status);
			}
		}
		while (rc--) {
			status = vchiq_release_service(ka_handle);
			if (status != VCHIQ_SUCCESS) {
				vchiq_log_error(vchiq_susp_log_level,
					"%s vchiq_release_service error %d",
					__func__, status);
			}
		}
	}

shutdown:
	vchiq_shutdown(instance);
exit:
	return 0;
}

VCHIQ_STATUS_T
vchiq_arm_init_state(VCHIQ_STATE_T *state, VCHIQ_ARM_STATE_T *arm_state)
{
	VCHIQ_STATUS_T status = VCHIQ_SUCCESS;

	if (arm_state) {
		rwlock_init(&arm_state->susp_res_lock);

		init_completion(&arm_state->ka_evt);
		atomic_set(&arm_state->ka_use_count, 0);
		atomic_set(&arm_state->ka_use_ack_count, 0);
		atomic_set(&arm_state->ka_release_count, 0);

		init_completion(&arm_state->vc_suspend_complete);

		init_completion(&arm_state->vc_resume_complete);
		/* Initialise to 'done' state.  We only want to block on resume
		 * completion while videocore is suspended. */
		set_resume_state(arm_state, VC_RESUME_RESUMED);

		init_completion(&arm_state->resume_blocker);
		/* Initialise to 'done' state.  We only want to block on this
		 * completion while resume is blocked */
		complete_all(&arm_state->resume_blocker);

		init_completion(&arm_state->blocked_blocker);
		/* Initialise to 'done' state.  We only want to block on this
		 * completion while things are waiting on the resume blocker */
		complete_all(&arm_state->blocked_blocker);

		arm_state->suspend_timer_timeout = SUSPEND_TIMER_TIMEOUT_MS;
		arm_state->suspend_timer_running = 0;
		vchiq_init_timer(&arm_state->suspend_timer);
		arm_state->suspend_timer.data = (unsigned long)(state);
		arm_state->suspend_timer.function = suspend_timer_callback;

		arm_state->first_connect = 0;

	}
	return status;
}

/*
** Functions to modify the state variables;
**	set_suspend_state
**	set_resume_state
**
** There are more state variables than we might like, so ensure they remain in
** step.  Suspend and resume state are maintained separately, since most of
** these state machines can operate independently.  However, there are a few
** states where state transitions in one state machine cause a reset to the
** other state machine.  In addition, there are some completion events which
** need to occur on state machine reset and end-state(s), so these are also
** dealt with in these functions.
**
** In all states we set the state variable according to the input, but in some
** cases we perform additional steps outlined below;
**
** VC_SUSPEND_IDLE - Initialise the suspend completion at the same time.
**			The suspend completion is completed after any suspend
**			attempt.  When we reset the state machine we also reset
**			the completion.  This reset occurs when videocore is
**			resumed, and also if we initiate suspend after a suspend
**			failure.
**
** VC_SUSPEND_IN_PROGRESS - This state is considered the point of no return for
**			suspend - ie from this point on we must try to suspend
**			before resuming can occur.  We therefore also reset the
**			resume state machine to VC_RESUME_IDLE in this state.
**
** VC_SUSPEND_SUSPENDED - Suspend has completed successfully. Also call
**			complete_all on the suspend completion to notify
**			anything waiting for suspend to happen.
**
** VC_SUSPEND_REJECTED - Videocore rejected suspend. Videocore will also
**			initiate resume, so no need to alter resume state.
**			We call complete_all on the suspend completion to notify
**			of suspend rejection.
**
** VC_SUSPEND_FAILED - We failed to initiate videocore suspend.  We notify the
**			suspend completion and reset the resume state machine.
**
** VC_RESUME_IDLE - Initialise the resume completion at the same time.  The
**			resume completion is in its 'done' state whenever
**			videcore is running.  Therfore, the VC_RESUME_IDLE state
**			implies that videocore is suspended.
**			Hence, any thread which needs to wait until videocore is
**			running can wait on this completion - it will only block
**			if videocore is suspended.
**
** VC_RESUME_RESUMED - Resume has completed successfully.  Videocore is running.
**			Call complete_all on the resume completion to unblock
**			any threads waiting for resume.	 Also reset the suspend
**			state machine to it's idle state.
**
** VC_RESUME_FAILED - Currently unused - no mechanism to fail resume exists.
*/

void
set_suspend_state(VCHIQ_ARM_STATE_T *arm_state,
	enum vc_suspend_status new_state)
{
	/* set the state in all cases */
	arm_state->vc_suspend_state = new_state;

	/* state specific additional actions */
	switch (new_state) {
	case VC_SUSPEND_FORCE_CANCELED:
		complete_all(&arm_state->vc_suspend_complete);
		break;
	case VC_SUSPEND_REJECTED:
		complete_all(&arm_state->vc_suspend_complete);
		break;
	case VC_SUSPEND_FAILED:
		complete_all(&arm_state->vc_suspend_complete);
		arm_state->vc_resume_state = VC_RESUME_RESUMED;
		complete_all(&arm_state->vc_resume_complete);
		break;
	case VC_SUSPEND_IDLE:
		/* TODO: reinit_completion */
		INIT_COMPLETION(arm_state->vc_suspend_complete);
		break;
	case VC_SUSPEND_REQUESTED:
		break;
	case VC_SUSPEND_IN_PROGRESS:
		set_resume_state(arm_state, VC_RESUME_IDLE);
		break;
	case VC_SUSPEND_SUSPENDED:
		complete_all(&arm_state->vc_suspend_complete);
		break;
	default:
		BUG();
		break;
	}
}

void
set_resume_state(VCHIQ_ARM_STATE_T *arm_state,
	enum vc_resume_status new_state)
{
	/* set the state in all cases */
	arm_state->vc_resume_state = new_state;

	/* state specific additional actions */
	switch (new_state) {
	case VC_RESUME_FAILED:
		break;
	case VC_RESUME_IDLE:
		/* TODO: reinit_completion */
		INIT_COMPLETION(arm_state->vc_resume_complete);
		break;
	case VC_RESUME_REQUESTED:
		break;
	case VC_RESUME_IN_PROGRESS:
		break;
	case VC_RESUME_RESUMED:
		complete_all(&arm_state->vc_resume_complete);
		set_suspend_state(arm_state, VC_SUSPEND_IDLE);
		break;
	default:
		BUG();
		break;
	}
}


/* should be called with the write lock held */
inline void
start_suspend_timer(VCHIQ_ARM_STATE_T *arm_state)
{
	vchiq_del_timer(&arm_state->suspend_timer);
	arm_state->suspend_timer.expires = jiffies +
		msecs_to_jiffies(arm_state->
			suspend_timer_timeout);
	vchiq_add_timer(&arm_state->suspend_timer);
	arm_state->suspend_timer_running = 1;
}

/* should be called with the write lock held */
static inline void
stop_suspend_timer(VCHIQ_ARM_STATE_T *arm_state)
{
	if (arm_state->suspend_timer_running) {
		vchiq_del_timer(&arm_state->suspend_timer);
		arm_state->suspend_timer_running = 0;
	}
}

static inline int
need_resume(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	return (arm_state->vc_suspend_state > VC_SUSPEND_IDLE) &&
			(arm_state->vc_resume_state < VC_RESUME_REQUESTED) &&
			vchiq_videocore_wanted(state);
}

static int
block_resume(VCHIQ_ARM_STATE_T *arm_state)
{
	int status = VCHIQ_SUCCESS;
	const unsigned long timeout_val =
				msecs_to_jiffies(FORCE_SUSPEND_TIMEOUT_MS);
	int resume_count = 0;

	/* Allow any threads which were blocked by the last force suspend to
	 * complete if they haven't already.  Only give this one shot; if
	 * blocked_count is incremented after blocked_blocker is completed
	 * (which only happens when blocked_count hits 0) then those threads
	 * will have to wait until next time around */
	if (arm_state->blocked_count) {
		/* TODO: reinit_completion */
		INIT_COMPLETION(arm_state->blocked_blocker);
		write_unlock_bh(&arm_state->susp_res_lock);
		vchiq_log_info(vchiq_susp_log_level, "%s wait for previously "
			"blocked clients", __func__);
		if (wait_for_completion_interruptible_timeout(
				&arm_state->blocked_blocker, timeout_val)
					<= 0) {
			vchiq_log_error(vchiq_susp_log_level, "%s wait for "
				"previously blocked clients failed" , __func__);
			status = VCHIQ_ERROR;
			write_lock_bh(&arm_state->susp_res_lock);
			goto out;
		}
		vchiq_log_info(vchiq_susp_log_level, "%s previously blocked "
			"clients resumed", __func__);
		write_lock_bh(&arm_state->susp_res_lock);
	}

	/* We need to wait for resume to complete if it's in process */
	while (arm_state->vc_resume_state != VC_RESUME_RESUMED &&
			arm_state->vc_resume_state > VC_RESUME_IDLE) {
		if (resume_count > 1) {
			status = VCHIQ_ERROR;
			vchiq_log_error(vchiq_susp_log_level, "%s waited too "
				"many times for resume" , __func__);
			goto out;
		}
		write_unlock_bh(&arm_state->susp_res_lock);
		vchiq_log_info(vchiq_susp_log_level, "%s wait for resume",
			__func__);
		if (wait_for_completion_interruptible_timeout(
				&arm_state->vc_resume_complete, timeout_val)
					<= 0) {
			vchiq_log_error(vchiq_susp_log_level, "%s wait for "
				"resume failed (%s)", __func__,
				resume_state_names[arm_state->vc_resume_state +
							VC_RESUME_NUM_OFFSET]);
			status = VCHIQ_ERROR;
			write_lock_bh(&arm_state->susp_res_lock);
			goto out;
		}
		vchiq_log_info(vchiq_susp_log_level, "%s resumed", __func__);
		write_lock_bh(&arm_state->susp_res_lock);
		resume_count++;
	}
	/* TODO: reinit_completion */
	INIT_COMPLETION(arm_state->resume_blocker);
	arm_state->resume_blocked = 1;

out:
	return status;
}

static inline void
unblock_resume(VCHIQ_ARM_STATE_T *arm_state)
{
	complete_all(&arm_state->resume_blocker);
	arm_state->resume_blocked = 0;
}

/* Initiate suspend via slot handler. Should be called with the write lock
 * held */
VCHIQ_STATUS_T
vchiq_arm_vcsuspend(VCHIQ_STATE_T *state)
{
	VCHIQ_STATUS_T status = VCHIQ_ERROR;
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);
	status = VCHIQ_SUCCESS;


	switch (arm_state->vc_suspend_state) {
	case VC_SUSPEND_REQUESTED:
		vchiq_log_info(vchiq_susp_log_level, "%s: suspend already "
			"requested", __func__);
		break;
	case VC_SUSPEND_IN_PROGRESS:
		vchiq_log_info(vchiq_susp_log_level, "%s: suspend already in "
			"progress", __func__);
		break;

	default:
		/* We don't expect to be in other states, so log but continue
		 * anyway */
		vchiq_log_error(vchiq_susp_log_level,
			"%s unexpected suspend state %s", __func__,
			suspend_state_names[arm_state->vc_suspend_state +
						VC_SUSPEND_NUM_OFFSET]);
		/* fall through */
	case VC_SUSPEND_REJECTED:
	case VC_SUSPEND_FAILED:
		/* Ensure any idle state actions have been run */
		set_suspend_state(arm_state, VC_SUSPEND_IDLE);
		/* fall through */
	case VC_SUSPEND_IDLE:
		vchiq_log_info(vchiq_susp_log_level,
			"%s: suspending", __func__);
		set_suspend_state(arm_state, VC_SUSPEND_REQUESTED);
		/* kick the slot handler thread to initiate suspend */
		request_poll(state, NULL, 0);
		break;
	}

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit %d", __func__, status);
	return status;
}

void
vchiq_platform_check_suspend(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	int susp = 0;

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	write_lock_bh(&arm_state->susp_res_lock);
	if (arm_state->vc_suspend_state == VC_SUSPEND_REQUESTED &&
			arm_state->vc_resume_state == VC_RESUME_RESUMED) {
		set_suspend_state(arm_state, VC_SUSPEND_IN_PROGRESS);
		susp = 1;
	}
	write_unlock_bh(&arm_state->susp_res_lock);

	if (susp)
		vchiq_platform_suspend(state);

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit", __func__);
	return;
}


static void
output_timeout_error(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	char service_err[50] = "";
	int vc_use_count = arm_state->videocore_use_count;
	int active_services = state->unused_service;
	int i;

	if (!arm_state->videocore_use_count) {
		snprintf(service_err, 50, " Videocore usecount is 0");
		goto output_msg;
	}
	for (i = 0; i < active_services; i++) {
		VCHIQ_SERVICE_T *service_ptr = state->services[i];
		if (service_ptr && service_ptr->service_use_count &&
			(service_ptr->srvstate != VCHIQ_SRVSTATE_FREE)) {
			snprintf(service_err, 50, " %c%c%c%c(%8x) service has "
				"use count %d%s", VCHIQ_FOURCC_AS_4CHARS(
					service_ptr->base.fourcc),
				 service_ptr->client_id,
				 service_ptr->service_use_count,
				 service_ptr->service_use_count ==
					 vc_use_count ? "" : " (+ more)");
			break;
		}
	}

output_msg:
	vchiq_log_error(vchiq_susp_log_level,
		"timed out waiting for vc suspend (%d).%s",
		 arm_state->autosuspend_override, service_err);

}

/* Try to get videocore into suspended state, regardless of autosuspend state.
** We don't actually force suspend, since videocore may get into a bad state
** if we force suspend at a bad time.  Instead, we wait for autosuspend to
** determine a good point to suspend.  If this doesn't happen within 100ms we
** report failure.
**
** Returns VCHIQ_SUCCESS if videocore suspended successfully, VCHIQ_RETRY if
** videocore failed to suspend in time or VCHIQ_ERROR if interrupted.
*/
VCHIQ_STATUS_T
vchiq_arm_force_suspend(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	VCHIQ_STATUS_T status = VCHIQ_ERROR;
	long rc = 0;
	int repeat = -1;

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	write_lock_bh(&arm_state->susp_res_lock);

	status = block_resume(arm_state);
	if (status != VCHIQ_SUCCESS)
		goto unlock;
	if (arm_state->vc_suspend_state == VC_SUSPEND_SUSPENDED) {
		/* Already suspended - just block resume and exit */
		vchiq_log_info(vchiq_susp_log_level, "%s already suspended",
			__func__);
		status = VCHIQ_SUCCESS;
		goto unlock;
	} else if (arm_state->vc_suspend_state <= VC_SUSPEND_IDLE) {
		/* initiate suspend immediately in the case that we're waiting
		 * for the timeout */
		stop_suspend_timer(arm_state);
		if (!vchiq_videocore_wanted(state)) {
			vchiq_log_info(vchiq_susp_log_level, "%s videocore "
				"idle, initiating suspend", __func__);
			status = vchiq_arm_vcsuspend(state);
		} else if (arm_state->autosuspend_override <
						FORCE_SUSPEND_FAIL_MAX) {
			vchiq_log_info(vchiq_susp_log_level, "%s letting "
				"videocore go idle", __func__);
			status = VCHIQ_SUCCESS;
		} else {
			vchiq_log_warning(vchiq_susp_log_level, "%s failed too "
				"many times - attempting suspend", __func__);
			status = vchiq_arm_vcsuspend(state);
		}
	} else {
		vchiq_log_info(vchiq_susp_log_level, "%s videocore suspend "
			"in progress - wait for completion", __func__);
		status = VCHIQ_SUCCESS;
	}

	/* Wait for suspend to happen due to system idle (not forced..) */
	if (status != VCHIQ_SUCCESS)
		goto unblock_resume;

	do {
		write_unlock_bh(&arm_state->susp_res_lock);

		rc = wait_for_completion_interruptible_timeout(
				&arm_state->vc_suspend_complete,
				msecs_to_jiffies(FORCE_SUSPEND_TIMEOUT_MS));

		write_lock_bh(&arm_state->susp_res_lock);
		if (rc < 0) {
			vchiq_log_warning(vchiq_susp_log_level, "%s "
				"interrupted waiting for suspend", __func__);
			status = VCHIQ_ERROR;
			goto unblock_resume;
		} else if (rc == 0) {
			if (arm_state->vc_suspend_state > VC_SUSPEND_IDLE) {
				/* Repeat timeout once if in progress */
				if (repeat < 0) {
					repeat = 1;
					continue;
				}
			}
			arm_state->autosuspend_override++;
			output_timeout_error(state);

			status = VCHIQ_RETRY;
			goto unblock_resume;
		}
	} while (0 < (repeat--));

	/* Check and report state in case we need to abort ARM suspend */
	if (arm_state->vc_suspend_state != VC_SUSPEND_SUSPENDED) {
		status = VCHIQ_RETRY;
		vchiq_log_error(vchiq_susp_log_level,
			"%s videocore suspend failed (state %s)", __func__,
			suspend_state_names[arm_state->vc_suspend_state +
						VC_SUSPEND_NUM_OFFSET]);
		/* Reset the state only if it's still in an error state.
		 * Something could have already initiated another suspend. */
		if (arm_state->vc_suspend_state < VC_SUSPEND_IDLE)
			set_suspend_state(arm_state, VC_SUSPEND_IDLE);

		goto unblock_resume;
	}

	/* successfully suspended - unlock and exit */
	goto unlock;

unblock_resume:
	/* all error states need to unblock resume before exit */
	unblock_resume(arm_state);

unlock:
	write_unlock_bh(&arm_state->susp_res_lock);

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit %d", __func__, status);
	return status;
}

void
vchiq_check_suspend(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	write_lock_bh(&arm_state->susp_res_lock);
	if (arm_state->vc_suspend_state != VC_SUSPEND_SUSPENDED &&
			arm_state->first_connect &&
			!vchiq_videocore_wanted(state)) {
		vchiq_arm_vcsuspend(state);
	}
	write_unlock_bh(&arm_state->susp_res_lock);

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit", __func__);
	return;
}


int
vchiq_arm_allow_resume(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	int resume = 0;
	int ret = -1;

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	write_lock_bh(&arm_state->susp_res_lock);
	unblock_resume(arm_state);
	resume = vchiq_check_resume(state);
	write_unlock_bh(&arm_state->susp_res_lock);

	if (resume) {
		if (wait_for_completion_interruptible(
			&arm_state->vc_resume_complete) < 0) {
			vchiq_log_error(vchiq_susp_log_level,
				"%s interrupted", __func__);
			/* failed, cannot accurately derive suspend
			 * state, so exit early. */
			goto out;
		}
	}

	read_lock_bh(&arm_state->susp_res_lock);
	if (arm_state->vc_suspend_state == VC_SUSPEND_SUSPENDED) {
		vchiq_log_info(vchiq_susp_log_level,
				"%s: Videocore remains suspended", __func__);
	} else {
		vchiq_log_info(vchiq_susp_log_level,
				"%s: Videocore resumed", __func__);
		ret = 0;
	}
	read_unlock_bh(&arm_state->susp_res_lock);
out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit %d", __func__, ret);
	return ret;
}

/* This function should be called with the write lock held */
int
vchiq_check_resume(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	int resume = 0;

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	if (need_resume(state)) {
		set_resume_state(arm_state, VC_RESUME_REQUESTED);
		request_poll(state, NULL, 0);
		resume = 1;
	}

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit", __func__);
	return resume;
}

#ifdef notyet
void
vchiq_platform_check_resume(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	int res = 0;

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	write_lock_bh(&arm_state->susp_res_lock);
	if (arm_state->wake_address == 0) {
		vchiq_log_info(vchiq_susp_log_level,
					"%s: already awake", __func__);
		goto unlock;
	}
	if (arm_state->vc_resume_state == VC_RESUME_IN_PROGRESS) {
		vchiq_log_info(vchiq_susp_log_level,
					"%s: already resuming", __func__);
		goto unlock;
	}

	if (arm_state->vc_resume_state == VC_RESUME_REQUESTED) {
		set_resume_state(arm_state, VC_RESUME_IN_PROGRESS);
		res = 1;
	} else
		vchiq_log_trace(vchiq_susp_log_level,
				"%s: not resuming (resume state %s)", __func__,
				resume_state_names[arm_state->vc_resume_state +
							VC_RESUME_NUM_OFFSET]);

unlock:
	write_unlock_bh(&arm_state->susp_res_lock);

	if (res)
		vchiq_platform_resume(state);

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit", __func__);
	return;

}
#endif



VCHIQ_STATUS_T
vchiq_use_internal(VCHIQ_STATE_T *state, VCHIQ_SERVICE_T *service,
		enum USE_TYPE_E use_type)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	VCHIQ_STATUS_T ret = VCHIQ_SUCCESS;
	char entity[16];
	int *entity_uc;
	int local_uc, local_entity_uc;

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	if (use_type == USE_TYPE_VCHIQ) {
		snprintf(entity, sizeof(entity), "VCHIQ:   ");
		entity_uc = &arm_state->peer_use_count;
	} else if (service) {
		snprintf(entity, sizeof(entity), "%c%c%c%c:%8x",
			VCHIQ_FOURCC_AS_4CHARS(service->base.fourcc),
			service->client_id);
		entity_uc = &service->service_use_count;
	} else {
		vchiq_log_error(vchiq_susp_log_level, "%s null service "
				"ptr", __func__);
		ret = VCHIQ_ERROR;
		goto out;
	}

	write_lock_bh(&arm_state->susp_res_lock);
	while (arm_state->resume_blocked) {
		/* If we call 'use' while force suspend is waiting for suspend,
		 * then we're about to block the thread which the force is
		 * waiting to complete, so we're bound to just time out. In this
		 * case, set the suspend state such that the wait will be
		 * canceled, so we can complete as quickly as possible. */
		if (arm_state->resume_blocked && arm_state->vc_suspend_state ==
				VC_SUSPEND_IDLE) {
			set_suspend_state(arm_state, VC_SUSPEND_FORCE_CANCELED);
			break;
		}
		/* If suspend is already in progress then we need to block */
		if (!try_wait_for_completion(&arm_state->resume_blocker)) {
			/* Indicate that there are threads waiting on the resume
			 * blocker.  These need to be allowed to complete before
			 * a _second_ call to force suspend can complete,
			 * otherwise low priority threads might never actually
			 * continue */
			arm_state->blocked_count++;
			write_unlock_bh(&arm_state->susp_res_lock);
			vchiq_log_info(vchiq_susp_log_level, "%s %s resume "
				"blocked - waiting...", __func__, entity);
			if (wait_for_completion_killable(
					&arm_state->resume_blocker) != 0) {
				vchiq_log_error(vchiq_susp_log_level, "%s %s "
					"wait for resume blocker interrupted",
					__func__, entity);
				ret = VCHIQ_ERROR;
				write_lock_bh(&arm_state->susp_res_lock);
				arm_state->blocked_count--;
				write_unlock_bh(&arm_state->susp_res_lock);
				goto out;
			}
			vchiq_log_info(vchiq_susp_log_level, "%s %s resume "
				"unblocked", __func__, entity);
			write_lock_bh(&arm_state->susp_res_lock);
			if (--arm_state->blocked_count == 0)
				complete_all(&arm_state->blocked_blocker);
		}
	}

	stop_suspend_timer(arm_state);

	local_uc = ++arm_state->videocore_use_count;
	local_entity_uc = ++(*entity_uc);

	/* If there's a pending request which hasn't yet been serviced then
	 * just clear it.  If we're past VC_SUSPEND_REQUESTED state then
	 * vc_resume_complete will block until we either resume or fail to
	 * suspend */
	if (arm_state->vc_suspend_state <= VC_SUSPEND_REQUESTED)
		set_suspend_state(arm_state, VC_SUSPEND_IDLE);

	if ((use_type != USE_TYPE_SERVICE_NO_RESUME) && need_resume(state)) {
		set_resume_state(arm_state, VC_RESUME_REQUESTED);
		vchiq_log_info(vchiq_susp_log_level,
			"%s %s count %d, state count %d",
			__func__, entity, local_entity_uc, local_uc);
		request_poll(state, NULL, 0);
	} else
		vchiq_log_trace(vchiq_susp_log_level,
			"%s %s count %d, state count %d",
			__func__, entity, *entity_uc, local_uc);


	write_unlock_bh(&arm_state->susp_res_lock);

	/* Completion is in a done state when we're not suspended, so this won't
	 * block for the non-suspended case. */
	if (!try_wait_for_completion(&arm_state->vc_resume_complete)) {
		vchiq_log_info(vchiq_susp_log_level, "%s %s wait for resume",
			__func__, entity);
		if (wait_for_completion_killable(
				&arm_state->vc_resume_complete) != 0) {
			vchiq_log_error(vchiq_susp_log_level, "%s %s wait for "
				"resume interrupted", __func__, entity);
			ret = VCHIQ_ERROR;
			goto out;
		}
		vchiq_log_info(vchiq_susp_log_level, "%s %s resumed", __func__,
			entity);
	}

	if (ret == VCHIQ_SUCCESS) {
		VCHIQ_STATUS_T status = VCHIQ_SUCCESS;
		long ack_cnt = atomic_xchg(&arm_state->ka_use_ack_count, 0);
		while (ack_cnt && (status == VCHIQ_SUCCESS)) {
			/* Send the use notify to videocore */
			status = vchiq_send_remote_use_active(state);
			if (status == VCHIQ_SUCCESS)
				ack_cnt--;
			else
				atomic_add(ack_cnt,
					&arm_state->ka_use_ack_count);
		}
	}

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit %d", __func__, ret);
	return ret;
}

VCHIQ_STATUS_T
vchiq_release_internal(VCHIQ_STATE_T *state, VCHIQ_SERVICE_T *service)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	VCHIQ_STATUS_T ret = VCHIQ_SUCCESS;
	char entity[16];
	int *entity_uc;

	if (!arm_state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	if (service) {
		snprintf(entity, sizeof(entity), "%c%c%c%c:%8x",
			VCHIQ_FOURCC_AS_4CHARS(service->base.fourcc),
			service->client_id);
		entity_uc = &service->service_use_count;
	} else {
		snprintf(entity, sizeof(entity), "PEER:   ");
		entity_uc = &arm_state->peer_use_count;
	}

	write_lock_bh(&arm_state->susp_res_lock);
	if (!arm_state->videocore_use_count || !(*entity_uc)) {
		/* Don't use BUG_ON - don't allow user thread to crash kernel */
		WARN_ON(!arm_state->videocore_use_count);
		WARN_ON(!(*entity_uc));
		ret = VCHIQ_ERROR;
		goto unlock;
	}
	--arm_state->videocore_use_count;
	--(*entity_uc);

	if (!vchiq_videocore_wanted(state)) {
		if (vchiq_platform_use_suspend_timer() &&
				!arm_state->resume_blocked) {
			/* Only use the timer if we're not trying to force
			 * suspend (=> resume_blocked) */
			start_suspend_timer(arm_state);
		} else {
			vchiq_log_info(vchiq_susp_log_level,
				"%s %s count %d, state count %d - suspending",
				__func__, entity, *entity_uc,
				arm_state->videocore_use_count);
			vchiq_arm_vcsuspend(state);
		}
	} else
		vchiq_log_trace(vchiq_susp_log_level,
			"%s %s count %d, state count %d",
			__func__, entity, *entity_uc,
			arm_state->videocore_use_count);

unlock:
	write_unlock_bh(&arm_state->susp_res_lock);

out:
	vchiq_log_trace(vchiq_susp_log_level, "%s exit %d", __func__, ret);
	return ret;
}

void
vchiq_on_remote_use(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);
	atomic_inc(&arm_state->ka_use_count);
	complete(&arm_state->ka_evt);
}

void
vchiq_on_remote_release(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);
	atomic_inc(&arm_state->ka_release_count);
	complete(&arm_state->ka_evt);
}

VCHIQ_STATUS_T
vchiq_use_service_internal(VCHIQ_SERVICE_T *service)
{
	return vchiq_use_internal(service->state, service, USE_TYPE_SERVICE);
}

VCHIQ_STATUS_T
vchiq_release_service_internal(VCHIQ_SERVICE_T *service)
{
	return vchiq_release_internal(service->state, service);
}

static void suspend_timer_callback(unsigned long context)
{
	VCHIQ_STATE_T *state = (VCHIQ_STATE_T *)context;
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	if (!arm_state)
		goto out;
	vchiq_log_info(vchiq_susp_log_level,
		"%s - suspend timer expired - check suspend", __func__);
	vchiq_check_suspend(state);
out:
	return;
}

VCHIQ_STATUS_T
vchiq_use_service_no_resume(VCHIQ_SERVICE_HANDLE_T handle)
{
	VCHIQ_STATUS_T ret = VCHIQ_ERROR;
	VCHIQ_SERVICE_T *service = find_service_by_handle(handle);
	if (service) {
		ret = vchiq_use_internal(service->state, service,
				USE_TYPE_SERVICE_NO_RESUME);
		unlock_service(service);
	}
	return ret;
}

VCHIQ_STATUS_T
vchiq_use_service(VCHIQ_SERVICE_HANDLE_T handle)
{
	VCHIQ_STATUS_T ret = VCHIQ_ERROR;
	VCHIQ_SERVICE_T *service = find_service_by_handle(handle);
	if (service) {
		ret = vchiq_use_internal(service->state, service,
				USE_TYPE_SERVICE);
		unlock_service(service);
	}
	return ret;
}

VCHIQ_STATUS_T
vchiq_release_service(VCHIQ_SERVICE_HANDLE_T handle)
{
	VCHIQ_STATUS_T ret = VCHIQ_ERROR;
	VCHIQ_SERVICE_T *service = find_service_by_handle(handle);
	if (service) {
		ret = vchiq_release_internal(service->state, service);
		unlock_service(service);
	}
	return ret;
}

void
vchiq_dump_service_use_state(VCHIQ_STATE_T *state)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	int i, j = 0;
	/* Only dump 64 services */
	static const int local_max_services = 64;
	/* If there's more than 64 services, only dump ones with
	 * non-zero counts */
	int only_nonzero = 0;
	static const char *nz = "<-- preventing suspend";

	enum vc_suspend_status vc_suspend_state;
	enum vc_resume_status  vc_resume_state;
	int peer_count;
	int vc_use_count;
	int active_services;
	struct service_data_struct {
		int fourcc;
		int clientid;
		int use_count;
	} service_data[local_max_services];

	if (!arm_state)
		return;

	read_lock_bh(&arm_state->susp_res_lock);
	vc_suspend_state = arm_state->vc_suspend_state;
	vc_resume_state  = arm_state->vc_resume_state;
	peer_count = arm_state->peer_use_count;
	vc_use_count = arm_state->videocore_use_count;
	active_services = state->unused_service;
	if (active_services > local_max_services)
		only_nonzero = 1;

	for (i = 0; (i < active_services) && (j < local_max_services); i++) {
		VCHIQ_SERVICE_T *service_ptr = state->services[i];
		if (!service_ptr)
			continue;

		if (only_nonzero && !service_ptr->service_use_count)
			continue;

		if (service_ptr->srvstate != VCHIQ_SRVSTATE_FREE) {
			service_data[j].fourcc = service_ptr->base.fourcc;
			service_data[j].clientid = service_ptr->client_id;
			service_data[j++].use_count = service_ptr->
							service_use_count;
		}
	}

	read_unlock_bh(&arm_state->susp_res_lock);

	vchiq_log_warning(vchiq_susp_log_level,
		"-- Videcore suspend state: %s --",
		suspend_state_names[vc_suspend_state + VC_SUSPEND_NUM_OFFSET]);
	vchiq_log_warning(vchiq_susp_log_level,
		"-- Videcore resume state: %s --",
		resume_state_names[vc_resume_state + VC_RESUME_NUM_OFFSET]);

	if (only_nonzero)
		vchiq_log_warning(vchiq_susp_log_level, "Too many active "
			"services (%d).  Only dumping up to first %d services "
			"with non-zero use-count", active_services,
			local_max_services);

	for (i = 0; i < j; i++) {
		vchiq_log_warning(vchiq_susp_log_level,
			"----- %c%c%c%c:%d service count %d %s",
			VCHIQ_FOURCC_AS_4CHARS(service_data[i].fourcc),
			service_data[i].clientid,
			service_data[i].use_count,
			service_data[i].use_count ? nz : "");
	}
	vchiq_log_warning(vchiq_susp_log_level,
		"----- VCHIQ use count count %d", peer_count);
	vchiq_log_warning(vchiq_susp_log_level,
		"--- Overall vchiq instance use count %d", vc_use_count);

	vchiq_dump_platform_use_state(state);
}

VCHIQ_STATUS_T
vchiq_check_service(VCHIQ_SERVICE_T *service)
{
	VCHIQ_ARM_STATE_T *arm_state;
	VCHIQ_STATUS_T ret = VCHIQ_ERROR;

	if (!service || !service->state)
		goto out;

	vchiq_log_trace(vchiq_susp_log_level, "%s", __func__);

	arm_state = vchiq_platform_get_arm_state(service->state);

	read_lock_bh(&arm_state->susp_res_lock);
	if (service->service_use_count)
		ret = VCHIQ_SUCCESS;
	read_unlock_bh(&arm_state->susp_res_lock);

	if (ret == VCHIQ_ERROR) {
		vchiq_log_error(vchiq_susp_log_level,
			"%s ERROR - %c%c%c%c:%8x service count %d, "
			"state count %d, videocore suspend state %s", __func__,
			VCHIQ_FOURCC_AS_4CHARS(service->base.fourcc),
			service->client_id, service->service_use_count,
			arm_state->videocore_use_count,
			suspend_state_names[arm_state->vc_suspend_state +
						VC_SUSPEND_NUM_OFFSET]);
		vchiq_dump_service_use_state(service->state);
	}
out:
	return ret;
}

/* stub functions */
void vchiq_on_remote_use_active(VCHIQ_STATE_T *state)
{
	(void)state;
}

void vchiq_platform_conn_state_changed(VCHIQ_STATE_T *state,
	VCHIQ_CONNSTATE_T oldstate, VCHIQ_CONNSTATE_T newstate)
{
	VCHIQ_ARM_STATE_T *arm_state = vchiq_platform_get_arm_state(state);
	vchiq_log_info(vchiq_susp_log_level, "%d: %s->%s", state->id,
		get_conn_state_name(oldstate), get_conn_state_name(newstate));
	if (state->conn_state == VCHIQ_CONNSTATE_CONNECTED) {
		write_lock_bh(&arm_state->susp_res_lock);
		if (!arm_state->first_connect) {
			char threadname[10];
			arm_state->first_connect = 1;
			write_unlock_bh(&arm_state->susp_res_lock);
			snprintf(threadname, sizeof(threadname), "VCHIQka-%d",
				state->id);
			arm_state->ka_thread = vchiq_thread_create(
				&vchiq_keepalive_thread_func,
				(void *)state,
				threadname);
			if (arm_state->ka_thread == NULL) {
				vchiq_log_error(vchiq_susp_log_level,
					"vchiq: FATAL: couldn't create thread %s",
					threadname);
			} else {
				wake_up_process(arm_state->ka_thread);
			}
		} else
			write_unlock_bh(&arm_state->susp_res_lock);
	}
}

/****************************************************************************
*
*   vchiq_init - called when the module is loaded.
*
***************************************************************************/

int __init vchiq_init(void);
int __init
vchiq_init(void)
{
	int err;

#ifdef notyet
	/* create proc entries */
	err = vchiq_proc_init();
	if (err != 0)
		goto failed_proc_init;
#endif

	vchiq_cdev = make_dev(&vchiq_cdevsw, 0,
	    UID_ROOT, GID_WHEEL, 0600, "vchiq");
	if (!vchiq_cdev) {
		printf("Failed to create /dev/vchiq");
		return (-ENXIO);
	}

	spin_lock_init(&msg_queue_spinlock);

	err = vchiq_platform_init(&g_state);
	if (err != 0)
		goto failed_platform_init;

	vchiq_log_info(vchiq_arm_log_level,
		"vchiq: initialised - version %d (min %d)",
		VCHIQ_VERSION, VCHIQ_VERSION_MIN);

	return 0;

failed_platform_init:
	if (vchiq_cdev) {
		destroy_dev(vchiq_cdev);
		vchiq_cdev = NULL;
	}
	vchiq_log_warning(vchiq_arm_log_level, "could not load vchiq");
	return err;
}

#ifdef notyet
static int vchiq_instance_get_use_count(VCHIQ_INSTANCE_T instance)
{
	VCHIQ_SERVICE_T *service;
	int use_count = 0, i;
	i = 0;
	while ((service = next_service_by_instance(instance->state,
		instance, &i)) != NULL) {
		use_count += service->service_use_count;
		unlock_service(service);
	}
	return use_count;
}

/* read the per-process use-count */
static int proc_read_use_count(char *page, char **start,
			       off_t off, int count,
			       int *eof, void *data)
{
	VCHIQ_INSTANCE_T instance = data;
	int len, use_count;

	use_count = vchiq_instance_get_use_count(instance);
	len = snprintf(page+off, count, "%d\n", use_count);

	return len;
}

/* add an instance (process) to the proc entries */
static int vchiq_proc_add_instance(VCHIQ_INSTANCE_T instance)
{
	char pidstr[32];
	struct proc_dir_entry *top, *use_count;
	struct proc_dir_entry *clients = vchiq_clients_top();
	int pid = instance->pid;

	snprintf(pidstr, sizeof(pidstr), "%d", pid);
	top = proc_mkdir(pidstr, clients);
	if (!top)
		goto fail_top;

	use_count = create_proc_read_entry("use_count",
					   0444, top,
					   proc_read_use_count,
					   instance);
	if (!use_count)
		goto fail_use_count;

	instance->proc_entry = top;

	return 0;

fail_use_count:
	remove_proc_entry(top->name, clients);
fail_top:
	return -ENOMEM;
}

static void vchiq_proc_remove_instance(VCHIQ_INSTANCE_T instance)
{
	struct proc_dir_entry *clients = vchiq_clients_top();
	remove_proc_entry("use_count", instance->proc_entry);
	remove_proc_entry(instance->proc_entry->name, clients);
}

#endif

/****************************************************************************
*
*   vchiq_exit - called when the module is unloaded.
*
***************************************************************************/

void vchiq_exit(void);
void
vchiq_exit(void)
{

	vchiq_platform_exit(&g_state);
	if (vchiq_cdev) {
		destroy_dev(vchiq_cdev);
		vchiq_cdev = NULL;
	}
}
