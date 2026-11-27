/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1999 Seigo Tanimura
 * Copyright (c) 2003 Mathew Kanner
 * Copyright (c) 2003-2006 Yuriy Tsibizov <yuriy.tsibizov@gfk.ru>
 * All rights reserved
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
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/sbuf.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/pcm/sound.h>

#include <dev/sound/midi/midi.h>
#include <dev/sound/midi/mpu401.h>

#include <dev/sound/pci/emuxkireg.h>
#include <dev/sound/pci/emu10kx.h>


static unsigned char
emu_mread(void *devinfo, int reg)
{
	struct emu_midiinfo *sc = devinfo;
	unsigned int d;

	d = 0;
	if (sc->is_emu10k1)
		d = emu_rd(sc->card, 0x18 + reg, 1);
	else
		d = emu_rdptr(sc->card, 0, sc->port + reg);

	return (d);
}

static void
emu_mwrite(void *devinfo, int reg, uint8_t b)
{
	struct emu_midiinfo *sc = devinfo;

	if (sc->is_emu10k1)
		emu_wr(sc->card, 0x18 + reg, b, 1);
	else
		emu_wrptr(sc->card, 0, sc->port + reg, b);
}

const struct mpu401_ops emu_mpu_ops = {
	emu_mread,
	emu_mwrite,
};

uint32_t
emu_midi_card_intr(void *p, uint32_t intr_status)
{
	struct emu_midiinfo *sc = p;

	if (sc->dev != NULL)
		mpu401_intr(device_get_softc(sc->dev));
	else {
		/*
		 * We should read MIDI event to unlock card after
		 * interrupt. XXX - check, why this happens.
		 */
		(void)emu_mread(sc, 0);
	}

	return (intr_status); /* Acknowledge everything */
}

#if 0
/*
 * Set up one MIDI port: enable its interrupt on the card and attach a midi
 * child for it.
 */
void
emu_midi_attach(device_t dev, struct emu_midiinfo *mi, device_t *devp)
{
	uint32_t inte_val, ipr_val;

	if (mi->is_emu10k1) {
		/* SB Live! - only one MIDI device here */
		inte_val = EMU_INTE_MIDIRXENABLE;
		ipr_val = EMU_IPR_MIDITRANSBUFE | EMU_IPR_MIDIRECVBUFE;
	} else if (mi->port == EMU_A_MUDATA1) {
		/* EXTERNAL MIDI (AudigyDrive) */
		inte_val = EMU_INTE_MIDIRXENABLE;
		ipr_val = EMU_IPR_MIDITRANSBUFE | EMU_IPR_MIDIRECVBUFE;
	} else {
		/* MIDI hw config port 2 */
		inte_val = EMU_INTE_A_MIDIRXENABLE2;
		ipr_val = EMU_IPR_A_MIDITRANSBUFE2 | EMU_IPR_A_MIDIRECBUFE2;
	}

	mi->ivars.ops = &emu_mpu_ops;
	mi->ivars.devinfo = mi;
	mi->ivars.desc = "EMU10Kx MIDI Interface";

	*devp = device_add_child(dev, "midi", DEVICE_UNIT_ANY);
	if (*devp == NULL)
		return;
	device_set_ivars(*devp, &mi->ivars);
	mi->dev = *devp;
	mi->ihandle = emu_intr_register(mi->card, inte_val, ipr_val,
	    &emu_midi_card_intr, mi);

	/*
	 * XXX I don not know how to check for Live!Drive / AudigyDrive
	 * presence.  Let us hope that IR enabling code will not harm if it is
	 * not present.
	 */
	if (mi->is_emu10k1 || mi->port == EMU_A_MUDATA1)
		emu_enable_ir(mi->card);
}

void
emu_midi_detach(struct emu_midiinfo *mi)
{
	if (mi == NULL)
		return;
	emu_intr_unregister(mi->card, mi->ihandle);
	mi->dev = NULL;
	free(mi, M_DEVBUF);
}
#endif
