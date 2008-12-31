/*-
 * Copyright (c) 2005 Rink Springer
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/xbox/xbox.c,v 1.4.4.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/reboot.h>
#include <machine/xbox.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#ifndef I686_CPU
#error You must have a I686_CPU in your kernel if you want to make an XBOX-compatible kernel
#endif

static void
xbox_poweroff(void* junk, int howto)
{
	if (!(howto & RB_POWEROFF))
		return;

	pic16l_poweroff();
}

static void
xbox_init(void)
{
	char* ptr;

	if (!arch_i386_is_xbox)
		return;

	/* register our poweroff function */
	EVENTHANDLER_REGISTER (shutdown_final, xbox_poweroff, NULL,
	                       SHUTDOWN_PRI_LAST);

	/*
	 * Some XBOX loaders, such as Cromwell, have a flaw which cause the
	 * nve(4) driver to fail attaching to the NIC.
	 *
	 * This is because they leave the NIC running; this will cause the
	 * Nvidia driver to fail as the NIC does not return any sensible
	 * values and thus fails attaching (using an error 0x5, this means
	 * it cannot find a valid PHY)
	 *
	 * We bluntly tell the NIC to stop whatever it's doing; this makes
	 * nve(4) attach correctly. As the NIC always resides at
	 * 0xfef00000-0xfef003ff on an XBOX, we simply hardcode this address.
	 */
	ptr = pmap_mapdev (0xfef00000, 0x400);
	*(uint32_t*)(ptr + 0x188) = 0; /* clear adapter control field */
	pmap_unmapdev ((vm_offset_t)ptr, 0x400);
}

/*
 * This must be called before the drivers, as the if_nve(4) driver will fail
 * if we do not do this in advance.
 */
SYSINIT(xbox, SI_SUB_DRIVERS, SI_ORDER_FIRST, xbox_init, NULL);
