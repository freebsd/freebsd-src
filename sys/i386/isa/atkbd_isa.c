/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 */

#include "atkbd.h"
#include "opt_kbd.h"

#if NATKBD > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/kbd/kbdreg.h>
#include <dev/kbd/atkbdreg.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

static int		atkbdprobe(struct isa_device *dev);
static int		atkbdattach(struct isa_device *dev);
static ointhand2_t	atkbd_isa_intr;

struct isa_driver atkbddriver = {
	atkbdprobe,
	atkbdattach,
	ATKBD_DRIVER_NAME,
	0,
};

static int
atkbdprobe(struct isa_device *dev)
{
	return ((atkbd_probe_unit(dev->id_unit, dev->id_iobase,
				  dev->id_irq, dev->id_flags)) ? 0 : -1);
}

static int
atkbdattach(struct isa_device *dev)
{
	atkbd_softc_t *sc;

	sc = atkbd_get_softc(dev->id_unit);
	if (sc == NULL)
		return 0;

	dev->id_ointr = atkbd_isa_intr;
	return ((atkbd_attach_unit(dev->id_unit, sc, dev->id_iobase,
				   dev->id_irq, dev->id_flags)) ? 0 : 1);
}

static void
atkbd_isa_intr(int unit)
{
	keyboard_t *kbd;

	kbd = atkbd_get_softc(unit)->kbd;
	(*kbdsw[kbd->kb_index]->intr)(kbd, NULL);
}

#endif /* NATKBD > 0 */
