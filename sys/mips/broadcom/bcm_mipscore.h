/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 *  $FreeBSD$
 */

#ifndef _BHND_CORES_MIPS_MIPSCOREVAR_H_
#define _BHND_CORES_MIPS_MIPSCOREVAR_H_

#define	MIPSCORE_MAX_RSPEC 2

struct mipscore_softc {
	device_t		dev;	/* CPU device */
	uint32_t		devid;
	struct resource_spec	rspec[MIPSCORE_MAX_RSPEC];
	struct bhnd_resource	*res[MIPSCORE_MAX_RSPEC];
};

struct mipscore_regs {
        uint32_t	corecontrol;
        uint32_t	exceptionbase;
        uint32_t	PAD1[1];	/* unmapped address */
        uint32_t	biststatus;
        uint32_t	intstatus;
        uint32_t	intmask[6];
        uint32_t	nmimask;
        uint32_t	PAD2[4];	/* unmapped addresses */
        uint32_t	gpioselect;
        uint32_t	gpiooutput;
        uint32_t	gpioenable;
        uint32_t	PAD3[101];	/* unmapped addresses */
        uint32_t	clkcontrolstatus;
};

#endif /* _BHND_CORES_MIPS_MIPSCOREVAR_H_ */
