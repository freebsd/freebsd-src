/*-
 * Copyright (c) 2012 Adrian Chadd
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
 * $FreeBSD$
 */
#ifndef	__IF_ATH_ALQ_H__
#define	__IF_ATH_ALQ_H__

#define	ATH_ALQ_INIT_STATE		1
struct if_ath_alq_init_state {
	uint32_t	sc_mac_version;
	uint32_t	sc_mac_revision;
	uint32_t	sc_phy_rev;
	uint32_t	sc_hal_magic;
};

#define	ATH_ALQ_EDMA_TXSTATUS		2
#define	ATH_ALQ_EDMA_RXSTATUS		3
#define	ATH_ALQ_EDMA_TXDESC		4

/*
 * These will always be logged, regardless.
 */
#define	ATH_ALQ_LOG_ALWAYS_MASK		0x00000001

#define	ATH_ALQ_FILENAME_LEN	128
#define	ATH_ALQ_DEVNAME_LEN	32

struct if_ath_alq {
	uint32_t	sc_alq_debug;		/* Debug flags to report */
	struct alq *	sc_alq_alq;		/* alq state */
	unsigned int	sc_alq_qsize;		/* queue size */
	unsigned int	sc_alq_numlost;		/* number of "lost" entries */
	int		sc_alq_isactive;
	char		sc_alq_devname[ATH_ALQ_DEVNAME_LEN];
	char		sc_alq_filename[ATH_ALQ_FILENAME_LEN];
	struct if_ath_alq_init_state sc_alq_cfg;
};

/* 128 bytes in total */
#define	ATH_ALQ_PAYLOAD_LEN		112

struct if_ath_alq_hdr {
	uint64_t	threadid;
	uint32_t	tstamp;
	uint16_t	op;
	uint16_t	len;	/* Length of (optional) payload */
};

struct if_ath_alq_payload {
	struct if_ath_alq_hdr hdr;
	char		payload[];
};

#ifdef	_KERNEL
static inline int
if_ath_alq_checkdebug(struct if_ath_alq *alq, uint16_t op)
{

	return ((alq->sc_alq_debug | ATH_ALQ_LOG_ALWAYS_MASK)
	    & (1 << (op - 1)));
}

extern	void if_ath_alq_init(struct if_ath_alq *alq, const char *devname);
extern	void if_ath_alq_setcfg(struct if_ath_alq *alq, uint32_t macVer,
	    uint32_t macRev, uint32_t phyRev, uint32_t halMagic);
extern	void if_ath_alq_tidyup(struct if_ath_alq *alq);
extern	int if_ath_alq_start(struct if_ath_alq *alq);
extern	int if_ath_alq_stop(struct if_ath_alq *alq);
extern	void if_ath_alq_post(struct if_ath_alq *alq, uint16_t op,
	    uint16_t len, const char *buf);
#endif	/* _KERNEL */

#endif
