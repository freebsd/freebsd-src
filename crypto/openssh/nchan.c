/*
 * Copyright (c) 1999 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#include "includes.h"
RCSID("$Id: nchan.c,v 1.10 2000/01/10 10:15:28 markus Exp $");

#include "ssh.h"

#include "buffer.h"
#include "packet.h"
#include "channels.h"
#include "nchan.h"

static void chan_send_ieof(Channel *c);
static void chan_send_oclose(Channel *c);
static void chan_shutdown_write(Channel *c);
static void chan_shutdown_read(Channel *c);
static void chan_delete_if_full_closed(Channel *c);

/*
 * EVENTS update channel input/output states execute ACTIONS
 */

/* events concerning the INPUT from socket for channel (istate) */
void
chan_rcvd_oclose(Channel *c)
{
	switch (c->istate) {
	case CHAN_INPUT_WAIT_OCLOSE:
		debug("channel %d: INPUT_WAIT_OCLOSE -> INPUT_CLOSED [rcvd OCLOSE]", c->self);
		c->istate = CHAN_INPUT_CLOSED;
		break;
	case CHAN_INPUT_OPEN:
		debug("channel %d: INPUT_OPEN -> INPUT_CLOSED [rvcd OCLOSE, send IEOF]", c->self);
		chan_shutdown_read(c);
		chan_send_ieof(c);
		c->istate = CHAN_INPUT_CLOSED;
		break;
	case CHAN_INPUT_WAIT_DRAIN:
		/* both local read_failed and remote write_failed  */
		log("channel %d: INPUT_WAIT_DRAIN -> INPUT_CLOSED [rvcd OCLOSE, send IEOF]", c->self);
		debug("channel %d: INPUT_WAIT_DRAIN -> INPUT_CLOSED [rvcd OCLOSE, send IEOF]", c->self);
		chan_send_ieof(c);
		c->istate = CHAN_INPUT_CLOSED;
		break;
	default:
		error("protocol error: chan_rcvd_oclose %d for istate %d", c->self, c->istate);
		return;
	}
	chan_delete_if_full_closed(c);
}
void
chan_read_failed(Channel *c)
{
	switch (c->istate) {
	case CHAN_INPUT_OPEN:
		debug("channel %d: INPUT_OPEN -> INPUT_WAIT_DRAIN [read failed]", c->self);
		chan_shutdown_read(c);
		c->istate = CHAN_INPUT_WAIT_DRAIN;
		break;
	default:
		error("internal error: we do not read, but chan_read_failed %d for istate %d",
		      c->self, c->istate);
		break;
	}
}
void
chan_ibuf_empty(Channel *c)
{
	if (buffer_len(&c->input)) {
		error("internal error: chan_ibuf_empty %d for non empty buffer", c->self);
		return;
	}
	switch (c->istate) {
	case CHAN_INPUT_WAIT_DRAIN:
		debug("channel %d: INPUT_WAIT_DRAIN -> INPUT_WAIT_OCLOSE [inbuf empty, send IEOF]", c->self);
		chan_send_ieof(c);
		c->istate = CHAN_INPUT_WAIT_OCLOSE;
		break;
	default:
		error("internal error: chan_ibuf_empty %d for istate %d", c->self, c->istate);
		break;
	}
}

