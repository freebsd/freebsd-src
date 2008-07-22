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

/* RCSID("$OpenBSD: nchan.h,v 1.10 2001/02/28 08:54:55 markus Exp $"); */

#ifndef NCHAN_H
#define NCHAN_H

/*
 * SSH Protocol 1.5 aka New Channel Protocol
 * Thanks to Martina, Axel and everyone who left Erlangen, leaving me bored.
 * Written by Markus Friedl in October 1999
 *
 * Protocol versions 1.3 and 1.5 differ in the handshake protocol used for the
 * tear down of channels:
 *
 * 1.3:	strict request-ack-protocol:
 * 	CLOSE	->
 * 		<-  CLOSE_CONFIRM
 *
 * 1.5:	uses variations of:
 * 	IEOF	->
 * 		<-  OCLOSE
 * 		<-  IEOF
 * 	OCLOSE	->
 * 	i.e. both sides have to close the channel
 *
 * See the debugging output from 'ssh -v' and 'sshd -d' of
 * ssh-1.2.27 as an example.
 *
 */

/* ssh-proto-1.5 overloads prot-1.3-message-types */
#define SSH_MSG_CHANNEL_INPUT_EOF	SSH_MSG_CHANNEL_CLOSE
#define SSH_MSG_CHANNEL_OUTPUT_CLOSE	SSH_MSG_CHANNEL_CLOSE_CONFIRMATION

/* possible input states */
#define CHAN_INPUT_OPEN			0x01
#define CHAN_INPUT_WAIT_DRAIN		0x02
#define CHAN_INPUT_WAIT_OCLOSE		0x04
#define CHAN_INPUT_CLOSED		0x08

/* possible output states */
#define CHAN_OUTPUT_OPEN		0x10
#define CHAN_OUTPUT_WAIT_DRAIN		0x20
#define CHAN_OUTPUT_WAIT_IEOF		0x40
#define CHAN_OUTPUT_CLOSED		0x80

#define CHAN_CLOSE_SENT			0x01
#define CHAN_CLOSE_RCVD			0x02


/* Channel EVENTS */
typedef void    chan_event_fn(Channel * c);

/* for the input state */
extern chan_event_fn	*chan_rcvd_oclose;
extern chan_event_fn	*chan_read_failed;
extern chan_event_fn	*chan_ibuf_empty;

/* for the output state */
extern chan_event_fn	*chan_rcvd_ieof;
extern chan_event_fn	*chan_write_failed;
extern chan_event_fn	*chan_obuf_empty;

int chan_is_dead(Channel * c);

void    chan_init_iostates(Channel * c);
void	chan_init(void);
#endif
