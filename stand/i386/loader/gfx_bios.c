/*
 * Copyright (c) 2024 Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * This file provides all the gfx glue, or stubs, so that we can build, if we
 * want, two versions of the bios loader: one with graphics support and one
 * without. This allows us to keep the calls in other places, like libraries
 * that are tricky to compile twice. It also reduces the number of ifdefs we
 * need to support the old text-only video console. This could also be two
 * separate files, but it is short and having it all here helps to constrain
 * dependency creap somewhat.
 */

#include <stand.h>
#ifndef BIOS_TEXT_ONLY
#include "bootstrap.h"
#include "libi386/libi386.h"
#include "libi386/vbe.h"
#include <gfx_fb.h>
#endif

#ifdef BIOS_TEXT_ONLY		/* Note: likely need a forced commits when this changes */
void autoload_font(bool bios);

void
autoload_font(bool bios)
{
}

vm_offset_t build_font_module(vm_offset_t addr);

vm_offset_t
build_font_module(vm_offset_t addr)
{
	return addr;
}

struct preloaded_file;
void bi_load_vbe_data(struct preloaded_file *kfp);

void bi_load_vbe_data(struct preloaded_file *kfp)
{
}

#else

void
bi_load_vbe_data(struct preloaded_file *kfp)
{
	if (!kfp->f_tg_kernel_support) {
		/*
		 * Loaded kernel does not have vt/vbe backend,
		 * switch console to text mode.
		 */
		if (vbe_available())
			bios_set_text_mode(VGA_TEXT_MODE);
		return;
	}

	if (vbe_available()) {
		file_addmetadata(kfp, MODINFOMD_VBE_FB,
		    sizeof(gfx_state.tg_fb), &gfx_state.tg_fb);
	}
}
#endif
