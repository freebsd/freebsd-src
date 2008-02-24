/*-
 * Copyright (c) 1997, 1998, 1999, 2000-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/usb/if_axereg.h,v 1.16 2007/09/25 21:08:33 imp Exp $
 */

/*
 * Definitions for the ASIX Electronics AX88172 to ethernet controller.
 */


/*
 * Vendor specific commands
 * ASIX conveniently doesn't document the 'set NODEID' command in their
 * datasheet (thanks a lot guys).
 * To make handling these commands easier, I added some extra data
 * which is decided by the axe_cmd() routine. Commands are encoded
 * in 16 bites, with the format: LDCC. L and D are both nibbles in
 * the high byte. L represents the data length (0 to 15) and D
 * represents the direction (0 for vendor read, 1 for vendor write).
 * CC is the command byte, as specified in the manual.
 */

#define AXE_CMD_DIR(x)	(((x) & 0x0F00) >> 8)
#define AXE_CMD_LEN(x)	(((x) & 0xF000) >> 12)
#define AXE_CMD_CMD(x)	((x) & 0x00FF)

#define AXE_172_CMD_READ_RXTX_SRAM		0x2002
#define AXE_182_CMD_READ_RXTX_SRAM		0x8002
#define AXE_172_CMD_WRITE_RX_SRAM		0x0103
#define AXE_172_CMD_WRITE_TX_SRAM		0x0104
#define AXE_182_CMD_WRITE_RXTX_SRAM		0x8103
#define AXE_CMD_MII_OPMODE_SW			0x0106
#define AXE_CMD_MII_READ_REG			0x2007
#define AXE_CMD_MII_WRITE_REG			0x2108
#define AXE_CMD_MII_READ_OPMODE			0x1009
#define AXE_CMD_MII_OPMODE_HW			0x010A
#define AXE_CMD_SROM_READ			0x200B
#define AXE_CMD_SROM_WRITE			0x010C
#define AXE_CMD_SROM_WR_ENABLE			0x010D
#define AXE_CMD_SROM_WR_DISABLE			0x010E
#define AXE_CMD_RXCTL_READ			0x200F
#define AXE_CMD_RXCTL_WRITE			0x0110
#define AXE_CMD_READ_IPG012			0x3011
#define AXE_172_CMD_WRITE_IPG0			0x0112
#define AXE_172_CMD_WRITE_IPG1			0x0113
#define AXE_172_CMD_WRITE_IPG2			0x0114
#define AXE_178_CMD_WRITE_IPG012		0x0112
#define AXE_CMD_READ_MCAST			0x8015
#define AXE_CMD_WRITE_MCAST			0x8116
#define AXE_172_CMD_READ_NODEID			0x6017
#define AXE_172_CMD_WRITE_NODEID		0x6118
#define AXE_178_CMD_READ_NODEID			0x6013
#define AXE_178_CMD_WRITE_NODEID		0x6114
#define AXE_CMD_READ_PHYID			0x2019
#define AXE_172_CMD_READ_MEDIA			0x101A
#define AXE_178_CMD_READ_MEDIA			0x201A
#define AXE_CMD_WRITE_MEDIA			0x011B
#define AXE_CMD_READ_MONITOR_MODE		0x101C
#define AXE_CMD_WRITE_MONITOR_MODE		0x011D
#define AXE_CMD_READ_GPIO			0x101E
#define AXE_CMD_WRITE_GPIO			0x011F
#define AXE_CMD_SW_RESET_REG			0x0120
#define AXE_CMD_SW_PHY_STATUS			0x0021
#define AXE_CMD_SW_PHY_SELECT			0x0122

#define AXE_SW_RESET_CLEAR			0x00
#define AXE_SW_RESET_RR				0x01
#define AXE_SW_RESET_RT				0x02
#define AXE_SW_RESET_PRTE			0x04
#define AXE_SW_RESET_PRL			0x08
#define AXE_SW_RESET_BZ				0x10
#define AXE_SW_RESET_IPRL			0x20
#define AXE_SW_RESET_IPPD			0x40

/* AX88178 documentation says to always write this bit... */
#define AXE_178_RESET_MAGIC			0x40

#define AXE_178_MEDIA_GMII			0x0001
#define AXE_MEDIA_FULL_DUPLEX			0x0002
#define AXE_172_MEDIA_TX_ABORT_ALLOW		0x0004
/* AX88178/88772 documentation says to always write 1 to bit 2 */
#define AXE_178_MEDIA_MAGIC			0x0004
/* AX88772 documentation says to always write 0 to bit 3 */
#define AXE_178_MEDIA_ENCK			0x0008
#define AXE_172_MEDIA_FLOW_CONTROL_EN		0x0010
#define AXE_178_MEDIA_RXFLOW_CONTROL_EN		0x0010
#define AXE_178_MEDIA_TXFLOW_CONTROL_EN		0x0020
#define AXE_178_MEDIA_JUMBO_EN			0x0040
#define AXE_178_MEDIA_LTPF_ONLY			0x0080
#define AXE_178_MEDIA_RX_EN			0x0100
#define AXE_178_MEDIA_100TX			0x0200
#define AXE_178_MEDIA_SBP			0x0800
#define AXE_178_MEDIA_SUPERMAC			0x1000

