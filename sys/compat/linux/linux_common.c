/*-
 * Copyright (c) 2014 Vassilis Laganakos
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sx.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>

SET_DECLARE(linux_device_handler_set, struct linux_device_handler);

TAILQ_HEAD(, linux_ioctl_handler_element) linux_ioctl_handlers =
    TAILQ_HEAD_INITIALIZER(linux_ioctl_handlers);
struct sx linux_ioctl_sx;
SX_SYSINIT(linux_ioctl, &linux_ioctl_sx, "Linux ioctl handlers");

static int
linux_common_modevent(module_t mod, int type, void *data)
{
	struct linux_device_handler **ldhp;

	switch(type) {
	case MOD_LOAD:
#ifdef INVARIANTS
		linux_check_errtbl();
#endif
		linux_dev_shm_create();
		linux_osd_jail_register();
		SET_FOREACH(ldhp, linux_device_handler_set)
			linux_device_register_handler(*ldhp);
		linux_netlink_register();
		break;
	case MOD_UNLOAD:
		linux_dev_shm_destroy();
		linux_osd_jail_deregister();
		SET_FOREACH(ldhp, linux_device_handler_set)
			linux_device_unregister_handler(*ldhp);
		linux_netlink_deregister();
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t linux_common_mod = {
	"linux_common",
	linux_common_modevent,
	0
};

DECLARE_MODULE(linux_common, linux_common_mod, SI_SUB_EXEC, SI_ORDER_ANY);
MODULE_VERSION(linux_common, 1);
MODULE_DEPEND(linux_common, netlink, 1, 1, 1);
