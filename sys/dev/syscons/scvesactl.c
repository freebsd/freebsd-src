/*-
 * Copyright (c) 1998 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $Id: scvesactl.c,v 1.3 1998/09/25 11:55:46 yokota Exp $
 */

#include "sc.h"
#include "opt_vesa.h"
#include "opt_vm86.h"

#if (NSC > 0 && defined(VESA) && defined(VM86)) || defined(VESA_MODULE)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/kernel.h>

#include <machine/apm_bios.h>
#include <machine/console.h>
#include <machine/pc/vesa.h>

#include <i386/isa/videoio.h>
#include <i386/isa/syscons.h>

static d_ioctl_t *prev_user_ioctl;

static int
vesa_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	scr_stat *scp;
	struct tty *tp;
	video_info_t info;
	video_adapter_t *adp;
	int mode;
	int error;
	int s;

	tp = scdevtotty(dev);
	if (!tp)
		return ENXIO;
	scp = sc_get_scr_stat(tp->t_dev);

	switch (cmd) {
	case SW_VESA_USER:

		mode = (int)data;
		if ((*biosvidsw.get_info)(scp->adp, mode, &info))
			return ENODEV;
		if (info.vi_flags & V_INFO_GRAPHICS)
			goto vesa_graphics;
		else
			goto vesa_text;

	/* generic text modes */
	case SW_TEXT_132x25: case SW_TEXT_132x30:
	case SW_TEXT_132x43: case SW_TEXT_132x50:
	case SW_TEXT_132x60:
		adp = get_adapter(scp);
		if (!(adp->va_flags & V_ADP_MODECHANGE))
			return ENODEV;
		return sc_set_text_mode(scp, tp, cmd & 0xff, 0, 0, 0);

	/* text modes */
	case SW_VESA_C80x60:
	case SW_VESA_C132x25:
	case SW_VESA_C132x43:
	case SW_VESA_C132x50:
	case SW_VESA_C132x60:
		adp = get_adapter(scp);
		if (!(adp->va_flags & V_ADP_MODECHANGE))
			return ENODEV;
		mode = (cmd & 0xff) + M_VESA_BASE;
vesa_text:
		return sc_set_text_mode(scp, tp, mode, 0, 0, 0);

	/* graphics modes */
	case SW_VESA_32K_320: 	case SW_VESA_64K_320: 
	case SW_VESA_FULL_320:

	case SW_VESA_CG640x400:

	case SW_VESA_CG640x480:
	case SW_VESA_32K_640:	case SW_VESA_64K_640:
	case SW_VESA_FULL_640:

	case SW_VESA_800x600:	case SW_VESA_CG800x600:
	case SW_VESA_32K_800:	case SW_VESA_64K_800:
	case SW_VESA_FULL_800:

	case SW_VESA_1024x768:	case SW_VESA_CG1024x768:
	case SW_VESA_32K_1024:	case SW_VESA_64K_1024:
	case SW_VESA_FULL_1024:

	case SW_VESA_1280x1024:	case SW_VESA_CG1280x1024:
	case SW_VESA_32K_1280:	case SW_VESA_64K_1280:
	case SW_VESA_FULL_1280:
		adp = get_adapter(scp);
		if (!(adp->va_flags & V_ADP_MODECHANGE))
			return ENODEV;
		mode = (cmd & 0xff) + M_VESA_BASE;
vesa_graphics:
		return sc_set_graphics_mode(scp, tp, mode);
	}

	if (prev_user_ioctl)
		return (*prev_user_ioctl)(dev, cmd, data, flag, p);
	else
		return ENOIOCTL;
}

int
vesa_load_ioctl(void)
{
	if (prev_user_ioctl)
		return EBUSY;
	prev_user_ioctl = sc_user_ioctl;
	sc_user_ioctl = vesa_ioctl;
	return 0;
}

int
vesa_unload_ioctl(void)
{
	if (sc_user_ioctl != vesa_ioctl)
		return EBUSY;
	sc_user_ioctl = prev_user_ioctl;
	prev_user_ioctl = NULL;
	return 0;
}

#endif /* (NSC > 0 && VESA && VM86) || VESA_MODULE */
