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
 *	$Id: ppb_1284.c,v 1.7 1999/01/10 12:04:54 nsouch Exp $
 *
 */

/*
 * General purpose routines for the IEEE1284-1994 Standard
 */

#include "opt_ppb_1284.h"

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
static int
do_1284_wait(struct ppb_device *dev, char mask, char status)
{
	return (ppb_poll_device(dev, 4, mask, status, PPB_NOINTR | PPB_POLL));
}

static int
do_peripheral_wait(struct ppb_device *dev, char mask, char status)
{
	return (ppb_poll_device(dev, 100, mask, status, PPB_NOINTR | PPB_POLL));
}

#define nibble2char(s) (((s & ~nACK) >> 3) | (~s & nBUSY) >> 4)

/*
 * ppb_1284_reset_error()
 *
 * Unconditionaly reset the error field
 */
static int
ppb_1284_reset_error(struct ppb_device *dev, int state)
{
	dev->ppb->error = PPB_NO_ERROR;
	dev->ppb->state = state;

	return (0);
}

/*
 * ppb_1284_get_state()
 *
 * Get IEEE1284 state
 */
static int
ppb_1284_get_state(struct ppb_device *dev)
{
        return (dev->ppb->state);
}

/*
 * ppb_1284_set_state()
 *
 * Change IEEE1284 state if no error occured
 */
static int
ppb_1284_set_state(struct ppb_device *dev, int state)
{
	/* call ppb_1284_reset_error() if you absolutly want to change
	 * the state from PPB_ERROR to another */
	if ((dev->ppb->state != PPB_ERROR) &&
			(dev->ppb->error == PPB_NO_ERROR)) {
		dev->ppb->state = state;
		dev->ppb->error = PPB_NO_ERROR;
	}

	return (0);
}

static int
ppb_1284_set_error(struct ppb_device *dev, int error, int event)
{
	/* do not accumulate errors */
	if ((dev->ppb->error == PPB_NO_ERROR) &&
			(dev->ppb->state != PPB_ERROR)) {
		dev->ppb->error = error;
		dev->ppb->state = PPB_ERROR;
	}

#ifdef DEBUG_1284
	printf("ppb1284: error=%d status=0x%x event=%d\n", error,
		ppb_rstr(dev) & 0xff, event);
#endif

	return (0);
}

/*
 * ppb_request_mode()
 *
 * Converts mode+options into ext. value
 */
static int
ppb_request_mode(int mode, int options)
{
	int request_mode = 0;

	if (options & PPB_EXTENSIBILITY_LINK) {
		request_mode = EXT_LINK_1284_NORMAL;

	} else {
		switch (mode) {
		case PPB_NIBBLE:
			request_mode = (options & PPB_REQUEST_ID) ?
					NIBBLE_1284_REQUEST_ID :
					NIBBLE_1284_NORMAL;
			break;
		case PPB_PS2:
			request_mode = (options & PPB_REQUEST_ID) ?
					BYTE_1284_REQUEST_ID :
					BYTE_1284_NORMAL;
			break;
		case PPB_ECP:
			if (options & PPB_USE_RLE)
				request_mode = (options & PPB_REQUEST_ID) ?
					ECP_1284_RLE_REQUEST_ID :
					ECP_1284_RLE;
			else
				request_mode = (options & PPB_REQUEST_ID) ?
					ECP_1284_REQUEST_ID :
					ECP_1284_NORMAL;
			break;
		case PPB_EPP:
			request_mode = EPP_1284_NORMAL;
			break;
		default:
			panic("%s: unsupported mode %d\n", __FUNCTION__, mode);
		}
	}

	return (request_mode);
}

/*
 * ppb_peripheral_negociate()
 *
 * Negociate the peripheral side
 */
