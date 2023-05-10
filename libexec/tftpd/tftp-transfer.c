/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2008 Edwin Groothuis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/tftp.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "tftp-file.h"
#include "tftp-io.h"
#include "tftp-utils.h"
#include "tftp-options.h"
#include "tftp-transfer.h"

struct block_data {
	off_t offset;
	uint16_t block;
	int size;
};

/*
 * Send a file via the TFTP data session.
 */
int
tftp_send(int peer, uint16_t *block, struct tftp_stats *ts)
{
	struct tftphdr *rp;
	int size, n_data, n_ack, sendtry, acktry;
	u_int i, j;
	uint16_t oldblock, windowblock;
	char sendbuffer[MAXPKTSIZE];
	char recvbuffer[MAXPKTSIZE];
	struct block_data window[WINDOWSIZE_MAX];

	rp = (struct tftphdr *)recvbuffer;
	*block = 1;
	ts->amount = 0;
	windowblock = 0;
	acktry = 0;
	do {
read_block:
		if (debug & DEBUG_SIMPLE)
			tftp_log(LOG_DEBUG, "Sending block %d (window block %d)",
			    *block, windowblock);

		window[windowblock].offset = tell_file();
		window[windowblock].block = *block;
		size = read_file(sendbuffer, segsize);
		if (size < 0) {
			tftp_log(LOG_ERR, "read_file returned %d", size);
			send_error(peer, errno + 100);
			return -1;
		}
		window[windowblock].size = size;
		windowblock++;

		for (sendtry = 0; ; sendtry++) {
			n_data = send_data(peer, *block, sendbuffer, size);
			if (n_data == 0)
				break;

			if (sendtry == maxtimeouts) {
				tftp_log(LOG_ERR,
				    "Cannot send DATA packet #%d, "
				    "giving up", *block);
				return -1;
			}
			tftp_log(LOG_ERR,
			    "Cannot send DATA packet #%d, trying again",
			    *block);
		}

		/* Only check for ACK for last block in window. */
		if (windowblock == windowsize || size != segsize) {
			n_ack = receive_packet(peer, recvbuffer,
			    MAXPKTSIZE, NULL, timeoutpacket);
			if (n_ack < 0) {
				if (n_ack == RP_TIMEOUT) {
					if (acktry == maxtimeouts) {
						tftp_log(LOG_ERR,
						    "Timeout #%d send ACK %d "
						    "giving up", acktry, *block);
						return -1;
					}
					tftp_log(LOG_WARNING,
					    "Timeout #%d on ACK %d",
					    acktry, *block);

					acktry++;
					ts->retries++;
					if (seek_file(window[0].offset) != 0) {
						tftp_log(LOG_ERR,
						    "seek_file failed: %s",
						    strerror(errno));
						send_error(peer, errno + 100);
						return -1;
					}
					*block = window[0].block;
					windowblock = 0;
					goto read_block;
				}

				/* Either read failure or ERROR packet */
				if (debug & DEBUG_SIMPLE)
					tftp_log(LOG_ERR, "Aborting: %s",
					    rp_strerror(n_ack));
				return -1;
			}
			if (rp->th_opcode == ACK) {
				/*
				 * Look for the ACKed block in our open
				 * window.
				 */
				for (i = 0; i < windowblock; i++) {
					if (rp->th_block == window[i].block)
						break;
				}

				if (i == windowblock) {
					/* Did not recognize ACK. */
					if (debug & DEBUG_SIMPLE)
						tftp_log(LOG_DEBUG,
						    "ACK %d out of window",
						    rp->th_block);

					/* Re-synchronize with the other side */
					(void) synchnet(peer);

					/* Resend the current window. */
					ts->retries++;
					if (seek_file(window[0].offset) != 0) {
						tftp_log(LOG_ERR,
						    "seek_file failed: %s",
						    strerror(errno));
						send_error(peer, errno + 100);
						return -1;
					}
					*block = window[0].block;
					windowblock = 0;
					goto read_block;
				}

				/* ACKed at least some data. */
				acktry = 0;
				for (j = 0; j <= i; j++) {
					if (debug & DEBUG_SIMPLE)
						tftp_log(LOG_DEBUG,
						    "ACKed block %d",
						    window[j].block);
					ts->blocks++;
					ts->amount += window[j].size;
				}

				/*
				 * Partial ACK.  Rewind state to first
				 * un-ACKed block.
				 */
				if (i + 1 != windowblock) {
					if (debug & DEBUG_SIMPLE)
						tftp_log(LOG_DEBUG,
						    "Partial ACK");
					if (seek_file(window[i + 1].offset) !=
					    0) {
						tftp_log(LOG_ERR,
						    "seek_file failed: %s",
						    strerror(errno));
						send_error(peer, errno + 100);
						return -1;
					}
					*block = window[i + 1].block;
					windowblock = 0;
					ts->retries++;
					goto read_block;
				}

				windowblock = 0;
			}

		}
		oldblock = *block;
		(*block)++;
		if (oldblock > *block) {
			if (options[OPT_ROLLOVER].o_request == NULL) {
				/*
				 * "rollover" option not specified in
				 * tftp client.  Default to rolling block
				 * counter to 0.
				 */
				*block = 0;
			} else {
				*block = atoi(options[OPT_ROLLOVER].o_request);
			}

			ts->rollovers++;
		}
		gettimeofday(&(ts->tstop), NULL);
	} while (size == segsize);
	return 0;
}

/*
 * Receive a file via the TFTP data session.
 *
 * - It could be that the first block has already arrived while
 *   trying to figure out if we were receiving options or not. In
 *   that case it is passed to this function.
 */
