#-
# Copyright (c) 2010 Nathan Whitehorn
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#


#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
        
#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/mmuvar.h>

/**
 * MOEA64 kobj methods for 64-bit Book-S page table
 * manipulation routines used, for example, by hypervisors.
 */

INTERFACE moea64;


/**
 * Copy ref/changed bits from PTE referenced by _pt_cookie to _pvo_pt.
 */
METHOD void pte_synch {
	mmu_t		_mmu;
	uintptr_t	_pt_cookie;
	struct lpte	*_pvo_pt;
};

/**
 * Clear bits ptebit (a mask) from the low word of the PTE referenced by
 * _pt_cookie. Note that _pvo_pt is for reference use only -- the bit should
 * NOT be cleared there.
 */
METHOD void pte_clear {
	mmu_t		_mmu;
	uintptr_t	_pt_cookie;
	struct lpte	*_pvo_pt;
	uint64_t	_vpn;
	uint64_t	_ptebit;
};

/**
 * Invalidate the PTE referenced by _pt_cookie, synchronizing its validity
 * and ref/changed bits after completion.
 */
METHOD void pte_unset {
	mmu_t		_mmu;
	uintptr_t	_pt_cookie;
	struct lpte	*_pvo_pt;
	uint64_t	_vpn;
};

/**
 * Update the PTE referenced by _pt_cookie with the values in _pvo_pt,
 * making sure that the values of ref/changed bits are preserved and
 * synchronized back to _pvo_pt.
 */
METHOD void pte_change {
	mmu_t		_mmu;
	uintptr_t	_pt_cookie;
	struct lpte	*_pvo_pt;
	uint64_t	_vpn;
};
	

/**
 * Insert the PTE _pvo_pt into the PTEG group _ptegidx, returning the index
 * of the PTE in its group at completion, or -1 if no slots were free. Must
 * not replace PTEs marked LPTE_WIRED or LPTE_LOCKED, and must set LPTE_HID
 * and LPTE_VALID appropriately in _pvo_pt.
 */
METHOD int pte_insert {
	mmu_t		_mmu;
	u_int		_ptegidx;
	struct lpte	*_pvo_pt;
};

/**
 * Return the page table reference cookie corresponding to _pvo, or -1 if
 * the _pvo is not currently in the page table.
 */
METHOD uintptr_t pvo_to_pte {
	mmu_t		_mmu;
	const struct pvo_entry *_pvo;
};


