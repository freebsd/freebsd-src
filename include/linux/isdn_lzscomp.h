/* $Id: isdn_lzscomp.h,v 1.1.4.1 2001/11/20 14:19:38 kai Exp $
 *
 * Header for isdn_lzscomp.c
 * Concentrated here to not mess up half a dozen kernel headers with code
 * snippets
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define CI_LZS_COMPRESS		17
#define CILEN_LZS_COMPRESS	5

#define LZS_CMODE_NONE		0
#define LZS_CMODE_LCB		1
#define LZS_CMODE_CRC		2
#define LZS_CMODE_SEQNO		3	/* MUST be implemented (default) */
#define LZS_CMODE_EXT		4	/* Seems to be what Win0.95 uses */

#define LZS_COMP_MAX_HISTS	1	/* Don't waste peers ressources */
#define LZS_COMP_DEF_HISTS	1	/* Most likely to negotiate */
#define LZS_DECOMP_MAX_HISTS	32	/* More is really nonsense */
#define LZS_DECOMP_DEF_HISTS	8	/* If we get it, this may be optimal */

#define LZS_HIST_BYTE1(word)   	(word>>8)	/* Just for better reading */
#define LZS_HIST_BYTE2(word)	(word&0xff)	/* of this big endian stuff */
#define LZS_HIST_WORD(b1,b2)	((b1<<8)|b2)	/* (network byte order rulez) */