int
tftp_receive(int peer, uint16_t *block, struct tftp_stats *ts,
    struct tftphdr *firstblock, size_t fb_size)
{
	struct tftphdr *rp;
	uint16_t oldblock, windowstart;
	int n_data, n_ack, writesize, i, retry, windowblock;
	char recvbuffer[MAXPKTSIZE];

	ts->amount = 0;
	windowblock = 0;

	if (firstblock != NULL) {
		writesize = write_file(firstblock->th_data, fb_size);
		ts->amount += writesize;
		ts->blocks++;
		windowblock++;
		if (windowsize == 1 || fb_size != segsize) {
			for (i = 0; ; i++) {
				n_ack = send_ack(peer, *block);
				if (n_ack > 0) {
					if (i == maxtimeouts) {
						tftp_log(LOG_ERR,
						    "Cannot send ACK packet #%d, "
						    "giving up", *block);
						return -1;
					}
					tftp_log(LOG_ERR,
					    "Cannot send ACK packet #%d, trying again",
					    *block);
					continue;
				}

				break;
			}
		}

		if (fb_size != segsize) {
			write_close();
			gettimeofday(&(ts->tstop), NULL);
			return 0;
		}
	}

	rp = (struct tftphdr *)recvbuffer;
	do {
		oldblock = *block;
		(*block)++;
		if (oldblock > *block) {
			if (options[OPT_ROLLOVER].o_request == NULL) {
				/*
				 * "rollover" option not specified in
				 * tftp client.  Default to rolling block
				 * counter to 0.
				 */
				*block = 0;
			} else {
				*block = atoi(options[OPT_ROLLOVER].o_request);
			}

			ts->rollovers++;
		}

		for (retry = 0; ; retry++) {
			if (debug & DEBUG_SIMPLE)
				tftp_log(LOG_DEBUG,
				    "Receiving DATA block %d (window block %d)",
				    *block, windowblock);

			n_data = receive_packet(peer, recvbuffer,
			    MAXPKTSIZE, NULL, timeoutpacket);
			if (n_data < 0) {
				if (retry == maxtimeouts) {
					tftp_log(LOG_ERR,
					    "Timeout #%d on DATA block %d, "
					    "giving up", retry, *block);
					return -1;
				}
				if (n_data == RP_TIMEOUT) {
					tftp_log(LOG_WARNING,
					    "Timeout #%d on DATA block %d",
					    retry, *block);
					send_ack(peer, oldblock);
					windowblock = 0;
					continue;
				}

				/* Either read failure or ERROR packet */
				if (debug & DEBUG_SIMPLE)
					tftp_log(LOG_DEBUG, "Aborting: %s",
					    rp_strerror(n_data));
				return -1;
			}
			if (rp->th_opcode == DATA) {
				ts->blocks++;

				if (rp->th_block == *block)
					break;

				/*
				 * Ignore duplicate blocks within the
				 * window.
				 *
				 * This does not handle duplicate
				 * blocks during a rollover as
				 * gracefully, but that should still
				 * recover eventually.
				 */
				if (*block > windowsize)
					windowstart = *block - windowsize;
				else
					windowstart = 0;
				if (rp->th_block > windowstart &&
				    rp->th_block < *block) {
					if (debug & DEBUG_SIMPLE)
						tftp_log(LOG_DEBUG,
					    "Ignoring duplicate DATA block %d",
						    rp->th_block);
					windowblock++;
					retry = 0;
					continue;
				}

				tftp_log(LOG_WARNING,
				    "Expected DATA block %d, got block %d",
				    *block, rp->th_block);

				/* Re-synchronize with the other side */
				(void) synchnet(peer);

				tftp_log(LOG_INFO, "Trying to sync");
				*block = oldblock;
				ts->retries++;
				goto send_ack;	/* rexmit */

			} else {
				tftp_log(LOG_WARNING,
				    "Expected DATA block, got %s block",
				    packettype(rp->th_opcode));
			}
		}

		if (n_data > 0) {
			writesize = write_file(rp->th_data, n_data);
			ts->amount += writesize;
			if (writesize <= 0) {
				tftp_log(LOG_ERR,
				    "write_file returned %d", writesize);
				if (writesize < 0)
					send_error(peer, errno + 100);
				else
					send_error(peer, ENOSPACE);
				return -1;
			}
		}
		if (n_data != segsize)
			write_close();
		windowblock++;

		/* Only send ACKs for the last block in the window. */
		if (windowblock < windowsize && n_data == segsize)
			continue;
send_ack:
		for (i = 0; ; i++) {
			n_ack = send_ack(peer, *block);
			if (n_ack > 0) {

				if (i == maxtimeouts) {
					tftp_log(LOG_ERR,
					    "Cannot send ACK packet #%d, "
					    "giving up", *block);
					return -1;
				}

				tftp_log(LOG_ERR,
				    "Cannot send ACK packet #%d, trying again",
				    *block);
				continue;
			}

			if (debug & DEBUG_SIMPLE)
				tftp_log(LOG_DEBUG, "Sent ACK for %d", *block);
			windowblock = 0;
			break;
		}
		gettimeofday(&(ts->tstop), NULL);
	} while (n_data == segsize);

	/* Don't do late packet management for the client implementation */
	if (acting_as_client)
		return 0;

	for (i = 0; ; i++) {
		n_data = receive_packet(peer, (char *)rp, pktsize,
		    NULL, -timeoutpacket);
		if (n_data <= 0)
			break;
		if (n_data > 0 &&
		    rp->th_opcode == DATA &&	/* and got a data block */
		    *block == rp->th_block)	/* then my last ack was lost */
			send_ack(peer, *block);	/* resend final ack */
	}

	return 0;
}