int
ppb_peripheral_negociate(struct ppb_device *dev, int mode, int options)
{
	int spin, request_mode, error = 0;
	char r;

	ppb_set_mode(dev, PPB_COMPATIBLE);
	ppb_1284_set_state(dev, PPB_PERIPHERAL_NEGOCIATION);

	/* compute ext. value */
	request_mode = ppb_request_mode(mode, options);

	/* wait host */
	spin = 10;
	while (spin-- && (ppb_rstr(dev) & nBUSY))
		DELAY(1);

	/* check termination */
	if (!(ppb_rstr(dev) & SELECT) || !spin) {
		error = ENODEV;
		goto error;
	}

	/* Event 4 - read ext. value */
	r = ppb_rdtr(dev);

	/* nibble mode is not supported */
	if ((r == (char)request_mode) ||
			(r == NIBBLE_1284_NORMAL)) {

		/* Event 5 - restore direction bit, no data avail */
		ppb_wctr(dev, (STROBE | nINIT) & ~(SELECTIN));
		DELAY(1);

		/* Event 6 */
		ppb_wctr(dev, (nINIT) & ~(SELECTIN | STROBE));

		if (r == NIBBLE_1284_NORMAL) {
#ifdef DEBUG_1284
			printf("R");
#endif
			ppb_1284_set_error(dev, PPB_MODE_UNSUPPORTED, 4);
			error = EINVAL;
			goto error;
		} else {
			ppb_1284_set_state(dev, PPB_PERIPHERAL_IDLE);
			switch (r) {
			case BYTE_1284_NORMAL:
				ppb_set_mode(dev, PPB_BYTE);
				break;
			default:
				break;
			}
#ifdef DEBUG_1284
			printf("A");
#endif
			/* negociation succeeds */
		}
	} else {
		/* Event 5 - mode not supported */
		ppb_wctr(dev, SELECTIN);
		DELAY(1);

		/* Event 6 */
		ppb_wctr(dev, (SELECTIN) & ~(STROBE | nINIT));
		ppb_1284_set_error(dev, PPB_MODE_UNSUPPORTED, 4);

#ifdef DEBUG_1284
		printf("r");
#endif
		error = EINVAL;
		goto error;
	}

	return (0);

error:
	ppb_peripheral_terminate(dev, PPB_WAIT);
	return (error);
}

/*
 * ppb_peripheral_terminate()
 *
 * Terminate peripheral transfer side
 *
 * Always return 0 in compatible mode
 */
int
ppb_peripheral_terminate(struct ppb_device *dev, int how)
{
	int error = 0;

#ifdef DEBUG_1284
	printf("t");
#endif

	ppb_1284_set_state(dev, PPB_PERIPHERAL_TERMINATION);

	/* Event 22 - wait up to host response time (1s) */
	if ((error = do_peripheral_wait(dev, SELECT | nBUSY, 0))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 22);
		goto error;
	}

	/* Event 24 */
        ppb_wctr(dev, (nINIT | STROBE) & ~(AUTOFEED | SELECTIN));

	/* Event 25 - wait up to host response time (1s) */
	if ((error = do_peripheral_wait(dev, nBUSY, nBUSY))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 25);
		goto error;
	}

	/* Event 26 */
        ppb_wctr(dev, (SELECTIN | nINIT | STROBE) & ~(AUTOFEED));
	DELAY(1);
	/* Event 27 */
        ppb_wctr(dev, (SELECTIN | nINIT) & ~(STROBE | AUTOFEED));

	/* Event 28 - wait up to host response time (1s) */
	if ((error = do_peripheral_wait(dev, nBUSY, 0))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 28);
		goto error;
	}
	
error:
	ppb_set_mode(dev, PPB_COMPATIBLE);
	ppb_1284_set_state(dev, PPB_FORWARD_IDLE);

	return (0);
}

/*
 * byte_peripheral_outbyte()
 *
 * Write 1 byte in BYTE mode
 */
