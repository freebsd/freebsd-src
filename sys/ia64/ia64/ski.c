/*-
 * Copyright (c) 2001 Doug Rabson
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
 * $FreeBSD$
 */

/*
 * Fake out bits of EFI and SAL when running under SKI.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/efi.h>
#include <machine/sal.h>
#include <machine/md_var.h>

extern u_int64_t ski_fake_pal[];	// *not* a function decl
extern void ia64_ski_init(void);
extern u_int64_t ia64_pal_entry;

static EFI_STATUS ski_fake_efi_proc(void);

static EFI_RUNTIME_SERVICES ski_fake_efi = {
	{ EFI_RUNTIME_SERVICES_SIGNATURE,
	  EFI_RUNTIME_SERVICES_REVISION,
	  0, 0, 0 },

	(EFI_GET_TIME)			ski_fake_efi_proc,
	(EFI_SET_TIME)			ski_fake_efi_proc,
	(EFI_GET_WAKEUP_TIME)		ski_fake_efi_proc,
	(EFI_SET_WAKEUP_TIME)		ski_fake_efi_proc,
	
	(EFI_SET_VIRTUAL_ADDRESS_MAP)	ski_fake_efi_proc,
	(EFI_CONVERT_POINTER)		ski_fake_efi_proc,

	(EFI_GET_VARIABLE)		ski_fake_efi_proc,
	(EFI_GET_NEXT_VARIABLE_NAME)	ski_fake_efi_proc,
	(EFI_SET_VARIABLE)		ski_fake_efi_proc,

	(EFI_GET_NEXT_HIGH_MONO_COUNT)	ski_fake_efi_proc,
	(EFI_RESET_SYSTEM)		ski_fake_efi_proc
};

static EFI_STATUS
ski_fake_efi_proc(void)
{
	return EFI_UNSUPPORTED;
}

static struct ia64_sal_result
ski_fake_sal(u_int64_t a1, u_int64_t a2, u_int64_t a3, u_int64_t a4,
	     u_int64_t a5, u_int64_t a6, u_int64_t a7, u_int64_t a8)
{
	struct ia64_sal_result res;

	if (a1 == SAL_FREQ_BASE) {
		/*
		 * Fake the values from my SDV.
		 */
		res.sal_status = 0;
		res.sal_result[0] = 133347096;
		res.sal_result[1] = 0;
		res.sal_result[2] = 0;
		return res;
	}

	/*
	 * Return an error for anything we don't care about.
	 */
	res.sal_status = -3;
	res.sal_result[0] = 0;
	res.sal_result[1] = 0;
	res.sal_result[2] = 0;
	return res;
}

void
ia64_ski_init(void)
{
	if (!ia64_running_in_simulator())
		return;

	ia64_efi_runtime = &ski_fake_efi;
	ia64_pal_entry = (u_int64_t) ski_fake_pal;
	ia64_sal_entry = ski_fake_sal;
}
