/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *	$Id: ppb_1284.c,v 1.4 1998/08/03 19:14:31 msmith Exp $
 *
 */

#include "opt_debug_1284.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/clock.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_1284.h>

/*
 * do_1284_wait()
 *
 * Wait for the peripherial up to 40ms
 */
int
do_1284_wait(struct ppb_device *dev, char mask, char status)
{
	int i;
	char r;

	/* try up to 5ms */
	for (i = 0; i < 20; i++) {
		r = ppb_rstr(dev);
		DELAY(25);
		if ((r & mask) == status)
			return (0);
	}

	return (ppb_poll_device(dev, 4, mask, status, PPB_NOINTR));
}

#define nibble2char(s) (((s & ~nACK) >> 3) | (~s & nBUSY) >> 4)

/*
 * byte_1284_inbyte()
 *
 * Read 1 byte in BYTE mode
 */
int
byte_1284_inbyte(struct ppb_device *dev, char *buffer)
{
	int error;

	/* notify the peripherial to put data on the lines */
	ppb_wctr(dev, PCD |  AUTOFEED | nSTROBE | nINIT | nSELECTIN);

	/* wait for valid byte signal */
	if ((error = do_1284_wait(dev, nACK, 0)))
		return (error);

	/* fetch data */
	*buffer = ppb_rdtr(dev);

	/* indicate that data has been received, not ready for another */
	ppb_wctr(dev, PCD | nAUTOFEED | nSTROBE | nINIT | nSELECTIN);

	/* wait peripherial's acknowledgement */
	if ((error = do_1284_wait(dev, nACK, nACK)))
		return (error);

	/* acknowledge the peripherial */
	ppb_wctr(dev, PCD | nAUTOFEED |  STROBE | nINIT | nSELECTIN);

	return (0);
}

/*
 * nibble_1284_inbyte()
 *
 * Read 1 byte in NIBBLE mode
 */
int
nibble_1284_inbyte(struct ppb_device *dev, char *buffer)
{
	char nibble[2];
	int i, error;

	for (i = 0; i < 2; i++) {
		/* ready to take data (nAUTO low) */
		ppb_wctr(dev, AUTOFEED & ~(STROBE | SELECTIN));

		if ((error = do_1284_wait(dev, nACK, 0)))
			return (error);

		/* read nibble */
		nibble[i] = ppb_rstr(dev);

		/* ack, not ready for another nibble */
		ppb_wctr(dev, 0 & ~(AUTOFEED | STROBE | SELECTIN));

		/* wait ack from peripherial */
		if ((error = do_1284_wait(dev, nACK, nACK)))
			return (error);
	}

	*buffer = ((nibble2char(nibble[1]) << 4) & 0xf0) |
				(nibble2char(nibble[0]) & 0x0f);

	return (0);
}

/*
 * nibble_1284_sync()
 */
void
nibble_1284_sync(struct ppb_device *dev)
{
	char ctr;

	ctr = ppb_rctr(dev);

	ppb_wctr(dev, (ctr & ~AUTOFEED) | SELECTIN);
	if (do_1284_wait(dev, nACK, 0))
		return;

	ppb_wctr(dev, ctr | AUTOFEED);
	do_1284_wait(dev, nACK, nACK);

	ppb_wctr(dev, (ctr & ~AUTOFEED) | SELECTIN);

	return;
}

/*
 * ppb_1284_negociate()
 *
 * Normal nibble mode or request device id mode (see ppb_1284.h)
 */
int
ppb_1284_negociate(struct ppb_device *dev, int mode)
{
	int error;
	int phase = 0;

	ppb_wctr(dev, (nINIT | SELECTIN) & ~(STROBE | AUTOFEED));
	DELAY(1);

	ppb_wdtr(dev, mode);
	DELAY(1);

	ppb_wctr(dev, (nINIT | AUTOFEED) & ~(STROBE | SELECTIN));

	if ((error = do_1284_wait(dev, nACK | PERROR | SELECT | nFAULT,
			PERROR  | SELECT | nFAULT)))
		goto error;

	phase = 1;

	ppb_wctr(dev, (nINIT | STROBE | AUTOFEED) & ~SELECTIN);
	DELAY(5);

	ppb_wctr(dev, nINIT & ~(SELECTIN | AUTOFEED | STROBE));

#if 0	/* not respected by most devices */
	if ((error = do_1284_wait(dev, nACK, nACK)))
		goto error;

	if (mode == 0)
		if ((error = do_1284_wait(dev, SELECT, 0)))
			goto error;
	else
		if ((error = do_1284_wait(dev, SELECT, SELECT)))
			goto error;
#endif

	return (0);

error:
	if (bootverbose)
		printf("%s: status=0x%x %d\n", __FUNCTION__, ppb_rstr(dev), phase);

	return (error);
}

int
ppb_1284_terminate(struct ppb_device *dev, int how)
{
	int error;

	switch (how) {
	case VALID_STATE:

		ppb_wctr(dev, SELECTIN & ~(STROBE | AUTOFEED));

		if ((error = do_1284_wait(dev, nACK | nBUSY | nFAULT, nFAULT)))
			return (error);

		ppb_wctr(dev, (SELECTIN | AUTOFEED) & ~STROBE);

		if ((error = do_1284_wait(dev, nACK, nACK)))
			return (error);

		ppb_wctr(dev, SELECTIN & ~(STROBE | AUTOFEED));
		break;

	default:
		return (EINVAL);
	}

	return (0);
}
