/*-
 * Granch SBNI16 G.SHDSL Modem driver definitions
 * Written by Denis I. Timofeev, 2002-2003.
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
 * $FreeBSD: src/sys/dev/sbsh/if_sbshreg.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* SBNI16 controller chip definitions */

/* CR bits */
#define	TXEN	0x01		/* transmitter enable */
#define	RXEN	0x02		/* receiver  enable */
#define	NCRC	0x04		/* ignore received CRC */
#define	DLBK	0x08		/* digital loopback */
#define CMOD	0x10		/* 0 - use CRC-32, 1 - CRC-16 */
#define FMOD	0x20		/* interframe fill: 0 - all ones, 1 - 0xfe */
#define PMOD	0x40		/* data polarity: 0 - normal, 1 - invert */
#define XRST	0x80		/* reset the transceiver */

/* CRB bits */
#define RDBE	0x01		/* read burst enable */
#define WTBE	0x02		/* write burst enable */
#define RODD	0x04		/* receive 2-byte alignment */
#define RXDE	0x08		/* receive data enable */

/* SR and IMR bits */
#define	TXS	0x01		/* transmit success */
#define	RXS	0x02		/* receive success */
/* SR only */
#define	CRC	0x04		/* CRC error */
#define	OFL	0x08		/* fifo overflow error */
#define	UFL	0x10		/* fifo underflow error */
#define	EXT	0x20		/* interrupt from sk70725 */
/* IMR only */
#define	TSI	0x80		/* generate test interrupt */

#define LAST_FRAG	0x00008000

#define SBNI16_MAX_FRAME	(1536 + 16)

/* We don't have official vendor id yet... */
#define SBNI16_VENDOR	0x55
#define SBNI16_DEVICE	0x9d
#define SBNI16_SUBDEV	3

/* transceiver chip set definitions */
enum  CX28975_API_commands {
	_DSL_RESET_SYSTEM	= 0x00,
	_DSL_SYSTEM_ENABLE	= 0x01,
	_DSL_AFE_CONFIG		= 0x02,
	_DSL_TRAINING_MODE	= 0x03,
	_DSL_CLOCK_CONFIG	= 0x04,
	_DSL_PCM_MF_LEN		= 0x05,
	_DSL_SYSTEM_CONFIG	= 0x06,
	_DSL_LOOPBACK		= 0x09,
	_DSL_ACTIVATION		= 0x0b,
	_DSL_FORCE_DEACTIVATE	= 0x0c,
	_DSL_TEST_MODE		= 0x0d,
	_DSL_DATA_RATE		= 0x0e,
	_DSL_PREACTIVATION_CFG	= 0x0f,
	_DSL_FR_PCM_CONFIG	= 0x10,
	_DSL_FR_HDSL_CONFIG	= 0x11,
	_DSL_PCM_CLK_CONF	= 0x12,
	_AFE_TX_GAIN		= 0x13,
	_DSL_PREACT_USER_INFO	= 0x14,
	_DSL_PREACT_RATE_LIST	= 0x15,
	_DSL_TX_ISO_PULSE	= 0x16,
	_BP_ERLE_TEST_MODE	= 0x18,
	_DSL_MULTI_PAIR_CONFIG	= 0x19,
	_DSL_NB_MULTI_RATE_CONFIG	= 0x1a,
	_DSL_MULTI_RATE_CONFIG	= 0x1b,
	_ATM_PHY_MODE		= 0x1c,
	_ATM_PHY_UTOPIA_CONFIG	= 0x1d,
	_ATM_PHY_IF_MODE	= 0x1e,
	_ATM_PHY_INJECT_HEC_ERROR	= 0x1f,
	_ATM_PHY_CONFIG		= 0x20,
	_DSL_TNB_BER_STATE	= 0x21,
	_DSL_RNB_BER_STATE	= 0x22,
	_DSL_TP_BER_STATE	= 0x23,
	_DSL_RP_BER_STATE	= 0x24,
	_DSL_PRBS_CONFIGURE	= 0x25,
	_DSL_CONST_FILL		= 0x26,
	_DSL_DBANK		= 0x27,
	_DSL_NB_CONFIG		= 0x28,
	_DSL_TNB_FRM_OFST	= 0x29,
	_DSL_RNB_FRM_OFST	= 0x2a,
	_DSL_TP_FRM_OFST	= 0x2e,
	_DSL_RP_FRM_OFST	= 0x2f,
	_DSL_TP_MAPPER_VALUE	= 0x30,
	_DSL_TP_MAPPER_WRITE	= 0x31,
	_DSL_RP_MAPPER_VALUE	= 0x32,
	_DSL_RP_MAPPER_WRITE	= 0x33,
	_DSL_TH_MAPPER_VALUE	= 0x34,
	_DSL_TH_MAPPER_WRITE	= 0x35,
	_DSL_RH_MAPPER_VALUE	= 0x36,
	_DSL_RH_MAPPER_WRITE	= 0x37,
	_DSL_TNB_MAPPER_VALUE	= 0x38,
	_DSL_TNB_MAPPER_WRITE	= 0x39,
	_DSL_RNB_MAPPER_VALUE	= 0x3a,
	_DSL_RNB_MAPPER_WRITE	= 0x3b,
	_DSL_CLEAR_ERROR_CTRS	= 0x40,
	_DSL_INJECT_CRC_ERROR	= 0x41,
	_DSL_THRESHOLDS		= 0x43,
	_DSL_FR_SET_STATE_MACHINE	= 0x4a,
	_DSL_FR_TNB_RESET	= 0x4b,
	_DSL_FR_RNB_RESET	= 0x4c,
	_EOC_RESET		= 0x4d,
	_DSL_FR_TX_RESET	= 0x4e,
	_DSL_FR_RX_RESET	= 0x4f,
	_DSL_INTR_HOST_MASK	= 0x50,
	_DSL_INTR_API_SUBMASK	= 0x51,
	_DSL_DOWNLOAD_START	= 0x53,
	_DSL_DOWNLOAD_DATA	= 0x54,
	_DSL_DOWNLOAD_END	= 0x55,
	_DSL_DPLL_CLOCK_GEN	= 0x58,
	_DSL_NB_DPLL_CLOCK_GEN	= 0x59,
	_DSL_WRITE_REG		= 0x75,
	_DSL_WRITE_AFE		= 0x76
};