static int
byte_peripheral_outbyte(struct ppb_device *dev, char *buffer, int last)
{
	int error = 0;

	/* Event 7 */
	if ((error = do_1284_wait(dev, nBUSY, nBUSY))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 7);
		goto error;
	}

	/* check termination */
	if (!(ppb_rstr(dev) & SELECT)) {
		ppb_peripheral_terminate(dev, PPB_WAIT);
		goto error;
	}

	/* Event 15 - put byte on data lines */
#ifdef DEBUG_1284
	printf("B");
#endif
	ppb_wdtr(dev, *buffer);

	/* Event 9 */
	ppb_wctr(dev, (AUTOFEED | STROBE) & ~(nINIT | SELECTIN));

	/* Event 10 - wait data read */
	if ((error = do_peripheral_wait(dev, nBUSY, 0))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 16);
		goto error;
	}

	/* Event 11 */
	if (!last) {
		ppb_wctr(dev, (AUTOFEED) & ~(nINIT | STROBE | SELECTIN));
	} else {
		ppb_wctr(dev, (nINIT) & ~(STROBE | SELECTIN | AUTOFEED));
	}

#if 0
	/* Event 16 - wait strobe */
	if ((error = do_peripheral_wait(dev, nACK | nBUSY, 0))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 16);
		goto error;
	}
#endif

	/* check termination */
	if (!(ppb_rstr(dev) & SELECT)) {
		ppb_peripheral_terminate(dev, PPB_WAIT);
		goto error;
	}

error:
	return (error);
}

/*
 * byte_peripheral_write()
 *
 * Write n bytes in BYTE mode
 */
int
byte_peripheral_write(struct ppb_device *dev, char *buffer, int len, int *sent)
{
	int error = 0, i;
	char r;

	ppb_1284_set_state(dev, PPB_PERIPHERAL_TRANSFER);

	/* wait forever, the remote host is master and should initiate
	 * termination
	 */
	for (i=0; i<len; i++) {
		/* force remote nFAULT low to release the remote waiting
		 * process, if any
		 */
		r = ppb_rctr(dev);
		ppb_wctr(dev, r & ~nINIT);

#ifdef DEBUG_1284
		printf("y");
#endif
		/* Event 7 */
		error = ppb_poll_device(dev, PPB_FOREVER, nBUSY, nBUSY,
					PPB_INTR);

		if (error && error != EWOULDBLOCK)
			goto error;

#ifdef DEBUG_1284
		printf("b");
#endif
		if ((error = byte_peripheral_outbyte(dev, buffer+i, (i == len-1))))
			goto error;
	}
error:
	if (!error)
		ppb_1284_set_state(dev, PPB_PERIPHERAL_IDLE);

	*sent = i;
	return (error);
}

/*
 * byte_1284_inbyte()
 *
 * Read 1 byte in BYTE mode
 */
int
byte_1284_inbyte(struct ppb_device *dev, char *buffer)
{
	int error = 0;

	/* Event 7 - ready to take data (nAUTO low) */
	ppb_wctr(dev, (PCD | nINIT | AUTOFEED) & ~(STROBE | SELECTIN));

	/* Event 9 - peripheral set nAck low */
	if ((error = do_1284_wait(dev, nACK, 0))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 9);
		goto error;
	}

	/* read the byte */
	*buffer = ppb_rdtr(dev);

	/* Event 10 - data received, can't accept more */
	ppb_wctr(dev, (nINIT) & ~(AUTOFEED | STROBE | SELECTIN));

	/* Event 11 - peripheral ack */
	if ((error = do_1284_wait(dev, nACK, nACK))) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 11);
		goto error;
	}

	/* Event 16 - strobe */
	ppb_wctr(dev, (nINIT | STROBE) & ~(AUTOFEED | SELECTIN));
	DELAY(3);
	ppb_wctr(dev, (nINIT) & ~(AUTOFEED | STROBE | SELECTIN));

