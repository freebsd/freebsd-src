/*
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

#include <sys/systm.h>
#include <machine/clock.h>
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/ntoskrnl_var.h>

#define __stdcall __attribute__((__stdcall__))
#define FUNC void(*)(void)

__stdcall static void hal_stall_exec_cpu(uint32_t);
__stdcall static void hal_writeport_ulong(uint32_t *, uint32_t);
__stdcall static void hal_writeport_ushort(uint16_t *, uint16_t);
__stdcall static void hal_writeport_uchar(uint8_t *, uint8_t);
__stdcall static uint32_t hal_readport_ulong(uint32_t *);
__stdcall static uint16_t hal_readport_ushort(uint16_t *);
__stdcall static uint8_t hal_readport_uchar(uint8_t *);
__stdcall static uint8_t hal_lock(/*kspin_lock * */void);
__stdcall static void hal_unlock(/*kspin_lock *, uint8_t*/void);
__stdcall static uint8_t hal_irql(void);
__stdcall static void dummy (void);

__stdcall static void
hal_stall_exec_cpu(usecs)
	uint32_t		usecs;
{
	DELAY(usecs);
	return;
}

__stdcall static void
hal_writeport_ulong(port, val)
	uint32_t		*port;
	uint32_t		val;
{
	bus_space_write_4(I386_BUS_SPACE_IO, 0x0, (uint32_t)port, val);
	return;
}

__stdcall static void
hal_writeport_ushort(port, val)
	uint16_t		*port;
	uint16_t		val;
{
	bus_space_write_2(I386_BUS_SPACE_IO, 0x0, (uint32_t)port, val);
	return;
}

__stdcall static void
hal_writeport_uchar(port, val)
	uint8_t			*port;
	uint8_t			val;
{
	bus_space_write_1(I386_BUS_SPACE_IO, 0x0, (uint32_t)port, val);
	return;
}

__stdcall static uint16_t
hal_readport_ushort(port)
	uint16_t		*port;
{
	return(bus_space_read_2(I386_BUS_SPACE_IO, 0x0, (uint32_t)port));
}

__stdcall static uint32_t
hal_readport_ulong(port)
	uint32_t		*port;
{
	return(bus_space_read_4(I386_BUS_SPACE_IO, 0x0, (uint32_t)port));
}

__stdcall static uint8_t
hal_readport_uchar(port)
	uint8_t			*port;
{
	return(bus_space_read_1(I386_BUS_SPACE_IO, 0x0, (uint32_t)port));
}

__stdcall static uint8_t
hal_lock(/*lock*/void)
{
	kspin_lock		*lock;

	__asm__ __volatile__ ("" : "=c" (lock));

	mtx_lock((struct mtx *)*lock);
	return(0);
}

__stdcall static void
hal_unlock(/*lock, newirql*/void)
{
	kspin_lock		*lock;
	uint8_t			newiqrl;

	__asm__ __volatile__ ("" : "=c" (lock), "=d" (newiqrl));

	mtx_unlock((struct mtx *)*lock);
	return;
}

__stdcall static uint8_t
hal_irql(void)
{
	return(0);
}

__stdcall
static void dummy()
{
	printf ("hal dummy called...\n");
	return;
}

image_patch_table hal_functbl[] = {
	{ "KeStallExecutionProcessor", (FUNC)hal_stall_exec_cpu },
	{ "WRITE_PORT_ULONG", (FUNC)hal_writeport_ulong },
	{ "WRITE_PORT_USHORT", (FUNC)hal_writeport_ushort },
	{ "WRITE_PORT_UCHAR", (FUNC)hal_writeport_uchar },
	{ "READ_PORT_ULONG", (FUNC)hal_readport_ulong },
	{ "READ_PORT_USHORT", (FUNC)hal_readport_ushort },
	{ "READ_PORT_UCHAR", (FUNC)hal_readport_uchar },
	{ "KfAcquireSpinLock", (FUNC)hal_lock },
	{ "KfReleaseSpinLock", (FUNC)hal_unlock },
	{ "KeGetCurrentIrql", (FUNC)hal_irql },

	/*
	 * This last entry is a catch-all for any function we haven't
	 * implemented yet. The PE import list patching routine will
	 * use it for any function that doesn't have an explicit match
	 * in this table.
	 */

	{ NULL, (FUNC)dummy },

	/* End of list. */

	{ NULL, NULL },
};
