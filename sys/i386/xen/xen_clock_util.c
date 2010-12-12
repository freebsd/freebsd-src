/*-
 * Copyright (c) 2009 Adrian Chadd
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
 *
 * $FreeBSD: stable/8/sys/i386/xen/xen_clock_util.c 199583 2009-11-20 15:27:52Z jhb $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/8/sys/i386/xen/xen_clock_util.c 199583 2009-11-20 15:27:52Z jhb $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>

#include <xen/xen_intr.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <xen/hypervisor.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenfunc.h>
#include <xen/interface/io/xenbus.h>
#include <xen/interface/vcpu.h>
#include <machine/cpu.h>

#include <machine/xen/xen_clock_util.h>

/*
 * Read the current hypervisor start time (wall clock) from Xen.
 */
void
xen_fetch_wallclock(struct timespec *ts)
{ 
        shared_info_t *s = HYPERVISOR_shared_info;
        uint32_t ts_version;
   
        do {
                ts_version = s->wc_version;
                rmb();
                ts->tv_sec  = s->wc_sec;
                ts->tv_nsec = s->wc_nsec;
                rmb();
        }
        while ((s->wc_version & 1) | (ts_version ^ s->wc_version));
}

/*
 * Read the current hypervisor system uptime value from Xen.
 */
void
xen_fetch_uptime(struct timespec *ts)
{
        shared_info_t           *s = HYPERVISOR_shared_info;
        struct vcpu_time_info   *src;
	struct shadow_time_info	dst;
        uint32_t pre_version, post_version;
        
        src = &s->vcpu_info[smp_processor_id()].time;

        spinlock_enter();
        do {
                pre_version = dst.version = src->version;
                rmb();
                dst.system_timestamp  = src->system_time;
                rmb();
                post_version = src->version;
        }
        while ((pre_version & 1) | (pre_version ^ post_version));

        spinlock_exit();

	ts->tv_sec = dst.system_timestamp / 1000000000;
	ts->tv_nsec = dst.system_timestamp % 1000000000;
}