enum  CX28975_status_commands {
	_DSL_READ_CONTROL	= 0x80,
	_DSL_FAR_END_ATTEN	= 0x82,
	_DSL_NOISE_MARGIN	= 0x83,
	_DSL_STATUS		= 0x85,
	_DSL_PREACT_GET_FE_CAPS	= 0x88,
	_DSL_PREACT_GET_OPT_DATA_RATE	= 0x89,
	_DSL_VERSIONS		= 0x8a,
	_DSL_TP_BER_RESULTS	= 0x8c,
	_DSL_RP_BER_RESULTS	= 0x8d,
	_DSL_STAGE_NUMBER	= 0x8f,
	_DSL_AFE_SETTING	= 0x90,
	_DSL_TNB_BER_RESULTS	= 0x91,
	_DSL_RNB_BER_RESULTS	= 0x92,
	_BP_ERLE_RESULTS	= 0x93,
	_DSL_POWER_BACK_OFF_RESULT	= 0x94,
	_DSL_OPER_ERR_CTRS	= 0x9c,
	_DSL_TIME		= 0x9d,
	_DSL_HDSL_PERF_ERR_CTRS	= 0x9e,
	_DSL_READ_REG		= 0xa0,
	_DSL_READ_AFE		= 0xa1,
	_DSL_SYSTEM_PERF_ERR_CTRS	= 0xa2,
	_DSL_TP_MAPPER_READ	= 0xa3,
	_DSL_RP_MAPPER_READ	= 0xa4,
	_DSL_TH_MAPPER_READ	= 0xa5,
	_DSL_RH_MAPPER_READ	= 0xa6,
	_DSL_TNB_MAPPER_READ	= 0xa7,
	_DSL_RNB_MAPPER_READ	= 0xa8,
	_EOC_RX_GET_STATS	= 0xae,
	_EOC_TX_SEND_COMMAND	= 0xb0,
	_EOC_RX_GET_MSG		= 0xb1,
	_EOC_TX_GET_MSG_STATUS	= 0xb2,
	_EOC_TX_DELETE_MSG	= 0xb3,
	_ATM_PHY_OPER_ERR_CTRS	= 0xb8,
	_ATM_PHY_PERF_ERR_CTRS	= 0xb9,
	_ATM_PHY_CELL_CTRS	= 0xba
};

enum  CX28975_acknoledge_status_codes {
	_ACK_NOT_COMPLETE	= 0x00,
	_ACK_PASS		= 0x01,
	_ACK_BUSY		= 0x02,
	_ACK_NOT_APPLICABLE	= 0x03,
	_ACK_INVALID_DEST	= 0x04,
	_ACK_INVALID_OPCODE	= 0x05,
	_ACK_INVALID_LENGTH	= 0x06,
	_ACK_INVALID_DATA	= 0x07,
	_ACK_INVALID_CHKSUM	= 0x08,
	_ACK_NO_RESULT		= 0x09,
	_ACK_NOT_AVAILABLE	= 0x0a,
	_ACK_BOOT_WAKE_UP	= 0x0d,
	_ACK_OPER_WAKE_UP	= 0x0e
};
