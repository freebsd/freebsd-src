/*	$NetBSD: midwayvar.h,v 1.10 1997/03/20 21:34:46 chuck Exp $	*/

/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *	Washington University.
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

/*
 * m i d w a y v a r . h
 *
 * we define the en_softc here so that bus specific modules can allocate
 * it as the first item in their softc.   note that BSD-required 
 * "struct device" is in the mid_softc!
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 */

/*
 * params needed to determine softc size
 */

#ifndef EN_NTX
#define EN_NTX          8       /* number of tx bufs to use */
#endif
#ifndef EN_TXSZ
#define EN_TXSZ         32      /* trasmit buf size in KB */
#endif
#ifndef EN_RXSZ
#define EN_RXSZ         32      /* recv buf size in KB */
#endif
#define EN_MAXNRX       ((2048-(EN_NTX*EN_TXSZ))/EN_RXSZ)
				/* largest possible NRX (depends on RAM size) */


#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
#define EN_INTR_TYPE int
#define EN_INTR_RET(X) return(X)
#if defined(__NetBSD__) || defined(__OpenBSD__)
#define EN_IOCTL_CMDT u_long
#elif defined(__bsdi__)
#define EN_IOCTL_CMDT int
#endif

#elif defined(__FreeBSD__)

#define EN_INTR_TYPE void
#define EN_INTR_RET(X) return
#define EN_IOCTL_CMDT u_long

struct device {
  char dv_xname[IFNAMSIZ];
};

#define DV_IFNET 1

struct cfdriver {
  int zero;
  char *name;
  int one;
  int cd_ndevs;
  void *cd_devs[NEN];
};

#endif

/*
 * softc
 */

struct en_softc {
  /* bsd glue */
  struct device sc_dev;		/* system device */
  struct ifnet enif;		/* network ifnet handle */

  /* bus glue */
  bus_space_tag_t en_memt;	/* for EN_READ/EN_WRITE */
  bus_space_handle_t en_base;	/* base of en card */
  bus_size_t en_obmemsz;	/* size of en card (bytes) */
  void (*en_busreset) __P((void *));
				/* bus specific reset function */

  /* serv list */
  u_int32_t hwslistp;		/* hw pointer to service list (byte offset) */
  u_int16_t swslist[MID_SL_N];	/* software service list (see en_service()) */
  u_int16_t swsl_head, 		/* ends of swslist (index into swslist) */
	    swsl_tail;
  u_int32_t swsl_size;		/* # of items in swsl */
  

  /* xmit dma */
  u_int32_t dtq[MID_DTQ_N];	/* sw copy of dma q (see ENIDQ macros) */
  u_int32_t dtq_free;		/* # of dtq's free */
  u_int32_t dtq_us;		/* software copy of our pointer (byte offset) */
  u_int32_t dtq_chip;		/* chip's pointer (byte offset) */
  u_int32_t need_dtqs;		/* true if we ran out of DTQs */

  /* recv dma */
  u_int32_t drq[MID_DRQ_N];	/* sw copy of dma q (see ENIDQ macros) */
  u_int32_t drq_free;		/* # of drq's free */
  u_int32_t drq_us;		/* software copy of our pointer (byte offset) */
  u_int32_t drq_chip;		/* chip's pointer (byte offset) */
  u_int32_t need_drqs;		/* true if we ran out of DRQs */

  /* xmit buf ctrl. (per channel) */
  struct {
    u_int32_t mbsize;		/* # mbuf bytes we are using (max=TXHIWAT) */
    u_int32_t bfree;		/* # free bytes in buffer (not dma or xmit) */
    u_int32_t start, stop;	/* ends of buffer area (byte offset) */
    u_int32_t cur;		/* next free area (byte offset) */
    u_int32_t nref;		/* # of VCs using this channel */
    struct ifqueue indma;	/* mbufs being dma'd now */
    struct ifqueue q;		/* mbufs waiting for dma now */
  } txslot[MID_NTX_CH];

  /* xmit vc ctrl. (per vc) */
  u_int8_t txspeed[MID_N_VC];	/* speed of tx on a VC */
  u_int8_t txvc2slot[MID_N_VC]; /* map VC to slot */

  /* recv vc ctrl. (per vc).   maps VC number to recv slot */
  u_int16_t rxvc2slot[MID_N_VC];
  int en_nrx;			/* # of active rx slots */

  /* recv buf ctrl. (per recv slot) */
  struct {
    void *rxhand;		/* recv. handle if doing direct delivery */
    u_int32_t mode;		/* saved copy of mode info */
    u_int32_t start, stop;	/* ends of my buffer area */
    u_int32_t cur;		/* where I am at */
    u_int16_t atm_vci;		/* backpointer to VCI */
    u_int8_t atm_flags;		/* copy of atm_flags from atm_ph */
    u_int8_t oth_flags;		/* other flags */
    u_int32_t raw_threshold;	/* for raw mode */
    struct ifqueue indma;	/* mbufs being dma'd now */
    struct ifqueue q;		/* mbufs waiting for dma now */
  } rxslot[EN_MAXNRX];		/* recv info */

  u_int8_t macaddr[6];		/* card unique mac address */

  /* stats */
  u_int32_t vtrash;		/* sw copy of counter */
  u_int32_t otrash;		/* sw copy of counter */
  u_int32_t ttrash;		/* # of RBD's with T bit set */
  u_int32_t mfix;		/* # of times we had to call mfix */
  u_int32_t mfixfail;		/* # of times mfix failed */
  u_int32_t headbyte;		/* # of times we used BYTE DMA at front */
  u_int32_t tailbyte;		/* # of times we used BYTE DMA at end */
  u_int32_t tailflush;		/* # of times we had to FLUSH out DMA bytes */
  u_int32_t txmbovr;		/* # of times we dropped due to mbsize */
  u_int32_t dmaovr;		/* tx dma overflow count */
  u_int32_t txoutspace;		/* out of space in xmit buffer */
  u_int32_t txdtqout;		/* out of DTQs */
  u_int32_t launch;		/* total # of launches */
  u_int32_t lheader;		/* # of launches without OB header */
  u_int32_t ltail;		/* # of launches without OB tail */
  u_int32_t hwpull;		/* # of pulls off hardware service list */
  u_int32_t swadd;		/* # of pushes on sw service list */
  u_int32_t rxqnotus;		/* # of times we pull from rx q, but fail */
  u_int32_t rxqus;		/* # of good pulls from rx q */
  u_int32_t rxoutboth;		/* # of times out of mbufs and DRQs */
  u_int32_t rxdrqout;		/* # of times out of DRQs */
  u_int32_t rxmbufout;		/* # of time out of mbufs */

  /* random stuff */
  u_int32_t ipl;		/* sbus interrupt lvl (1 on pci?) */
  u_int8_t bestburstcode;	/* code of best burst we can use */
  u_int8_t bestburstlen;	/* length of best burst (bytes) */
  u_int8_t bestburstshift;	/* (x >> shift) == (x / bestburstlen) */
  u_int8_t bestburstmask;	/* bits to check if not multiple of burst */
  u_int8_t alburst;		/* align dma bursts? */
  u_int8_t is_adaptec;		/* adaptec version of midway? */
};

/*
 * exported functions
 */

void	en_attach __P((struct en_softc *));
EN_INTR_TYPE	en_intr __P((void *));
void	en_reset __P((struct en_softc *));
