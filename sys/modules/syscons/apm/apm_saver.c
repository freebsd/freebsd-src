/*-
 * Copyright (c) 1995-1998 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fb/vgareg.h>

#include <i386/isa/isa.h>

#include <sys/select.h>
#include <machine/apm_bios.h>
#include <i386/apm/apm.h>

#include <saver.h>

extern int apm_display __P((int newstate));                                     

extern struct apm_softc apm_softc;

static int blanked=0;

static int
apm_saver(video_adapter_t *adp, int blank)
{
	struct apm_softc *sc = &apm_softc;                                      

	if (!sc->initialized || !sc->active)
		return 0;

	if (blank==blanked)
		return 0;

	blanked=blank;

	apm_display(!blanked);

	return 0;
}

static int
apm_init(video_adapter_t *adp)
{
	return 0;
}

static int
apm_term(video_adapter_t *adp)
{
	return 0;
}

static scrn_saver_t apm_module = {
	"apm_saver", apm_init, apm_term, apm_saver, NULL,
};

SAVER_MODULE(apm_saver, apm_module);