error:
	return (error);
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

		/* Event 7 - ready to take data (nAUTO low) */
		ppb_wctr(dev, (nINIT | AUTOFEED) & ~(STROBE | SELECTIN));

		/* Event 8 - peripheral writes the first nibble */

		/* Event 9 - peripheral set nAck low */
		if ((error = do_1284_wait(dev, nACK, 0))) {
			ppb_1284_set_error(dev, PPB_TIMEOUT, 9);
			goto error;
		}

		/* read nibble */
		nibble[i] = ppb_rstr(dev);

		/* Event 10 - ack, nibble received */
		ppb_wctr(dev, nINIT & ~(AUTOFEED | STROBE | SELECTIN));

		/* Event 11 - wait ack from peripherial */
		if ((error = do_1284_wait(dev, nACK, nACK))) {
			ppb_1284_set_error(dev, PPB_TIMEOUT, 11);
			goto error;
		}
	}

	*buffer = ((nibble2char(nibble[1]) << 4) & 0xf0) |
				(nibble2char(nibble[0]) & 0x0f);

error:
	return (error);
}

/*
 * spp_1284_read()
 *
 * Read in IEEE1284 NIBBLE/BYTE mode
 */
int
spp_1284_read(struct ppb_device *dev, int mode, char *buffer, int max, int *read)
{
	int error = 0, len = 0;
	int terminate_after_transfer = 1;
	int state;

	*read = len = 0;

	state = ppb_1284_get_state(dev);

	switch (state) {
	case PPB_FORWARD_IDLE:
		if ((error = ppb_1284_negociate(dev, mode, 0)))
			return (error);
		break;

	case PPB_REVERSE_IDLE:
		terminate_after_transfer = 0;
		break;
		
	default:
		ppb_1284_terminate(dev);
		if ((error = ppb_1284_negociate(dev, mode, 0)))
			return (error);
		break;
	}

	while ((len < max) && !(ppb_rstr(dev) & (nFAULT))) {

		ppb_1284_set_state(dev, PPB_REVERSE_TRANSFER);

#ifdef DEBUG_1284
		printf("B");
#endif

		switch (mode) {
		case PPB_NIBBLE:
			/* read a byte, error means no more data */
			if (nibble_1284_inbyte(dev, buffer+len))
				goto end_while;
			break;
		case PPB_BYTE:
			if (byte_1284_inbyte(dev, buffer+len))
				goto end_while;
			break;
		default:
			error = EINVAL;
			goto end_while;
		}
		len ++;
	}
end_while:

	if (!error)
		ppb_1284_set_state(dev, PPB_REVERSE_IDLE);

	*read = len;

	if (terminate_after_transfer || error)
		ppb_1284_terminate(dev);

	return (error);
}

/*
 * ppb_1284_read_id()
 *
 */
int
ppb_1284_read_id(struct ppb_device *dev, int mode, char *buffer,
		int max, int *read)
{
	int error = 0;

	/* fill the buffer with 0s */
	bzero(buffer, max);

	switch (mode) {
	case PPB_NIBBLE:
	case PPB_ECP:
		if ((error = ppb_1284_negociate(dev, PPB_NIBBLE, PPB_REQUEST_ID)))
			return (error);
		error = spp_1284_read(dev, PPB_NIBBLE, buffer, max, read);
		break;
	case PPB_BYTE:
		if ((error = ppb_1284_negociate(dev, PPB_BYTE, PPB_REQUEST_ID)))
			return (error);
		error = spp_1284_read(dev, PPB_BYTE, buffer, max, read);
		break;
	default:
		panic("%s: unsupported mode %d\n", __FUNCTION__, mode);
	}

	ppb_1284_terminate(dev);
	return (error);
}

/*
 * ppb_1284_read()
 *
 * IEEE1284 read
 */
