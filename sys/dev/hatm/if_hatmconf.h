/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * $FreeBSD$
 *
 * Default configuration
 */

/* configuration */
#define HE_CONFIG_VPI_BITS	2
#define HE_CONFIG_VCI_BITS	10

/* interrupt group 0 only */
/* the size must be 1 <= size <= 1023 */
#define HE_CONFIG_IRQ0_SIZE	256
#define HE_CONFIG_IRQ0_THRESH	224		/* retrigger interrupt */
#define HE_CONFIG_IRQ0_LINE	HE_REGM_IRQ_A	/* routing */

/* don't change these */
#define HE_CONFIG_TXMEM		(128 * 1024)	/* words */
#define HE_CONFIG_RXMEM		(64 * 1024)	/* words */
#define HE_CONFIG_LCMEM		(512 * 1024)	/* words */

/* group 0 - all AALs except AAL.raw */
/* receive group 0 buffer pools (mbufs and mbufs+cluster) */
/* the size must be a power of 2: 4 <= size <= 8192 */
#define HE_CONFIG_RBPS0_SIZE	2048	/* entries per queue */
#define HE_CONFIG_RBPS0_THRESH	256	/* interrupt threshold */
#define HE_CONFIG_RBPL0_SIZE	512	/* entries per queue */
#define HE_CONFIG_RBPL0_THRESH	32	/* interrupt threshold */

/* receive group 0 buffer return queue */
/* the size must be a power of 2: 1 <= size <= 16384 */
#define HE_CONFIG_RBRQ0_SIZE	512	/* entries in queue */
#define HE_CONFIG_RBRQ0_THRESH	256	/* interrupt threshold */
#define HE_CONFIG_RBRQ0_TOUT	10	/* interrupt timeout */
#define HE_CONFIG_RBRQ0_PCNT	5	/* packet count threshold */

/* group 1 - raw cells */
/* receive group 1 small buffer pool */
/* the size must be a power of 2: 4 <= size <= 8192 */
#define HE_CONFIG_RBPS1_SIZE	1024	/* entries in queue */
#define HE_CONFIG_RBPS1_THRESH	512	/* interrupt threshold */

/* receive group 1 buffer return queue */
/* the size must be a power of 2: 1 <= size <= 16384 */
#define HE_CONFIG_RBRQ1_SIZE	512	/* entries in queue */
#define HE_CONFIG_RBRQ1_THRESH	256	/* interrupt threshold */
#define HE_CONFIG_RBRQ1_TOUT	100	/* interrupt timeout */
#define HE_CONFIG_RBRQ1_PCNT	25	/* packet count threshold */

/* there is only one TPD queue */
/* the size must be a power of 2: 1 <= size <= 4096 */
#define HE_CONFIG_TPDRQ_SIZE	2048	/* entries in queue */

/* transmit group 0 */
/* the size must be a power of 2: 1 <= size <= 16384 */
#define HE_CONFIG_TBRQ_SIZE	512	/* entries in queue */
#define HE_CONFIG_TBRQ_THRESH	400	/* interrupt threshold */

/* Maximum number of TPDs to allocate to a single VCC. This
 * number should depend on the cell rate and the maximum allowed cell delay */
#define HE_CONFIG_TPD_MAXCC	2048

/* Maximum number of external mbuf pages */
#define HE_CONFIG_MAX_MBUF_PAGES	256

/* Maximum number of TPDs used for one packet */
#define HE_CONFIG_MAX_TPD_PER_PACKET			\
	((((HE_MAX_PDU + MCLBYTES - 1) / MCLBYTES + 2) / 3) + 2)

/* Number of TPDs to reserve for close operations */
#define HE_CONFIG_TPD_RESERVE	32

/* Number of TPDs per VCC when to re-enable flow control */
#define HE_CONFIG_TPD_FLOW_ENB	80

/* MCR for flushing CBR and ABR connections at close */
#define HE_CONFIG_FLUSH_RATE	200000
