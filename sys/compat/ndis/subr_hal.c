/*-
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/module.h>

#include <sys/systm.h>
#include <machine/clock.h>
#include <machine/bus.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/hal_var.h>

static void KeStallExecutionProcessor(uint32_t);
static void WRITE_PORT_BUFFER_ULONG(uint32_t *,
	uint32_t *, uint32_t);
static void WRITE_PORT_BUFFER_USHORT(uint16_t *,
	uint16_t *, uint32_t);
static void WRITE_PORT_BUFFER_UCHAR(uint8_t *,
	uint8_t *, uint32_t);
static void WRITE_PORT_ULONG(uint32_t *, uint32_t);
static void WRITE_PORT_USHORT(uint16_t *, uint16_t);
static void WRITE_PORT_UCHAR(uint8_t *, uint8_t);
static uint32_t READ_PORT_ULONG(uint32_t *);
static uint16_t READ_PORT_USHORT(uint16_t *);
static uint8_t READ_PORT_UCHAR(uint8_t *);
static void READ_PORT_BUFFER_ULONG(uint32_t *,
	uint32_t *, uint32_t);
static void READ_PORT_BUFFER_USHORT(uint16_t *,
	uint16_t *, uint32_t);
static void READ_PORT_BUFFER_UCHAR(uint8_t *,
	uint8_t *, uint32_t);
static uint64_t KeQueryPerformanceCounter(uint64_t *);
static void dummy (void);

extern struct mtx_pool *ndis_mtxpool;

int
hal_libinit()
{
	image_patch_table	*patch;

	patch = hal_functbl;
	while (patch->ipt_func != NULL) {
		windrv_wrap((funcptr)patch->ipt_func,
		    (funcptr *)&patch->ipt_wrap,
		    patch->ipt_argcnt, patch->ipt_ftype);
		patch++;
	}

	return(0);
}

int
hal_libfini()
{
	image_patch_table	*patch;

	patch = hal_functbl;
	while (patch->ipt_func != NULL) {
		windrv_unwrap(patch->ipt_wrap);
		patch++;
	}

	return(0);
}

static void
KeStallExecutionProcessor(usecs)
	uint32_t		usecs;
{
	DELAY(usecs);
	return;
}

static void
WRITE_PORT_ULONG(port, val)
	uint32_t		*port;
	uint32_t		val;
{
	bus_space_write_4(NDIS_BUS_SPACE_IO, 0x0, (bus_size_t)port, val);
	return;
}

static void
WRITE_PORT_USHORT(port, val)
	uint16_t		*port;
	uint16_t		val;
{
	bus_space_write_2(NDIS_BUS_SPACE_IO, 0x0, (bus_size_t)port, val);
	return;
}

static void
WRITE_PORT_UCHAR(port, val)
	uint8_t			*port;
	uint8_t			val;
{
	bus_space_write_1(NDIS_BUS_SPACE_IO, 0x0, (bus_size_t)port, val);
	return;
}

static void
WRITE_PORT_BUFFER_ULONG(port, val, cnt)
	uint32_t		*port;
	uint32_t		*val;
	uint32_t		cnt;
{
	bus_space_write_multi_4(NDIS_BUS_SPACE_IO, 0x0,
	    (bus_size_t)port, val, cnt);
	return;
}

static void
WRITE_PORT_BUFFER_USHORT(port, val, cnt)
	uint16_t		*port;
	uint16_t		*val;
	uint32_t		cnt;
{
	bus_space_write_multi_2(NDIS_BUS_SPACE_IO, 0x0,
	    (bus_size_t)port, val, cnt);
	return;
}

static void
WRITE_PORT_BUFFER_UCHAR(port, val, cnt)
	uint8_t			*port;
	uint8_t			*val;
	uint32_t		cnt;
{
	bus_space_write_multi_1(NDIS_BUS_SPACE_IO, 0x0,
	    (bus_size_t)port, val, cnt);
	return;
}

static uint16_t
READ_PORT_USHORT(port)
	uint16_t		*port;
{
	return(bus_space_read_2(NDIS_BUS_SPACE_IO, 0x0, (bus_size_t)port));
}

static uint32_t
READ_PORT_ULONG(port)
	uint32_t		*port;
{
	return(bus_space_read_4(NDIS_BUS_SPACE_IO, 0x0, (bus_size_t)port));
}

static uint8_t
READ_PORT_UCHAR(port)
	uint8_t			*port;
{
	return(bus_space_read_1(NDIS_BUS_SPACE_IO, 0x0, (bus_size_t)port));
}

static void
READ_PORT_BUFFER_ULONG(port, val, cnt)
	uint32_t		*port;
	uint32_t		*val;
	uint32_t		cnt;
{
	bus_space_read_multi_4(NDIS_BUS_SPACE_IO, 0x0,
	    (bus_size_t)port, val, cnt);
	return;
}

static void
READ_PORT_BUFFER_USHORT(port, val, cnt)
	uint16_t		*port;
	uint16_t		*val;
	uint32_t		cnt;
{
	bus_space_read_multi_2(NDIS_BUS_SPACE_IO, 0x0,
	    (bus_size_t)port, val, cnt);
	return;
}

static void
READ_PORT_BUFFER_UCHAR(port, val, cnt)
	uint8_t			*port;
	uint8_t			*val;
	uint32_t		cnt;
{
	bus_space_read_multi_1(NDIS_BUS_SPACE_IO, 0x0,
	    (bus_size_t)port, val, cnt);
	return;
}

/*
 * The spinlock implementation in Windows differs from that of FreeBSD.
 * The basic operation of spinlocks involves two steps: 1) spin in a
 * tight loop while trying to acquire a lock, 2) after obtaining the
 * lock, disable preemption. (Note that on uniprocessor systems, you're
 * allowed to skip the first step and just lock out pre-emption, since
 * it's not possible for you to be in contention with another running
 * thread.) Later, you release the lock then re-enable preemption.
 * The difference between Windows and FreeBSD lies in how preemption
 * is disabled. In FreeBSD, it's done using critical_enter(), which on
 * the x86 arch translates to a cli instruction. This masks off all
 * interrupts, and effectively stops the scheduler from ever running
 * so _nothing_ can execute except the current thread. In Windows,
 * preemption is disabled by raising the processor IRQL to DISPATCH_LEVEL.
 * This stops other threads from running, but does _not_ block device
 * interrupts. This means ISRs can still run, and they can make other
 * threads runable, but those other threads won't be able to execute
 * until the current thread lowers the IRQL to something less than
 * DISPATCH_LEVEL.
 *
 * There's another commonly used IRQL in Windows, which is APC_LEVEL.
 * An APC is an Asynchronous Procedure Call, which differs from a DPC
 * (Defered Procedure Call) in that a DPC is queued up to run in
 * another thread, while an APC runs in the thread that scheduled
 * it (similar to a signal handler in a UNIX process). We don't
 * actually support the notion of APCs in FreeBSD, so for now, the
 * only IRQLs we're interested in are DISPATCH_LEVEL and PASSIVE_LEVEL.
 *
 * To simulate DISPATCH_LEVEL, we raise the current thread's priority
 * to PI_REALTIME, which is the highest we can give it. This should,
 * if I understand things correctly, prevent anything except for an
 * interrupt thread from preempting us. PASSIVE_LEVEL is basically
 * everything else.
 *
 * Be aware that, at least on the x86 arch, the Windows spinlock
 * functions are divided up in peculiar ways. The actual spinlock
 * functions are KfAcquireSpinLock() and KfReleaseSpinLock(), and
 * they live in HAL.dll. Meanwhile, KeInitializeSpinLock(),
 * KefAcquireSpinLockAtDpcLevel() and KefReleaseSpinLockFromDpcLevel()
 * live in ntoskrnl.exe. Most Windows source code will call
 * KeAcquireSpinLock() and KeReleaseSpinLock(), but these are just
 * macros that call KfAcquireSpinLock() and KfReleaseSpinLock().
 * KefAcquireSpinLockAtDpcLevel() and KefReleaseSpinLockFromDpcLevel()
 * perform the lock aquisition/release functions without doing the
 * IRQL manipulation, and are used when one is already running at
 * DISPATCH_LEVEL. Make sense? Good.
 *
 * According to the Microsoft documentation, any thread that calls
 * KeAcquireSpinLock() must be running at IRQL <= DISPATCH_LEVEL. If
 * we detect someone trying to acquire a spinlock from DEVICE_LEVEL
 * or HIGH_LEVEL, we panic.
 */