int
ppb_1284_read(struct ppb_device *dev, int mode, char *buffer,
		int max, int *read)
{
	int error = 0;

	switch (mode) {
	case PPB_NIBBLE:
	case PPB_BYTE:
		error = spp_1284_read(dev, mode, buffer, max, read);
		break;
	default:
		return (EINVAL);
	}

	return (error);
}

/*
 * ppb_1284_negociate()
 *
 * IEEE1284 negociation phase
 *
 * Normal nibble mode or request device id mode (see ppb_1284.h)
 *
 * After negociation, nFAULT is low if data is available
 */
int
ppb_1284_negociate(struct ppb_device *dev, int mode, int options)
{
	int error;
	int request_mode;

#ifdef DEBUG_1284
	printf("n");
#endif

	if (ppb_1284_get_state(dev) >= PPB_PERIPHERAL_NEGOCIATION)
		ppb_peripheral_terminate(dev, PPB_WAIT);

	if (ppb_1284_get_state(dev) != PPB_FORWARD_IDLE)
		ppb_1284_terminate(dev);

#ifdef DEBUG_1284
	printf("%d", mode);
#endif

	/* ensure the host is in compatible mode */
	ppb_set_mode(dev, PPB_COMPATIBLE);

	/* reset error to catch the actual negociation error */
	ppb_1284_reset_error(dev, PPB_FORWARD_IDLE);

	/* calculate ext. value */
	request_mode = ppb_request_mode(mode, options);

	/* default state */
	ppb_wctr(dev, (nINIT | SELECTIN) & ~(STROBE | AUTOFEED));
	DELAY(1);

	/* enter negociation phase */
	ppb_1284_set_state(dev, PPB_NEGOCIATION);

	/* Event 0 - put the exten. value on the data lines */
	ppb_wdtr(dev, request_mode);

#ifdef PERIPH_1284
	/* request remote host attention */
        ppb_wctr(dev, (nINIT | STROBE) & ~(AUTOFEED | SELECTIN));
        DELAY(1);
        ppb_wctr(dev, (nINIT) & ~(STROBE | AUTOFEED | SELECTIN));
#else
	DELAY(1);

#endif /* !PERIPH_1284 */

	/* Event 1 - enter IEEE1284 mode */
	ppb_wctr(dev, (nINIT | AUTOFEED) & ~(STROBE | SELECTIN));

#ifdef PERIPH_1284
	/* ignore the PError line, wait a bit more, remote host's 
	 * interrupts don't respond fast enough */
	if (ppb_poll_device(dev, 40, nACK | SELECT | nFAULT,
				SELECT | nFAULT, PPB_NOINTR | PPB_POLL)) {
                ppb_1284_set_error(dev, PPB_NOT_IEEE1284, 2);
                error = ENODEV;
                goto error;
        }
#else
	/* Event 2 - trying IEEE1284 dialog */
	if (do_1284_wait(dev, nACK | PERROR | SELECT | nFAULT,
			PERROR  | SELECT | nFAULT)) {
		ppb_1284_set_error(dev, PPB_NOT_IEEE1284, 2);
		error = ENODEV;
		goto error;
	}
#endif /* !PERIPH_1284 */

	/* Event 3 - latch the ext. value to the peripheral */
	ppb_wctr(dev, (nINIT | STROBE | AUTOFEED) & ~SELECTIN);
	DELAY(1);

	/* Event 4 - IEEE1284 device recognized */
	ppb_wctr(dev, nINIT & ~(SELECTIN | AUTOFEED | STROBE));

	/* Event 6 - waiting for status lines */
	if (do_1284_wait(dev, nACK, nACK)) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 6);
		error = EBUSY;
		goto error;
	}

	/* Event 7 - quering result consider nACK not to misunderstand
	 * a remote computer terminate sequence */
	if (options & PPB_EXTENSIBILITY_LINK) {

		/* XXX not fully supported yet */
		ppb_1284_terminate(dev);
		return (0);

	}
	if (request_mode == NIBBLE_1284_NORMAL) {
		if (do_1284_wait(dev, nACK | SELECT, nACK)) {
			ppb_1284_set_error(dev, PPB_MODE_UNSUPPORTED, 7);
			error = ENODEV;
			goto error;
		}
	} else {
		if (do_1284_wait(dev, nACK | SELECT, SELECT | nACK)) {
			ppb_1284_set_error(dev, PPB_MODE_UNSUPPORTED, 7);
			error = ENODEV;
			goto error;
		}
	}

	switch (mode) {
	case PPB_NIBBLE:
	case PPB_PS2:
		/* enter reverse idle phase */
		ppb_1284_set_state(dev, PPB_REVERSE_IDLE);
		break;
	case PPB_ECP:
		/* negociation ok, now setup the communication */
		ppb_1284_set_state(dev, PPB_SETUP);
		ppb_wctr(dev, (nINIT | AUTOFEED) & ~(SELECTIN | STROBE));

#ifdef PERIPH_1284
		/* ignore PError line */
		if (do_1284_wait(dev, nACK | SELECT | nBUSY,
                                        nACK | SELECT | nBUSY)) {
                        ppb_1284_set_error(dev, PPB_TIMEOUT, 30);
                        error = ENODEV;
                        goto error;
                }
#else
		if (do_1284_wait(dev, nACK | SELECT | PERROR | nBUSY,
					nACK | SELECT | PERROR | nBUSY)) {
			ppb_1284_set_error(dev, PPB_TIMEOUT, 30);
			error = ENODEV;
			goto error;
		}
#endif /* !PERIPH_1284 */

		/* ok, the host enters the ForwardIdle state */
		ppb_1284_set_state(dev, PPB_ECP_FORWARD_IDLE);
		break;
	case PPB_EPP:
		ppb_1284_set_state(dev, PPB_EPP_IDLE);
		break;

	default:
		panic("%s: unknown mode (%d)!", __FUNCTION__, mode);
	}
	ppb_set_mode(dev, mode);

	return (0);

