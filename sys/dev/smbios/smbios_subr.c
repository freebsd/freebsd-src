/*-
 * Copyright 2014 John Baldwin
 * Copyright 2019 Stephen J. Kiernan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Id: machdep.c,v 1.193 1996/06/18 01:22:04 bde Exp
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/smbios/smbios.h>

static const struct {
	const char	*vm_bname;
	int		vm_guest;
} vm_bnames[] = {
	{ "QEMU",	VM_GUEST_VM },		/* QEMU */
	{ "Plex86",	VM_GUEST_VM },		/* Plex86 */
	{ "Bochs",	VM_GUEST_VM },		/* Bochs */
	{ "Xen",	VM_GUEST_XEN },		/* Xen */
	{ "BHYVE",	VM_GUEST_BHYVE },	/* bhyve */
	{ "Seabios",	VM_GUEST_KVM },		/* KVM */
};

static const struct {
	const char	*vm_pname;
	int		vm_guest;
} vm_pnames[] = {
	{ "VMware Virtual Platform",	VM_GUEST_VMWARE },
	{ "Virtual Machine",		VM_GUEST_VM }, /* Microsoft VirtualPC */
	{ "QEMU Virtual Machine",	VM_GUEST_VM },
	{ "VirtualBox",			VM_GUEST_VBOX },
	{ "Parallels Virtual Platform",	VM_GUEST_PARALLELS },
	{ "KVM",			VM_GUEST_KVM },
};

void
identify_hypervisor_smbios(void)
{
	char *p;
	int i;

	/*
	 * Some platforms, e.g., amd64, have other ways of detecting what kind
	 * of hypervisor we may be running under.  Make sure we don't clobber a
	 * more specific vm_guest that's been previously detected.
	 */
	if (vm_guest != VM_GUEST_NO && vm_guest != VM_GUEST_VM)
		return;

	/*
	 * XXX: Some of these entries may not be needed since they were
	 * added to FreeBSD before the checks above.
	 */
	p = kern_getenv("smbios.bios.vendor");
	if (p != NULL) {
		for (i = 0; i < nitems(vm_bnames); i++)
			if (strcmp(p, vm_bnames[i].vm_bname) == 0) {
				vm_guest = vm_bnames[i].vm_guest;
				/* If we have a specific match, return */
				if (vm_guest != VM_GUEST_VM) {
					freeenv(p);
					return;
				}
				/*
				 * We are done with bnames, but there might be
				 * a more specific match in the pnames
				 */
				break;
			}
		freeenv(p);
	}
	p = kern_getenv("smbios.system.product");
	if (p != NULL) {
		for (i = 0; i < nitems(vm_pnames); i++)
			if (strcmp(p, vm_pnames[i].vm_pname) == 0) {
				vm_guest = vm_pnames[i].vm_guest;
				freeenv(p);
				return;
			}
		freeenv(p);
	}
}