#define AXE_RXCMD_PROMISC			0x0001
#define AXE_RXCMD_ALLMULTI			0x0002
#define AXE_172_RXCMD_UNICAST			0x0004
#define AXE_178_RXCMD_KEEP_INVALID_CRC		0x0004
#define AXE_RXCMD_BROADCAST			0x0008
#define AXE_RXCMD_MULTICAST			0x0010
#define AXE_178_RXCMD_AP			0x0020
#define AXE_RXCMD_ENABLE			0x0080
#define AXE_178_RXCMD_MFB			0x0300	/* Max Frame Burst */
#define AXE_178_RXCMD_MFB_2048			0x0000
#define AXE_178_RXCMD_MFB_4096			0x0100
#define AXE_178_RXCMD_MFB_8192			0x0200
#define AXE_178_RXCMD_MFB_16384			0x0300

#define AXE_NOPHY				0xE0
#define AXE_INTPHY				0x10

#define AXE_TIMEOUT		1000
#define AXE_172_BUFSZ		1536
#define AXE_178_MIN_BUFSZ	2048
#define AXE_178_MAX_BUFSZ	16384
#define AXE_MIN_FRAMELEN	60
#define AXE_RX_FRAMES		1
#define AXE_TX_FRAMES		1

#define AXE_RX_LIST_CNT		1
#define AXE_TX_LIST_CNT		1

#define AXE_CTL_READ		0x01
#define AXE_CTL_WRITE		0x02

#define AXE_CONFIG_NO		1
#define AXE_IFACE_IDX		0

/*
 * The interrupt endpoint is currently unused
 * by the ASIX part.
 */
#define AXE_ENDPT_RX		0x0
#define AXE_ENDPT_TX		0x1
#define AXE_ENDPT_INTR		0x2
#define AXE_ENDPT_MAX		0x3

struct axe_sframe_hdr {
	uint16_t	len;
	uint16_t	ilen;
} __packed;

struct axe_type {
	struct usb_devno        axe_dev;
	uint32_t                axe_flags;
#define	AX172	0x0000		/* AX88172 */
#define	AX178	0x0001		/* AX88178 */
#define	AX772	0x0002		/* AX88772 */
};

#define AXE_INC(x, y)		(x) = (x + 1) % y

struct axe_softc {
#if defined(__FreeBSD__)
#define GET_MII(sc) (device_get_softc((sc)->axe_miibus))
#elif defined(__NetBSD__)
#define GET_MII(sc) (&(sc)->axe_mii)
#elif defined(__OpenBSD__)
#define GET_MII(sc) (&(sc)->axe_mii)
#endif
	struct ifnet		*axe_ifp;
	device_t		axe_miibus;
	device_t		axe_dev;
	usbd_device_handle	axe_udev;
	usbd_interface_handle	axe_iface;
	u_int16_t		axe_vendor;
	u_int16_t		axe_product;
	u_int16_t		axe_flags;
	int			axe_ed[AXE_ENDPT_MAX];
	usbd_pipe_handle	axe_ep[AXE_ENDPT_MAX];
	int			axe_if_flags;
	struct ue_cdata		axe_cdata;
	struct callout_handle	axe_stat_ch;
	struct mtx		axe_mtx;
	struct sx		axe_sleeplock;
	char			axe_dying;
	int			axe_link;
	unsigned char		axe_ipgs[3];
	unsigned char 		axe_phyaddrs[2];
	struct timeval		axe_rx_notice;
	struct usb_qdat		axe_qdat;
	struct usb_task		axe_tick_task;
	int			axe_bufsz;
	int			axe_boundary;
};

#if 0
#define	AXE_LOCK(_sc)		mtx_lock(&(_sc)->axe_mtx)
#define	AXE_UNLOCK(_sc)		mtx_unlock(&(_sc)->axe_mtx)
#else
#define	AXE_LOCK(_sc)
#define	AXE_UNLOCK(_sc)
#endif
#define	AXE_SLEEPLOCK(_sc)	sx_xlock(&(_sc)->axe_sleeplock)
#define	AXE_SLEEPUNLOCK(_sc)	sx_xunlock(&(_sc)->axe_sleeplock)
#define	AXE_SLEEPLOCKASSERT(_sc) sx_assert(&(_sc)->axe_sleeplock, SX_XLOCKED)