error:
	ppb_1284_terminate(dev);

	return (error);
}

/*
 * ppb_1284_terminate()
 *
 * IEEE1284 termination phase, return code should ignored since the host
 * is _always_ in compatible mode after ppb_1284_terminate()
 */
int
ppb_1284_terminate(struct ppb_device *dev)
{

#ifdef DEBUG_1284
	printf("T");
#endif

	/* do not reset error here to keep the error that
	 * may occured before the ppb_1284_terminate() call */
	ppb_1284_set_state(dev, PPB_TERMINATION);

#ifdef PERIPH_1284
	/* request remote host attention */
        ppb_wctr(dev, (nINIT | STROBE | SELECTIN) & ~(AUTOFEED));
        DELAY(1);
#endif /* PERIPH_1284 */

	/* Event 22 - set nSelectin low and nAutoFeed high */
	ppb_wctr(dev, (nINIT | SELECTIN) & ~(STROBE | AUTOFEED));

	/* Event 24 - waiting for peripheral, Xflag ignored */
	if (do_1284_wait(dev, nACK | nBUSY | nFAULT, nFAULT)) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 24);
		goto error;
	}

	/* Event 25 - set nAutoFd low */
	ppb_wctr(dev, (nINIT | SELECTIN | AUTOFEED) & ~STROBE);

	/* Event 26 - compatible mode status is set */

	/* Event 27 - peripheral set nAck high */
	if (do_1284_wait(dev, nACK, nACK)) {
		ppb_1284_set_error(dev, PPB_TIMEOUT, 27);
	}

	/* Event 28 - end termination, return to idle phase */
	ppb_wctr(dev, (nINIT | SELECTIN) & ~(STROBE | AUTOFEED));

error:
	/* return to compatible mode */
	ppb_set_mode(dev, PPB_COMPATIBLE);
	ppb_1284_set_state(dev, PPB_FORWARD_IDLE);

	return (0);
}