uint8_t
KfAcquireSpinLock(lock)
	kspin_lock		*lock;
{
	uint8_t			oldirql;

	/* I am so going to hell for this. */
	if (KeGetCurrentIrql() > DISPATCH_LEVEL)
		panic("IRQL_NOT_LESS_THAN_OR_EQUAL");

	oldirql = KeRaiseIrql(DISPATCH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(lock);

	return(oldirql);
}

void
KfReleaseSpinLock(lock, newirql)
	kspin_lock		*lock;
	uint8_t			newirql;
{
	KeReleaseSpinLockFromDpcLevel(lock);
	KeLowerIrql(newirql);

	return;
}

 uint8_t
KeGetCurrentIrql()
{
	if (AT_DISPATCH_LEVEL(curthread))
		return(DISPATCH_LEVEL);
	return(PASSIVE_LEVEL);
}

static uint64_t
KeQueryPerformanceCounter(freq)
	uint64_t		*freq;
{
	if (freq != NULL)
		*freq = hz;

	return((uint64_t)ticks);
}

uint8_t
KfRaiseIrql(irql)
	uint8_t			irql;
{
	uint8_t			oldirql;

	if (irql < KeGetCurrentIrql())
		panic("IRQL_NOT_LESS_THAN");

	if (KeGetCurrentIrql() == DISPATCH_LEVEL)
		return(DISPATCH_LEVEL);

	mtx_lock_spin(&sched_lock);
	oldirql = curthread->td_base_pri;
	sched_prio(curthread, PI_REALTIME);
#if __FreeBSD_version < 600000
	curthread->td_base_pri = PI_REALTIME;
#endif
	mtx_unlock_spin(&sched_lock);

	return(oldirql);
}

void 
KfLowerIrql(oldirql)
	uint8_t			oldirql;
{
	if (oldirql == DISPATCH_LEVEL)
		return;

	if (KeGetCurrentIrql() != DISPATCH_LEVEL)
		panic("IRQL_NOT_GREATER_THAN");

	mtx_lock_spin(&sched_lock);
#if __FreeBSD_version < 600000
	curthread->td_base_pri = oldirql;
#endif
	sched_prio(curthread, oldirql);
	mtx_unlock_spin(&sched_lock);

	return;
}

static void dummy()
{
	printf ("hal dummy called...\n");
	return;
}

image_patch_table hal_functbl[] = {
	IMPORT_SFUNC(KeStallExecutionProcessor, 1),
	IMPORT_SFUNC(WRITE_PORT_ULONG, 2),
	IMPORT_SFUNC(WRITE_PORT_USHORT, 2),
	IMPORT_SFUNC(WRITE_PORT_UCHAR, 2),
	IMPORT_SFUNC(WRITE_PORT_BUFFER_ULONG, 3),
	IMPORT_SFUNC(WRITE_PORT_BUFFER_USHORT, 3),
	IMPORT_SFUNC(WRITE_PORT_BUFFER_UCHAR, 3),
	IMPORT_SFUNC(READ_PORT_ULONG, 1),
	IMPORT_SFUNC(READ_PORT_USHORT, 1),
	IMPORT_SFUNC(READ_PORT_UCHAR, 1),
	IMPORT_SFUNC(READ_PORT_BUFFER_ULONG, 3),
	IMPORT_SFUNC(READ_PORT_BUFFER_USHORT, 3),
	IMPORT_SFUNC(READ_PORT_BUFFER_UCHAR, 3),
	IMPORT_FFUNC(KfAcquireSpinLock, 1),
	IMPORT_FFUNC(KfReleaseSpinLock, 1),
	IMPORT_SFUNC(KeGetCurrentIrql, 0),
	IMPORT_SFUNC(KeQueryPerformanceCounter, 1),
	IMPORT_FFUNC(KfLowerIrql, 1),
	IMPORT_FFUNC(KfRaiseIrql, 1),

	/*
	 * This last entry is a catch-all for any function we haven't
	 * implemented yet. The PE import list patching routine will
	 * use it for any function that doesn't have an explicit match
	 * in this table.
	 */

	{ NULL, (FUNC)dummy, NULL, 0, WINDRV_WRAP_STDCALL },

	/* End of list. */

	{ NULL, NULL, NULL }
};
