/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2025 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Hardware Tracing framework.
 *
 *    The framework manages hardware tracing units that collect information
 * about software execution and store it as events in highly compressed format
 * into DRAM. The events cover information about control flow changes of a
 * program, whether branches taken or not, exceptions taken, timing information,
 * cycles elapsed and more. That allows us to restore entire program flow of a
 * given application without performance impact.
 *
 * Design overview.
 *
 *    The framework provides character devices for mmap(2) and ioctl(2) system
 * calls to allow user to manage CPU (hardware) tracing units.
 *
 * /dev/hwt:
 *    .ioctl:
 *        hwt_ioctl():
 *               a) HWT_IOC_ALLOC
 *                  Allocates kernel tracing context CTX based on requested mode
 *                  of operation. Verifies the information that comes with the
 *                  request (pid, cpus), allocates unique ID for the context.
 *                  Creates a new character device for CTX management.
 *
 * /dev/hwt_%d[_%d], ident[, thread_id]
 *    .mmap
 *        Maps tracing buffers of the corresponding thread to userspace.
 *    .ioctl
 *        hwt_thread_ioctl():
 *               a) HWT_IOC_START
 *                  Enables tracing unit for a given context.
 *               b) HWT_IOC_RECORD_GET
 *                  Transfers (small) record entries collected during program
 *                  execution for a given context to userspace, such as mmaping
 *                  tables of executable and dynamic libraries, interpreter,
 *                  kernel mappings, tid of threads created, etc.
 *               c) HWT_IOC_SET_CONFIG
 *                  Allows to specify backend-specific configuration of the
 *                  trace unit.
 *               d) HWT_IOC_WAKEUP
 *                  Wakes up a thread that is currently sleeping.
 *               e) HWT_IOC_BUFPTR_GET
 *                  Transfers current hardware pointer in the filling buffer
 *                  to the userspace.
 *               f) HWT_IOC_SVC_BUF
 *                  To avoid data loss, userspace may notify kernel it has
 *                  copied out the given buffer, so kernel is ok to overwrite
 *
 * HWT context lifecycle in THREAD mode of operation:
 * 1. User invokes HWT_IOC_ALLOC ioctl with information about pid to trace and
 *    size of the buffers for the trace data to allocate.
 *    Some architectures may have different tracing units supported, so user
 *    also provides backend name to use for this context, e.g. "coresight".
 * 2. Kernel allocates context, lookups the proc for the given pid. Then it
 *    creates first hwt_thread in the context and allocates trace buffers for
 *    it. Immediately, kernel initializes tracing backend.
 *    Kernel creates character device and returns unique identificator of
 *    trace context to the user.
 * 3. To manage the new context, user opens the character device created.
 *    User invokes HWT_IOC_START ioctl, kernel marks context as RUNNING.
 *    At this point any HWT hook invocation by scheduler enables/disables
 *    tracing for threads associated with the context (threads of the proc).
 *    Any new threads creation (of the target proc) procedures will be invoking
 *    corresponding hooks in HWT framework, so that new hwt_thread and buffers
 *    allocated, character device for mmap(2) created on the fly.
 * 4. User issues HWT_IOC_RECORD_GET ioctl to fetch information about mmaping
 *    tables and threads created during application startup.
 * 5. User mmaps tracing buffers of each thread to userspace (using
 *    /dev/hwt_%d_%d % (ident, thread_id) character devices).
 * 6. User can repeat 4 if expected thread is not yet created during target
 *    application execution.
 * 7. User issues HWT_IOC_BUFPTR_GET ioctl to get current filling level of the
 *    hardware buffer of a given thread.
 * 8. User invokes trace decoder library to process available data and see the
 *    results in human readable form.
 * 9. User repeats 7 if needed.
 *
 * HWT context lifecycle in CPU mode of operation:
 * 1. User invokes HWT_IOC_ALLOC ioctl providing a set of CPU to trace within
 *    single CTX.
 * 2. Kernel verifies the set of CPU and allocates tracing context, creates
 *    a buffer for each CPU.
 *    Kernel creates a character device for every CPU provided in the request.
 *    Kernel initialized tracing backend.
 * 3. User opens character devices of interest to map the buffers to userspace.
 *    User can start tracing by invoking HWT_IOC_START on any of character
 *    device within the context, entire context will be marked as RUNNING.
 * 4. The rest is similar to the THREAD mode.
 *
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_contexthash.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_owner.h>
#include <dev/hwt/hwt_ownerhash.h>
#include <dev/hwt/hwt_backend.h>
#include <dev/hwt/hwt_record.h>
#include <dev/hwt/hwt_ioctl.h>
#include <dev/hwt/hwt_hook.h>

#define	HWT_DEBUG
#undef	HWT_DEBUG

#ifdef	HWT_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static eventhandler_tag hwt_exit_tag;
static struct cdev *hwt_cdev;
static struct cdevsw hwt_cdevsw = {
	.d_version	= D_VERSION,
	.d_name		= "hwt",
	.d_mmap_single	= NULL,
	.d_ioctl	= hwt_ioctl
};

static void
hwt_process_exit(void *arg __unused, struct proc *p)
{
	struct hwt_owner *ho;

	/* Stop HWTs associated with exiting owner, if any. */
	ho = hwt_ownerhash_lookup(p);
	if (ho)
		hwt_owner_shutdown(ho);
}

static int
hwt_load(void)
{
	struct make_dev_args args;
	int error;

	make_dev_args_init(&args);
	args.mda_devsw = &hwt_cdevsw;
	args.mda_flags = MAKEDEV_CHECKNAME | MAKEDEV_WAITOK;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = 0660;
	args.mda_si_drv1 = NULL;

	hwt_backend_load();
	hwt_ctx_load();
	hwt_contexthash_load();
	hwt_ownerhash_load();
	hwt_record_load();

	error = make_dev_s(&args, &hwt_cdev, "hwt");
	if (error != 0)
		return (error);

	hwt_exit_tag = EVENTHANDLER_REGISTER(process_exit, hwt_process_exit,
	    NULL, EVENTHANDLER_PRI_ANY);

	hwt_hook_load();

	return (0);
}

static int
hwt_unload(void)
{

	hwt_hook_unload();
	EVENTHANDLER_DEREGISTER(process_exit, hwt_exit_tag);
	destroy_dev(hwt_cdev);
	hwt_record_unload();
	hwt_ownerhash_unload();
	hwt_contexthash_unload();
	hwt_ctx_unload();
	hwt_backend_unload();

	return (0);
}

static int
hwt_modevent(module_t mod, int type, void *data)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		error = hwt_load();
		break;
	case MOD_UNLOAD:
		error = hwt_unload();
		break;
	default:
		error = 0;
		break;
	}

	return (error);
}

static moduledata_t hwt_mod = {
	"hwt",
	hwt_modevent,
	NULL
};

DECLARE_MODULE(hwt, hwt_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(hwt, 1);
