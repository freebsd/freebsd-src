/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Protocol and implementation definitions
 *
 */

#ifndef _FORE_H
#define _FORE_H

#ifndef	FORE_DEV_NAME
#define	FORE_DEV_NAME	"hfa"
#endif

#define	FORE_MAX_UNITS	8	/* Maximum number of devices we support */
#define	FORE_MIN_UCODE	0x20300	/* Minimum microcode version we support */

#define	FORE_IFF_MTU	9188	/* Network interface MTU */
#define	FORE_MAX_VCC	1024	/* Maximum number of open VCCs */
#define	FORE_MAX_VPI	0	/* Maximum VPI value */
#define	FORE_MAX_VCI	1023	/* Maximum VCI value */
#define	FORE_DEF_RATE	0x00000000	/* Default rate control = disabled */

#define	XMIT_QUELEN	32	/* Length of transmit queue */
#define	RECV_QUELEN	32	/* Length of receive queue */
#define	CMD_QUELEN	8	/* Length of command queue */

#define	FORE_TIME_TICK	5	/* Watchdog timer tick (seconds) */
#define	FORE_WATCHDOG	3	/* Device watchdog timeout (ticks) */
#define	FORE_RECV_RETRY	3	/* Wait for receive queue entry retry count */
#define	FORE_RECV_DELAY	10	/* Wait for receive queue entry delay (usec) */


/*
 * Receive Buffer strategies
 */
#define	BUF_MIN_VCC	4	/* Minimum for buffer supply calculations */

#define	BUF_DATA_ALIGN	4	/* Fore-required data alignment */

/*
 * Strategy 1 Small - mbuf
 * Strategy 1 Large - cluster mbuf
 *
 * XXX buffer controls - the RECV_MAX_SEGS calculation comes out wrong
 * using the true buffer size values if the CP really only does full-cell
 * filling of a particular buffer - we must clarify this...it also appears
 * the minimum buffer size is 64, even if the CP can only fit in 1 cell.
 */
#define SIZEOF_Buf_handle	16	/* XXX sizeof(Buf_handle) */

#undef m_ext
typedef struct m_ext	M_ext;
#define	m_ext		M_dat.MH.MH_dat.MH_ext
#define	BUF1_SM_HOFF	(sizeof(struct m_hdr))	/* Buffer-to-handle offset */
#define	BUF1_SM_HDR	(sizeof(struct m_hdr) + sizeof(struct pkthdr))
#define	BUF1_SM_LEN	(MHLEN)
#define	BUF1_LG_HOFF	(sizeof(struct m_hdr) + sizeof(struct pkthdr) \
			    + sizeof(M_ext))	/* Buffer-to-handle offset */
/*
 * BUF1_SM_DOFF - CP data offset into buffer data space
 * BUF1_SM_SIZE - Buffer size
 *
 * These should be defined as follows, but we need compile-time constants:
 *
 *	#define	BUF1_SM_DOFF (roundup(BUF1_SM_HOFF + SIZEOF_Buf_handle, 
 *			BUF_DATA_ALIGN) - BUF1_SM_HDR)
 *	#define	BUF1_SM_SIZE	MAX(BUF1_SM_LEN - BUF1_SM_DOFF, 64)
 *
 */
#define	BUF1_SM_DOFF	((BUF1_SM_HOFF + SIZEOF_Buf_handle) - BUF1_SM_HDR)
#define	BUF1_SM_SIZE	(BUF1_SM_LEN - BUF1_SM_DOFF)

#define	BUF1_SM_QUELEN	16	/* Entries in supply queue */
#define	BUF1_SM_CPPOOL	256	/* Buffers in CP-resident pool */
#define	BUF1_SM_ENTSIZE	8	/* Buffers in each supply queue entry */

#define	BUF1_LG_DOFF	0	/* CP data offset into mbuf data space */
#define	BUF1_LG_SIZE	MCLBYTES	/* Buffer size */
#define	BUF1_LG_QUELEN	16	/* Entries in supply queue */
#define	BUF1_LG_CPPOOL	512	/* Buffers in CP-resident pool */
#define	BUF1_LG_ENTSIZE	8	/* Buffers in each supply queue entry */

#endif	/* _FORE_H */