/* events concerning the OUTPUT from channel for socket (ostate) */
void
chan_rcvd_ieof(Channel *c)
{
	switch (c->ostate) {
	case CHAN_OUTPUT_OPEN:
		debug("channel %d: OUTPUT_OPEN -> OUTPUT_WAIT_DRAIN [rvcd IEOF]", c->self);
		c->ostate = CHAN_OUTPUT_WAIT_DRAIN;
		break;
	case CHAN_OUTPUT_WAIT_IEOF:
		debug("channel %d: OUTPUT_WAIT_IEOF -> OUTPUT_CLOSED [rvcd IEOF]", c->self);
		c->ostate = CHAN_OUTPUT_CLOSED;
		chan_delete_if_full_closed(c);
		break;
	default:
		error("protocol error: chan_rcvd_ieof %d for ostate %d", c->self, c->ostate);
		break;
	}
}
void
chan_write_failed(Channel *c)
{
	switch (c->ostate) {
	case CHAN_OUTPUT_OPEN:
		debug("channel %d: OUTPUT_OPEN -> OUTPUT_WAIT_IEOF [write failed]", c->self);
		chan_send_oclose(c);
		c->ostate = CHAN_OUTPUT_WAIT_IEOF;
		break;
	case CHAN_OUTPUT_WAIT_DRAIN:
		debug("channel %d: OUTPUT_WAIT_DRAIN -> OUTPUT_CLOSED [write failed]", c->self);
		chan_send_oclose(c);
		c->ostate = CHAN_OUTPUT_CLOSED;
		chan_delete_if_full_closed(c);
		break;
	default:
		error("internal error: chan_write_failed %d for ostate %d", c->self, c->ostate);
		break;
	}
}
void
chan_obuf_empty(Channel *c)
{
	if (buffer_len(&c->output)) {
		debug("internal error: chan_obuf_empty %d for non empty buffer", c->self);
		return;
	}
	switch (c->ostate) {
	case CHAN_OUTPUT_WAIT_DRAIN:
		debug("channel %d: OUTPUT_WAIT_DRAIN -> OUTPUT_CLOSED [obuf empty, send OCLOSE]", c->self);
		chan_send_oclose(c);
		c->ostate = CHAN_OUTPUT_CLOSED;
		chan_delete_if_full_closed(c);
		break;
	default:
		error("internal error: chan_obuf_empty %d for ostate %d", c->self, c->ostate);
		break;
	}
}

/*
 * ACTIONS: should never update the channel states: c->istate or c->ostate
 */
static void
chan_send_ieof(Channel *c)
{
	switch (c->istate) {
	case CHAN_INPUT_OPEN:
	case CHAN_INPUT_WAIT_DRAIN:
		packet_start(SSH_MSG_CHANNEL_INPUT_EOF);
		packet_put_int(c->remote_id);
		packet_send();
		break;
	default:
		error("internal error: channel %d: cannot send IEOF for istate %d", c->self, c->istate);
		break;
	}
}
static void
chan_send_oclose(Channel *c)
{
	switch (c->ostate) {
	case CHAN_OUTPUT_OPEN:
	case CHAN_OUTPUT_WAIT_DRAIN:
		chan_shutdown_write(c);
		buffer_consume(&c->output, buffer_len(&c->output));
		packet_start(SSH_MSG_CHANNEL_OUTPUT_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
		break;
	default:
		error("internal error: channel %d: cannot send OCLOSE for ostate %d", c->self, c->istate);
		break;
	}
}

/* helper */
static void
chan_shutdown_write(Channel *c)
{
	/* shutdown failure is allowed if write failed already */
	debug("channel %d: shutdown_write", c->self);
	if (shutdown(c->sock, SHUT_WR) < 0)
		debug("chan_shutdown_write failed for #%d/fd%d: %.100s",
		      c->self, c->sock, strerror(errno));
}
static void
chan_shutdown_read(Channel *c)
{
	debug("channel %d: shutdown_read", c->self);
	if (shutdown(c->sock, SHUT_RD) < 0)
		error("chan_shutdown_read failed for #%d/fd%d [i%d o%d]: %.100s",
		      c->self, c->sock, c->istate, c->ostate, strerror(errno));
}
static void
chan_delete_if_full_closed(Channel *c)
{
	if (c->istate == CHAN_INPUT_CLOSED && c->ostate == CHAN_OUTPUT_CLOSED) {
		debug("channel %d: full closed", c->self);
		channel_free(c->self);
	}
}
void
chan_init_iostates(Channel *c)
{
	c->ostate = CHAN_OUTPUT_OPEN;
	c->istate = CHAN_INPUT_OPEN;
}
