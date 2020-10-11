/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Contact Information :
 * Rajesh Kumar <rajesh1.kumar@amd.com>
 * Arpan Palit <Arpan.Palit@amd.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sbuf.h>

#include "xgbe.h"
#include "xgbe-common.h"

#define SYSCTL_BUF_LEN 64

typedef enum{
	/* Coalesce flag */
	rx_coalesce_usecs = 1,
	rx_max_coalesced_frames,
	rx_coalesce_usecs_irq,
	rx_max_coalesced_frames_irq,
	tx_coalesce_usecs,
	tx_max_coalesced_frames,
	tx_coalesce_usecs_irq,
	tx_max_coalesced_frames_irq,
	stats_block_coalesce_usecs,
	use_adaptive_rx_coalesce,
	use_adaptive_tx_coalesce,
	pkt_rate_low,
	rx_coalesce_usecs_low,
	rx_max_coalesced_frames_low,
	tx_coalesce_usecs_low,
	tx_max_coalesced_frames_low,
	pkt_rate_high,
	rx_coalesce_usecs_high,
	rx_max_coalesced_frames_high,
	tx_coalesce_usecs_high,
	tx_max_coalesced_frames_high,
	rate_sample_interval,

	/* Pasue flag */
	autoneg,
	tx_pause,
	rx_pause,

	/* link settings */
	speed,
	duplex,

	/* Ring settings */
	rx_pending,
	rx_mini_pending,
	rx_jumbo_pending,
	tx_pending,

	/* Channels settings */
	rx_count,
	tx_count,
	other_count,
	combined_count,
} sysctl_variable_t;

typedef enum {
	SYSL_NONE,
	SYSL_BOOL,
	SYSL_S32,
	SYSL_U8,
	SYSL_U16,
	SYSL_U32,
	SYSL_U64,
	SYSL_BE16,
	SYSL_IP4,
	SYSL_STR,
	SYSL_FLAG,
	SYSL_MAC,
} sysctl_type_t;

struct sysctl_info {
	uint8_t name[32];
	sysctl_type_t type;
	sysctl_variable_t flag;
	uint8_t support[16];
};

struct sysctl_op {
	/* Coalesce options */
	unsigned int rx_coalesce_usecs;
	unsigned int rx_max_coalesced_frames;
	unsigned int rx_coalesce_usecs_irq;
	unsigned int rx_max_coalesced_frames_irq;
	unsigned int tx_coalesce_usecs;
	unsigned int tx_max_coalesced_frames;
	unsigned int tx_coalesce_usecs_irq;
	unsigned int tx_max_coalesced_frames_irq;
	unsigned int stats_block_coalesce_usecs;
	unsigned int use_adaptive_rx_coalesce;
	unsigned int use_adaptive_tx_coalesce;
	unsigned int pkt_rate_low;
	unsigned int rx_coalesce_usecs_low;
	unsigned int rx_max_coalesced_frames_low;
	unsigned int tx_coalesce_usecs_low;
	unsigned int tx_max_coalesced_frames_low;
	unsigned int pkt_rate_high;
	unsigned int rx_coalesce_usecs_high;
	unsigned int rx_max_coalesced_frames_high;
	unsigned int tx_coalesce_usecs_high;
	unsigned int tx_max_coalesced_frames_high;
	unsigned int rate_sample_interval;

	/* Pasue options */
	unsigned int autoneg;
	unsigned int tx_pause;
	unsigned int rx_pause;

	/* Link settings options */
	unsigned int speed;
	unsigned int duplex;

	/* Ring param options */
	unsigned int rx_max_pending;
	unsigned int rx_mini_max_pending;
	unsigned int rx_jumbo_max_pending;
	unsigned int tx_max_pending;
	unsigned int rx_pending;
	unsigned int rx_mini_pending;
	unsigned int rx_jumbo_pending;
	unsigned int tx_pending;

	/* Channels options */
	unsigned int max_rx;
	unsigned int max_tx;
	unsigned int max_other;
	unsigned int max_combined;
	unsigned int rx_count;
	unsigned int tx_count;
	unsigned int other_count;
	unsigned int combined_count;
} sys_op;

#define GSTRING_LEN 32

struct xgbe_stats {
	char stat_string[GSTRING_LEN];
	int stat_size;
	int stat_offset;
};

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))

#define XGMAC_MMC_STAT(_string, _var)			   \
	{ _string,					      \
	  FIELD_SIZEOF(struct xgbe_mmc_stats, _var),	    \
	  offsetof(struct xgbe_prv_data, mmc_stats._var),       \
	}

#define XGMAC_EXT_STAT(_string, _var)			   \
	{ _string,					      \
	  FIELD_SIZEOF(struct xgbe_ext_stats, _var),	    \
	  offsetof(struct xgbe_prv_data, ext_stats._var),       \
	}
static const struct xgbe_stats xgbe_gstring_stats[] = {
	XGMAC_MMC_STAT("tx_bytes", txoctetcount_gb),
	XGMAC_MMC_STAT("tx_packets", txframecount_gb),
	XGMAC_MMC_STAT("tx_unicast_packets", txunicastframes_gb),
	XGMAC_MMC_STAT("tx_broadcast_packets", txbroadcastframes_gb),
	XGMAC_MMC_STAT("tx_multicast_packets", txmulticastframes_gb),
	XGMAC_MMC_STAT("tx_vlan_packets", txvlanframes_g),
	XGMAC_EXT_STAT("tx_vxlan_packets", tx_vxlan_packets),
	XGMAC_EXT_STAT("tx_tso_packets", tx_tso_packets),
	XGMAC_MMC_STAT("tx_64_byte_packets", tx64octets_gb),
	XGMAC_MMC_STAT("tx_65_to_127_byte_packets", tx65to127octets_gb),
	XGMAC_MMC_STAT("tx_128_to_255_byte_packets", tx128to255octets_gb),
	XGMAC_MMC_STAT("tx_256_to_511_byte_packets", tx256to511octets_gb),
	XGMAC_MMC_STAT("tx_512_to_1023_byte_packets", tx512to1023octets_gb),
	XGMAC_MMC_STAT("tx_1024_to_max_byte_packets", tx1024tomaxoctets_gb),
	XGMAC_MMC_STAT("tx_underflow_errors", txunderflowerror),
	XGMAC_MMC_STAT("tx_pause_frames", txpauseframes),

	XGMAC_MMC_STAT("rx_bytes", rxoctetcount_gb),
	XGMAC_MMC_STAT("rx_packets", rxframecount_gb),
	XGMAC_MMC_STAT("rx_unicast_packets", rxunicastframes_g),
	XGMAC_MMC_STAT("rx_broadcast_packets", rxbroadcastframes_g),
	XGMAC_MMC_STAT("rx_multicast_packets", rxmulticastframes_g),
	XGMAC_MMC_STAT("rx_vlan_packets", rxvlanframes_gb),
	XGMAC_EXT_STAT("rx_vxlan_packets", rx_vxlan_packets),
	XGMAC_MMC_STAT("rx_64_byte_packets", rx64octets_gb),
	XGMAC_MMC_STAT("rx_65_to_127_byte_packets", rx65to127octets_gb),
	XGMAC_MMC_STAT("rx_128_to_255_byte_packets", rx128to255octets_gb),
	XGMAC_MMC_STAT("rx_256_to_511_byte_packets", rx256to511octets_gb),
	XGMAC_MMC_STAT("rx_512_to_1023_byte_packets", rx512to1023octets_gb),
	XGMAC_MMC_STAT("rx_1024_to_max_byte_packets", rx1024tomaxoctets_gb),
	XGMAC_MMC_STAT("rx_undersize_packets", rxundersize_g),
	XGMAC_MMC_STAT("rx_oversize_packets", rxoversize_g),
	XGMAC_MMC_STAT("rx_crc_errors", rxcrcerror),
	XGMAC_MMC_STAT("rx_crc_errors_small_packets", rxrunterror),
	XGMAC_MMC_STAT("rx_crc_errors_giant_packets", rxjabbererror),
	XGMAC_MMC_STAT("rx_length_errors", rxlengtherror),
	XGMAC_MMC_STAT("rx_out_of_range_errors", rxoutofrangetype),
	XGMAC_MMC_STAT("rx_fifo_overflow_errors", rxfifooverflow),
	XGMAC_MMC_STAT("rx_watchdog_errors", rxwatchdogerror),
	XGMAC_EXT_STAT("rx_csum_errors", rx_csum_errors),
	XGMAC_EXT_STAT("rx_vxlan_csum_errors", rx_vxlan_csum_errors),
	XGMAC_MMC_STAT("rx_pause_frames", rxpauseframes),
	XGMAC_EXT_STAT("rx_split_header_packets", rx_split_header_packets),
	XGMAC_EXT_STAT("rx_buffer_unavailable", rx_buffer_unavailable),
};

#define XGBE_STATS_COUNT	ARRAY_SIZE(xgbe_gstring_stats)

char** alloc_sysctl_buffer(void);
void get_val(char *buf, char **op, char **val, int *n_op);
void fill_data(struct sysctl_op *sys_op, int flag, unsigned int value);

static int
exit_bad_op(void)
{

	printf("SYSCTL: bad command line option (s)\n");
	return(-EINVAL);
}

static inline unsigned
fls_long(unsigned long l)
{

	if (sizeof(l) == 4)
		return (fls(l));
	return (fls64(l));
}

static inline __attribute__((const))
unsigned long __rounddown_pow_of_two(unsigned long n)
{

	return (1UL << (fls_long(n) - 1));
}

static inline int
get_ubuf(struct sysctl_req *req, char *ubuf)
{
	int rc;

	printf("%s: len:0x%li idx:0x%li\n", __func__, req->newlen,
	    req->newidx);
	if (req->newlen >= SYSCTL_BUF_LEN)
		return (-EINVAL);

	rc = SYSCTL_IN(req, ubuf, req->newlen);
	if (rc)
		return (rc);
	ubuf[req->newlen] = '\0';

	return (0);
}

char**
alloc_sysctl_buffer(void)
{
	char **buffer;
	int i;

	buffer = malloc(sizeof(char *)*32, M_AXGBE, M_WAITOK | M_ZERO);
	for(i = 0; i < 32; i++)
		buffer[i] = malloc(sizeof(char)*32, M_AXGBE, M_WAITOK | M_ZERO);

	return (buffer);
}

void
get_val(char *buf, char **op, char **val, int *n_op)
{
	int blen = strlen(buf);
	int count = 0;
	int i, j;

	*n_op = 0;
	for (i = 0; i < blen; i++) {
		count++;
		/* Get sysctl command option */
		for (j = 0; buf[i] != ' '; j++) {
			if (i >= blen)
				break;
			op[*n_op][j] = buf[i++];
		}
		op[*n_op][j+1] = '\0';
		if (i >= strlen(buf))
			goto out;

		/* Get sysctl value*/
		i++;
		for (j = 0; buf[i] != ' '; j++) {
			if (i >= blen)
				break;
			val[*n_op][j] = buf[i++]; 
		}
		val[*n_op][j+1] = '\0';
		if (i >= strlen(buf))
			goto out;

		*n_op = count;
	}

out:
	*n_op = count;
}

void
fill_data(struct sysctl_op *sys_op, int flag, unsigned int value)
{

	switch(flag) {
	case 1:
	sys_op->rx_coalesce_usecs = value;
	break;
	case 2:
	sys_op->rx_max_coalesced_frames = value;
	break;
	case 3:
	sys_op->rx_coalesce_usecs_irq = value;
	break;
	case 4:
	sys_op->rx_max_coalesced_frames_irq = value;
	break;
	case 5:
	sys_op->tx_coalesce_usecs = value;
	break;
	case 6:
	sys_op->tx_max_coalesced_frames = value;
	break;
	case 7:
	sys_op->tx_coalesce_usecs_irq = value;
	break;
	case 8:
	sys_op->tx_max_coalesced_frames_irq = value;
	break;
	case 9:
	sys_op->stats_block_coalesce_usecs = value;
	break;
	case 10:
	sys_op->use_adaptive_rx_coalesce = value;
	break;
	case 11:
	sys_op->use_adaptive_tx_coalesce = value;
	break;
	case 12:
	sys_op->pkt_rate_low = value;
	break;
	case 13:
	sys_op->rx_coalesce_usecs_low = value;
	break;
	case 14:
	sys_op->rx_max_coalesced_frames_low = value;
	break;
	case 15:
	sys_op->tx_coalesce_usecs_low = value;
	break;
	case 16:
	sys_op->tx_max_coalesced_frames_low = value;
	break;
	case 17:
	sys_op->pkt_rate_high = value;
	break;
	case 18:
	sys_op->rx_coalesce_usecs_high = value;
	break;
	case 19:
	sys_op->rx_max_coalesced_frames_high = value;
	break;
	case 20:
	sys_op->tx_coalesce_usecs_high = value;
	break;
	case 21:
	sys_op->tx_max_coalesced_frames_high = value;
	break;
	case 22:
	sys_op->rate_sample_interval = value;
	break;
	case 23:
	sys_op->autoneg = value;
	break;
	case 24:
	sys_op->rx_pause = value;
	break;
	case 25:
	sys_op->tx_pause = value;
	break;
	case 26:
	sys_op->speed = value;
	break;
	case 27:
	sys_op->duplex = value;
	break;
	case 28:
	sys_op->rx_pending = value;
	break;
	case 29:
	sys_op->rx_mini_pending = value;
	break;
	case 30:
	sys_op->rx_jumbo_pending = value;
	break;
	case 31:
	sys_op->tx_pending = value;
	break;
	default:
		printf("Option error\n");
	}
}

static int
parse_generic_sysctl(struct xgbe_prv_data *pdata, char *buf,
    struct sysctl_info *info, unsigned int n_info)
{
	struct sysctl_op *sys_op = pdata->sys_op;
	unsigned int value;
	char **op, **val;
	int n_op = 0;
	int rc = 0;
	int i, idx;

	op = alloc_sysctl_buffer();
	val = alloc_sysctl_buffer();
	get_val(buf, op, val, &n_op);

	for (i = 0; i < n_op; i++) {
		for (idx = 0; idx < n_info; idx++) {
			if (strcmp(info[idx].name, op[i]) == 0) {
				if (strcmp(info[idx].support,
				    "not-supported") == 0){
					axgbe_printf(1, "ignoring not-supported "
					    "option \"%s\"\n", info[idx].name);
					break;
				}
				switch(info[idx].type) {
				case SYSL_BOOL: {
					if (!strcmp(val[i], "on"))
						fill_data(sys_op,
						    info[idx].flag, 1);
					else if (!strcmp(val[i], "off"))
						fill_data(sys_op,
						    info[idx].flag, 0);
					else
						rc = exit_bad_op();
					break;
				}
				case SYSL_S32:
					sscanf(val[i], "%u", &value);
					fill_data(sys_op, info[idx].flag, value);
					break;
				case SYSL_U8:
					if (!strcmp(val[i], "half"))
						fill_data(sys_op,
						    info[idx].flag, DUPLEX_HALF);
					else if (!strcmp(val[i], "full"))
						fill_data(sys_op,
						    info[idx].flag, DUPLEX_FULL);
					else
						exit_bad_op();
				default:
					rc = exit_bad_op();
				}
			}
		}
	}

	for(i = 0; i < 32; i++)
		free(op[i], M_AXGBE);
	free(op, M_AXGBE);

	for(i = 0; i < 32; i++)
		free(val[i], M_AXGBE);
	free(val, M_AXGBE);
	return (rc);
}


static int
sysctl_xgmac_reg_addr_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	unsigned int reg;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		axgbe_printf(2, "READ: %s: sysctl_xgmac_reg: 0x%x\n",  __func__,
		    pdata->sysctl_xgmac_reg);
		sbuf_printf(sb, "\nXGMAC reg_addr:	0x%x\n",
		    pdata->sysctl_xgmac_reg);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &reg);
		axgbe_printf(2, "WRITE: %s: reg: 0x%x\n",  __func__, reg);
		pdata->sysctl_xgmac_reg = reg;
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_get_drv_info_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	struct xgbe_hw_features *hw_feat = &pdata->hw_feat;
	ssize_t buf_size = 64;
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		sbuf_printf(sb, "\ndriver:	%s", XGBE_DRV_NAME);
		sbuf_printf(sb, "\nversion: %s", XGBE_DRV_VERSION);
		sbuf_printf(sb, "\nfirmware-version: %d.%d.%d",
		    XGMAC_GET_BITS(hw_feat->version, MAC_VR, USERVER),
		    XGMAC_GET_BITS(hw_feat->version, MAC_VR, DEVID),
		    XGMAC_GET_BITS(hw_feat->version, MAC_VR, SNPSVER));
		sbuf_printf(sb, "\nbus-info: %04d:%02d:%02d",
		    pdata->pcie_bus, pdata->pcie_device, pdata->pcie_func);

		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	return (-EINVAL);
}

static int
sysctl_get_link_info_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}
		
		sbuf_printf(sb, "\nLink is %s", pdata->phy.link ? "Up" : "Down");
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (0);
	}

	return (-EINVAL);
}

#define COALESCE_SYSCTL_INFO(__coalop)							\
{											\
	{ "adaptive-rx", SYSL_BOOL, use_adaptive_rx_coalesce, "not-supported" },	\
	{ "adaptive-tx", SYSL_BOOL, use_adaptive_tx_coalesce, "not-supported" },	\
	{ "sample-interval", SYSL_S32, rate_sample_interval, "not-supported" },		\
	{ "stats-block-usecs", SYSL_S32, stats_block_coalesce_usecs, "not-supported" },	\
	{ "pkt-rate-low", SYSL_S32, pkt_rate_low, "not-supported" },	  		\
	{ "pkt-rate-high", SYSL_S32, pkt_rate_high, "not-supported" },	  		\
	{ "rx-usecs", SYSL_S32, rx_coalesce_usecs, "supported" },	  		\
	{ "rx-frames", SYSL_S32, rx_max_coalesced_frames, "supported" },	  	\
	{ "rx-usecs-irq", SYSL_S32, rx_coalesce_usecs_irq, "not-supported" },	  	\
	{ "rx-frames-irq", SYSL_S32, rx_max_coalesced_frames_irq, "not-supported" },	\
	{ "tx-usecs", SYSL_S32, tx_coalesce_usecs, "not-supported" },	  		\
	{ "tx-frames", SYSL_S32, tx_max_coalesced_frames, "supported" },	  	\
	{ "tx-usecs-irq", SYSL_S32, tx_coalesce_usecs_irq, "not-supported" },	  	\
	{ "tx-frames-irq", SYSL_S32, tx_max_coalesced_frames_irq, "not-supported" },	\
	{ "rx-usecs-low", SYSL_S32, rx_coalesce_usecs_low, "not-supported" },	  	\
	{ "rx-frames-low", SYSL_S32, rx_max_coalesced_frames_low, "not-supported"},	\
	{ "tx-usecs-low", SYSL_S32, tx_coalesce_usecs_low, "not-supported" },	  	\
	{ "tx-frames-low", SYSL_S32, tx_max_coalesced_frames_low, "not-supported" },	\
	{ "rx-usecs-high", SYSL_S32, rx_coalesce_usecs_high, "not-supported" },	  	\
	{ "rx-frames-high", SYSL_S32, rx_max_coalesced_frames_high, "not-supported" },	\
	{ "tx-usecs-high", SYSL_S32, tx_coalesce_usecs_high, "not-supported" },	  	\
	{ "tx-frames-high", SYSL_S32, tx_max_coalesced_frames_high, "not-supported" },	\
}

static int
sysctl_coalesce_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	struct xgbe_hw_if *hw_if = &pdata->hw_if;
	struct sysctl_op *sys_op = pdata->sys_op;
	struct sysctl_info sysctl_coalesce[] = COALESCE_SYSCTL_INFO(coalop);
	unsigned int rx_frames, rx_riwt, rx_usecs;
	unsigned int tx_frames;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}
		sys_op->rx_coalesce_usecs = pdata->rx_usecs;
		sys_op->rx_max_coalesced_frames = pdata->rx_frames;
		sys_op->tx_max_coalesced_frames = pdata->tx_frames;

		sbuf_printf(sb, "\nAdaptive RX: %s  TX: %s\n",
		    sys_op->use_adaptive_rx_coalesce ? "on" : "off",
		    sys_op->use_adaptive_tx_coalesce ? "on" : "off");

		sbuf_printf(sb, "stats-block-usecs: %u\n"
		    "sample-interval: %u\n"
		    "pkt-rate-low: %u\n"
		    "pkt-rate-high: %u\n"
		    "\n"
		    "rx-usecs: %u\n"
		    "rx-frames: %u\n"
		    "rx-usecs-irq: %u\n"
		    "rx-frames-irq: %u\n"
		    "\n"
		    "tx-usecs: %u\n"
		    "tx-frames: %u\n"
		    "tx-usecs-irq: %u\n"
		    "tx-frames-irq: %u\n"
		    "\n"
		    "rx-usecs-low: %u\n"
		    "rx-frames-low: %u\n"
		    "tx-usecs-low: %u\n"
		    "tx-frames-low: %u\n"
		    "\n"
		    "rx-usecs-high: %u\n"
		    "rx-frames-high: %u\n"
		    "tx-usecs-high: %u\n"
		    "tx-frames-high: %u\n",
		    sys_op->stats_block_coalesce_usecs,
		    sys_op->rate_sample_interval,
		    sys_op->pkt_rate_low,
		    sys_op->pkt_rate_high,

		    sys_op->rx_coalesce_usecs,
		    sys_op->rx_max_coalesced_frames,
		    sys_op->rx_coalesce_usecs_irq,
		    sys_op->rx_max_coalesced_frames_irq,

		    sys_op->tx_coalesce_usecs,
		    sys_op->tx_max_coalesced_frames,
		    sys_op->tx_coalesce_usecs_irq,
		    sys_op->tx_max_coalesced_frames_irq,

		    sys_op->rx_coalesce_usecs_low,
		    sys_op->rx_max_coalesced_frames_low,
		    sys_op->tx_coalesce_usecs_low,
		    sys_op->tx_max_coalesced_frames_low,

		    sys_op->rx_coalesce_usecs_high,
		    sys_op->rx_max_coalesced_frames_high,
		    sys_op->tx_coalesce_usecs_high,
		    sys_op->tx_max_coalesced_frames_high);

		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (0);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		parse_generic_sysctl(pdata, buf, sysctl_coalesce,
		    ARRAY_SIZE(sysctl_coalesce)); 

		rx_riwt = hw_if->usec_to_riwt(pdata, sys_op->rx_coalesce_usecs);
		rx_usecs = sys_op->rx_coalesce_usecs;
		rx_frames = sys_op->rx_max_coalesced_frames;

		/* Use smallest possible value if conversion resulted in zero */
		if (rx_usecs && !rx_riwt)
			rx_riwt = 1;

		/* Check the bounds of values for Rx */
		if (rx_riwt > XGMAC_MAX_DMA_RIWT) {
			axgbe_printf(2, "rx-usec is limited to %d usecs\n",
			    hw_if->riwt_to_usec(pdata, XGMAC_MAX_DMA_RIWT));
			return (-EINVAL);
		}
		if (rx_frames > pdata->rx_desc_count) {
			axgbe_printf(2, "rx-frames is limited to %d frames\n",
			    pdata->rx_desc_count);
			return (-EINVAL);
		}

		tx_frames = sys_op->tx_max_coalesced_frames;

		/* Check the bounds of values for Tx */
		if (tx_frames > pdata->tx_desc_count) {
			axgbe_printf(2, "tx-frames is limited to %d frames\n",
			    pdata->tx_desc_count);
			return (-EINVAL);
		}

		pdata->rx_riwt = rx_riwt;
		pdata->rx_usecs = rx_usecs;
		pdata->rx_frames = rx_frames;
		hw_if->config_rx_coalesce(pdata);

		pdata->tx_frames = tx_frames;
		hw_if->config_tx_coalesce(pdata);
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);

	return (rc);
}

static int
sysctl_pauseparam_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	struct sysctl_op *sys_op = pdata->sys_op;
	struct sysctl_info sysctl_pauseparam[] = {
		{ "autoneg", SYSL_BOOL, autoneg, "supported" },
		{ "rx", SYSL_BOOL, rx_pause, "supported" },
		{ "tx", SYSL_BOOL, tx_pause, "supported" },
	};
	ssize_t buf_size = 512;
	char buf[buf_size];
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}
		sys_op->autoneg = pdata->phy.pause_autoneg;
		sys_op->tx_pause = pdata->phy.tx_pause;
		sys_op->rx_pause = pdata->phy.rx_pause;

		sbuf_printf(sb,
		    "\nAutonegotiate:	%s\n"
		    "RX:		%s\n"
		    "TX:		%s\n",
		    sys_op->autoneg ? "on" : "off",
		    sys_op->rx_pause ? "on" : "off",
		    sys_op->tx_pause ? "on" : "off");

		if (pdata->phy.lp_advertising) {
			int an_rx = 0, an_tx = 0;

			if (pdata->phy.advertising & pdata->phy.lp_advertising &
			    ADVERTISED_Pause) {
				an_tx = 1;
				an_rx = 1;
			} else if (pdata->phy.advertising &
			    pdata->phy.lp_advertising & ADVERTISED_Asym_Pause) {
				if (pdata->phy.advertising & ADVERTISED_Pause)
					an_rx = 1;
				else if (pdata->phy.lp_advertising &
				    ADVERTISED_Pause)
				an_tx = 1;
			}
			sbuf_printf(sb,
			    "\n->\nRX negotiated:	%s\n"
			    "TX negotiated:	%s\n",
			    an_rx ? "on" : "off",
			    an_tx ? "on" : "off");
		}
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (0);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		parse_generic_sysctl(pdata, buf, sysctl_pauseparam,
		    ARRAY_SIZE(sysctl_pauseparam));

		if (sys_op->autoneg && (pdata->phy.autoneg != AUTONEG_ENABLE)) {
			axgbe_error("autoneg disabled, pause autoneg not available\n");
			return (-EINVAL);
		}

		pdata->phy.pause_autoneg = sys_op->autoneg;
		pdata->phy.tx_pause = sys_op->tx_pause;
		pdata->phy.rx_pause = sys_op->rx_pause;

		XGBE_CLR_ADV(&pdata->phy, Pause);
		XGBE_CLR_ADV(&pdata->phy, Asym_Pause);

		if (sys_op->rx_pause) {
			XGBE_SET_ADV(&pdata->phy, Pause);
			XGBE_SET_ADV(&pdata->phy, Asym_Pause);
		}

		if (sys_op->tx_pause) {
			/* Equivalent to XOR of Asym_Pause */
			if (XGBE_ADV(&pdata->phy, Asym_Pause))
				XGBE_CLR_ADV(&pdata->phy, Asym_Pause);
			else
				XGBE_SET_ADV(&pdata->phy, Asym_Pause);
		}

		if (test_bit(XGBE_LINK_INIT, &pdata->dev_state))
			rc = pdata->phy_if.phy_config_aneg(pdata);

	}

	return (rc);
}

static int
sysctl_link_ksettings_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	struct sysctl_op *sys_op = pdata->sys_op;
	struct sysctl_info sysctl_linksettings[] = {
		{ "autoneg", SYSL_BOOL, autoneg, "supported" },
		{ "speed", SYSL_U32, speed, "supported" },
		{ "duplex", SYSL_U8, duplex, "supported" },
	};
	ssize_t buf_size = 512;
	char buf[buf_size], link_modes[16], speed_modes[16];
	struct sbuf *sb;
	uint32_t speed;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}
		sys_op->autoneg = pdata->phy.autoneg;
		sys_op->speed = pdata->phy.speed;
		sys_op->duplex = pdata->phy.duplex;

		XGBE_LM_COPY(&pdata->phy, supported, &pdata->phy, supported);
		XGBE_LM_COPY(&pdata->phy, advertising, &pdata->phy, advertising);
		XGBE_LM_COPY(&pdata->phy, lp_advertising, &pdata->phy, lp_advertising);

		switch (sys_op->speed) {
		case 1:
			strcpy(link_modes, "Unknown");
			strcpy(speed_modes, "Unknown");
			break;
		case 2:
			strcpy(link_modes, "10Gbps/Full");
			strcpy(speed_modes, "10000");
			break;
		case 3: 
			strcpy(link_modes, "2.5Gbps/Full");
			strcpy(speed_modes, "2500");
			break;
		case 4: 
			strcpy(link_modes, "1Gbps/Full");
			strcpy(speed_modes, "1000");
			break;
		case 5: 
			strcpy(link_modes, "100Mbps/Full");
			strcpy(speed_modes, "100");
			break;
		case 6:
			strcpy(link_modes, "10Mbps/Full");
			strcpy(speed_modes, "10");
			break;
		}
			
		sbuf_printf(sb,
		    "\nlink_modes: %s\n"
		    "autonegotiation: %s\n"
		    "speed: %sMbps\n",
		    link_modes,
		    (sys_op->autoneg == AUTONEG_DISABLE) ? "off" : "on",
		    speed_modes);

		switch (sys_op->duplex) {
			case DUPLEX_HALF:
				sbuf_printf(sb, "Duplex: Half\n");
				break;
			case DUPLEX_FULL:
				sbuf_printf(sb, "Duplex: Full\n");
				break;
			default:
				sbuf_printf(sb, "Duplex: Unknown\n");
				break;
		}
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (0);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		parse_generic_sysctl(pdata, buf, sysctl_linksettings,
		    ARRAY_SIZE(sysctl_linksettings));

		speed = sys_op->speed;

		if ((sys_op->autoneg != AUTONEG_ENABLE) &&
		    (sys_op->autoneg != AUTONEG_DISABLE)) {
			axgbe_error("unsupported autoneg %hhu\n",
			    (unsigned char)sys_op->autoneg);
			return (-EINVAL);
		}

		if (sys_op->autoneg == AUTONEG_DISABLE) {
			if (!pdata->phy_if.phy_valid_speed(pdata, speed)) {
				axgbe_error("unsupported speed %u\n", speed);
				return (-EINVAL);
			}

			if (sys_op->duplex != DUPLEX_FULL) {
				axgbe_error("unsupported duplex %hhu\n",
				    (unsigned char)sys_op->duplex);
				return (-EINVAL);
			}
		}

		pdata->phy.autoneg = sys_op->autoneg;
		pdata->phy.speed = speed;
		pdata->phy.duplex = sys_op->duplex;

		if (sys_op->autoneg == AUTONEG_ENABLE)
			XGBE_SET_ADV(&pdata->phy, Autoneg);
		else
			XGBE_CLR_ADV(&pdata->phy, Autoneg);

		if (test_bit(XGBE_LINK_INIT, &pdata->dev_state))
			rc = pdata->phy_if.phy_config_aneg(pdata);
	}

	return (rc);
}

static int
sysctl_ringparam_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	struct sysctl_op *sys_op = pdata->sys_op;
	struct sysctl_info sysctl_ringparam[] = {
		{ "rx", SYSL_S32, rx_pending, "supported" },
		{ "rx-mini", SYSL_S32, rx_mini_pending, "supported" },
		{ "rx-jumbo", SYSL_S32, rx_jumbo_pending, "supported" },
		{ "tx", SYSL_S32, tx_pending, "supported" },
	};
	ssize_t buf_size = 512;
	unsigned int rx, tx;
	char buf[buf_size];
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}
		sys_op->rx_max_pending = XGBE_RX_DESC_CNT_MAX;
		sys_op->tx_max_pending = XGBE_TX_DESC_CNT_MAX;
		sys_op->rx_pending = pdata->rx_desc_count;
		sys_op->tx_pending = pdata->tx_desc_count;

		sbuf_printf(sb,
		    "\nPre-set maximums:\n"
		    "RX:		%u\n"
		    "RX Mini:	%u\n"
		    "RX Jumbo:	%u\n"
		    "TX:		%u\n",
		    sys_op->rx_max_pending,
		    sys_op->rx_mini_max_pending,
		    sys_op->rx_jumbo_max_pending,
		    sys_op->tx_max_pending);

		sbuf_printf(sb,
		    "\nCurrent hardware settings:\n"
		    "RX:		%u\n"
		    "RX Mini:	%u\n"
		    "RX Jumbo:	%u\n"
		    "TX:		%u\n",
		    sys_op->rx_pending,
		    sys_op->rx_mini_pending,
		    sys_op->rx_jumbo_pending,
		    sys_op->tx_pending);

		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (0);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		parse_generic_sysctl(pdata, buf, sysctl_ringparam,
		    ARRAY_SIZE(sysctl_ringparam));

		if (sys_op->rx_mini_pending || sys_op->rx_jumbo_pending) {
			axgbe_error("unsupported ring parameter\n");
			return (-EINVAL);
		}

		if ((sys_op->rx_pending < XGBE_RX_DESC_CNT_MIN) ||
				(sys_op->rx_pending > XGBE_RX_DESC_CNT_MAX)) {
			axgbe_error("rx ring param must be between %u and %u\n",
			    XGBE_RX_DESC_CNT_MIN, XGBE_RX_DESC_CNT_MAX);
			return (-EINVAL);
		}

		if ((sys_op->tx_pending < XGBE_TX_DESC_CNT_MIN) ||
				(sys_op->tx_pending > XGBE_TX_DESC_CNT_MAX)) {
			axgbe_error("tx ring param must be between %u and %u\n",
			    XGBE_TX_DESC_CNT_MIN, XGBE_TX_DESC_CNT_MAX);
			return (-EINVAL);
		}

		rx = __rounddown_pow_of_two(sys_op->rx_pending);
		if (rx != sys_op->rx_pending)
			axgbe_printf(1,	"rx ring param rounded to power of 2: %u\n",
			    rx);

		tx = __rounddown_pow_of_two(sys_op->tx_pending);
		if (tx != sys_op->tx_pending)
			axgbe_printf(1, "tx ring param rounded to power of 2: %u\n",
			    tx);

		if ((rx == pdata->rx_desc_count) &&
		    (tx == pdata->tx_desc_count))
			goto out;

		pdata->rx_desc_count = rx;
		pdata->tx_desc_count = tx;

		/* TODO - restart dev */
	}

out:
	return (0);
}

static int
sysctl_channels_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	struct sysctl_op *sys_op = pdata->sys_op;
	struct sysctl_info sysctl_channels[] = {
		{ "rx", SYSL_S32, rx_count, "supported" },
		{ "tx", SYSL_S32, tx_count, "supported" },
		{ "other", SYSL_S32, other_count, "supported" },
		{ "combined", SYSL_S32, combined_count, "supported" },
	};
	unsigned int rx, tx, combined;
	ssize_t buf_size = 512;
	char buf[buf_size];
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}
		rx = min(pdata->hw_feat.rx_ch_cnt, pdata->rx_max_channel_count);
		rx = min(rx, pdata->channel_irq_count);
		tx = min(pdata->hw_feat.tx_ch_cnt, pdata->tx_max_channel_count);
		tx = min(tx, pdata->channel_irq_count);
		tx = min(tx, pdata->tx_max_q_count);

		combined = min(rx, tx);

		sys_op->max_combined = combined;
		sys_op->max_rx = rx ? rx - 1 : 0;
		sys_op->max_tx = tx ? tx - 1 : 0;

		/* Get current settings based on device state */
		rx = pdata->rx_ring_count;
		tx = pdata->tx_ring_count;

		combined = min(rx, tx);
		rx -= combined;
		tx -= combined;

		sys_op->combined_count = combined;
		sys_op->rx_count = rx;
		sys_op->tx_count = tx;

		sbuf_printf(sb,
		    "\nPre-set maximums:\n"
		    "RX:		%u\n"
		    "TX:		%u\n"
		    "Other:		%u\n"
		    "Combined:	%u\n",
		    sys_op->max_rx, sys_op->max_tx,
		    sys_op->max_other,
		    sys_op->max_combined);

		sbuf_printf(sb,
		    "\nCurrent hardware settings:\n"
		    "RX:		%u\n"
		    "TX:		%u\n"
		    "Other:		%u\n"
		    "Combined:	%u\n",
		    sys_op->rx_count, sys_op->tx_count,
		    sys_op->other_count,
		    sys_op->combined_count);

		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (0);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		parse_generic_sysctl(pdata, buf, sysctl_channels,
		    ARRAY_SIZE(sysctl_channels));

		axgbe_error( "channel inputs: combined=%u, rx-only=%u,"
		    " tx-only=%u\n", sys_op->combined_count,
		    sys_op->rx_count, sys_op->tx_count);
	}

	return (rc);
}


static int
sysctl_mac_stats_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	struct sbuf *sb;
	int rc = 0;
	int i;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		pdata->hw_if.read_mmc_stats(pdata);
		for (i = 0; i < XGBE_STATS_COUNT; i++) {
		sbuf_printf(sb, "\n %s: %lu",
		    xgbe_gstring_stats[i].stat_string,
		    *(uint64_t *)((uint8_t *)pdata + xgbe_gstring_stats[i].stat_offset));
		}
		for (i = 0; i < pdata->tx_ring_count; i++) {
			sbuf_printf(sb,
			    "\n txq_packets[%d]: %lu"
			    "\n txq_bytes[%d]: %lu",
			    i, pdata->ext_stats.txq_packets[i],
			    i, pdata->ext_stats.txq_bytes[i]);
		}
		for (i = 0; i < pdata->rx_ring_count; i++) {
			sbuf_printf(sb,
			    "\n rxq_packets[%d]: %lu"
			    "\n rxq_bytes[%d]: %lu",
			    i, pdata->ext_stats.rxq_packets[i],
			    i, pdata->ext_stats.rxq_bytes[i]);
		}

		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	return (-EINVAL);
}

static int
sysctl_xgmac_reg_value_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	unsigned int value;
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		value = XGMAC_IOREAD(pdata, pdata->sysctl_xgmac_reg);
		axgbe_printf(2, "READ: %s: value: 0x%x\n",  __func__, value);
		sbuf_printf(sb, "\nXGMAC reg_value:	0x%x\n", value);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &value);
		axgbe_printf(2, "WRITE: %s: value: 0x%x\n",  __func__, value);
		XGMAC_IOWRITE(pdata, pdata->sysctl_xgmac_reg, value);
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_xpcs_mmd_reg_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	unsigned int reg;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		axgbe_printf(2, "READ: %s: xpcs_mmd: 0x%x\n",  __func__,
		    pdata->sysctl_xpcs_mmd);
		sbuf_printf(sb, "\nXPCS mmd_reg:	0x%x\n",
		    pdata->sysctl_xpcs_mmd);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &reg);
		axgbe_printf(2, "WRITE: %s: mmd_reg: 0x%x\n",  __func__, reg);
		pdata->sysctl_xpcs_mmd = reg;
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_xpcs_reg_addr_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	unsigned int reg;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		axgbe_printf(2, "READ: %s: sysctl_xpcs_reg: 0x%x\n",  __func__,
		    pdata->sysctl_xpcs_reg);
		sbuf_printf(sb, "\nXPCS reg_addr:	0x%x\n",
		    pdata->sysctl_xpcs_reg);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &reg);
		axgbe_printf(2, "WRITE: %s: reg: 0x%x\n",  __func__, reg);
		pdata->sysctl_xpcs_reg = reg;
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_xpcs_reg_value_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	unsigned int value;
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		value = XMDIO_READ(pdata, pdata->sysctl_xpcs_mmd,
		    pdata->sysctl_xpcs_reg);
		axgbe_printf(2, "READ: %s: value: 0x%x\n",  __func__, value);
		sbuf_printf(sb, "\nXPCS reg_value:	0x%x\n", value);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &value);
		axgbe_printf(2, "WRITE: %s: value: 0x%x\n",  __func__, value);
		XMDIO_WRITE(pdata, pdata->sysctl_xpcs_mmd,
		    pdata->sysctl_xpcs_reg, value);
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_xprop_reg_addr_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	unsigned int reg;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		axgbe_printf(2, "READ: %s: sysctl_xprop_reg: 0x%x\n",  __func__,
		    pdata->sysctl_xprop_reg);
		sbuf_printf(sb, "\nXPROP reg_addr:	0x%x\n",
		    pdata->sysctl_xprop_reg);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &reg);
		axgbe_printf(2, "WRITE: %s: reg: 0x%x\n",  __func__, reg);
		pdata->sysctl_xprop_reg = reg;
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_xprop_reg_value_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	unsigned int value;
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		value = XP_IOREAD(pdata, pdata->sysctl_xprop_reg);
		axgbe_printf(2, "READ: %s: value: 0x%x\n",  __func__, value);
		sbuf_printf(sb, "\nXPROP reg_value:	0x%x\n", value);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &value);
		axgbe_printf(2, "WRITE: %s: value: 0x%x\n",  __func__, value);
		XP_IOWRITE(pdata, pdata->sysctl_xprop_reg, value);
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_xi2c_reg_addr_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	unsigned int reg;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		axgbe_printf(2, "READ: %s: sysctl_xi2c_reg: 0x%x\n",  __func__,
		    pdata->sysctl_xi2c_reg);
		sbuf_printf(sb, "\nXI2C reg_addr:	0x%x\n",
		    pdata->sysctl_xi2c_reg);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &reg);
		axgbe_printf(2, "WRITE: %s: reg: 0x%x\n",  __func__, reg);
		pdata->sysctl_xi2c_reg = reg;
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_xi2c_reg_value_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	ssize_t buf_size = 64;
	char buf[buf_size];
	unsigned int value;
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		value = XI2C_IOREAD(pdata, pdata->sysctl_xi2c_reg);
		axgbe_printf(2, "READ: %s: value: 0x%x\n",  __func__, value);
		sbuf_printf(sb, "\nXI2C reg_value:	0x%x\n", value);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%x", &value);
		axgbe_printf(2, "WRITE: %s: value: 0x%x\n",  __func__, value);
		XI2C_IOWRITE(pdata, pdata->sysctl_xi2c_reg, value);
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_an_cdr_wr_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	unsigned int an_cdr_wr = 0;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		axgbe_printf(2, "READ: %s: an_cdr_wr: %d\n",  __func__,
		    pdata->sysctl_an_cdr_workaround);
		sbuf_printf(sb, "%d\n", pdata->sysctl_an_cdr_workaround);
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%u", &an_cdr_wr);
		axgbe_printf(2, "WRITE: %s: an_cdr_wr: 0x%d\n",  __func__,
		    an_cdr_wr);

		if (an_cdr_wr)
			pdata->sysctl_an_cdr_workaround = 1;
		else
			pdata->sysctl_an_cdr_workaround = 0;
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

static int
sysctl_an_cdr_track_early_handler(SYSCTL_HANDLER_ARGS)
{
	struct xgbe_prv_data *pdata = (struct xgbe_prv_data *)arg1;
	unsigned int an_cdr_track_early = 0;
	ssize_t buf_size = 64;
	char buf[buf_size];
	struct sbuf *sb;
	int rc = 0;

	if (req->newptr == NULL) {
		sb = sbuf_new_for_sysctl(NULL, NULL, buf_size, req);
		if (sb == NULL) {
			rc = sb->s_error;
			return (rc);
		}

		axgbe_printf(2, "READ: %s: an_cdr_track_early %d\n",  __func__,
		    pdata->sysctl_an_cdr_track_early);
		sbuf_printf(sb, "%d\n", pdata->sysctl_an_cdr_track_early);			
		rc = sbuf_finish(sb);
		sbuf_delete(sb);
		return (rc);
	}

	rc = get_ubuf(req, buf);
	if (rc == 0) {
		sscanf(buf, "%u", &an_cdr_track_early);
		axgbe_printf(2, "WRITE: %s: an_cdr_track_early: %d\n",  __func__,
		    an_cdr_track_early);

		if (an_cdr_track_early)
			pdata->sysctl_an_cdr_track_early = 1;
		else
			pdata->sysctl_an_cdr_track_early = 0;
	}

	axgbe_printf(2, "%s: rc= %d\n",  __func__, rc);
	return (rc);
}

void
axgbe_sysctl_exit(struct xgbe_prv_data *pdata)
{

	if (pdata->sys_op)
		free(pdata->sys_op, M_AXGBE);
}

void 
axgbe_sysctl_init(struct xgbe_prv_data *pdata)
{
	struct sysctl_ctx_list *clist;
	struct sysctl_oid_list *top;
	struct sysctl_oid *parent; 
	struct sysctl_op *sys_op;

	sys_op = malloc(sizeof(*sys_op), M_AXGBE, M_WAITOK | M_ZERO);
	pdata->sys_op = sys_op;

	clist = device_get_sysctl_ctx(pdata->dev); 
	parent = device_get_sysctl_tree(pdata->dev);
	top = SYSCTL_CHILDREN(parent);

	/* Set defaults */
	pdata->sysctl_xgmac_reg = 0;
	pdata->sysctl_xpcs_mmd = 1;
	pdata->sysctl_xpcs_reg = 0;

	SYSCTL_ADD_UINT(clist, top, OID_AUTO, "axgbe_debug_level", CTLFLAG_RWTUN,
	    &pdata->debug_level, 0, "axgbe log level -- higher is verbose");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xgmac_register",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_xgmac_reg_addr_handler, "IU",
	    "xgmac register addr");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xgmac_register_value",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_xgmac_reg_value_handler, "IU",
	    "xgmac register value");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xpcs_mmd",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_xpcs_mmd_reg_handler, "IU", "xpcs mmd register");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xpcs_register",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_xpcs_reg_addr_handler, "IU", "xpcs register");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xpcs_register_value",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_xpcs_reg_value_handler, "IU",
	    "xpcs register value");

	if (pdata->xpcs_res) {
		SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xprop_register",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    pdata, 0, sysctl_xprop_reg_addr_handler,
		    "IU", "xprop register");

		SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xprop_register_value",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    pdata, 0, sysctl_xprop_reg_value_handler,
		    "IU", "xprop register value");
	}

	if (pdata->xpcs_res) {
		SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xi2c_register",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    pdata, 0, sysctl_xi2c_reg_addr_handler,
		    "IU", "xi2c register");

		SYSCTL_ADD_PROC(clist, top, OID_AUTO, "xi2c_register_value",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    pdata, 0, sysctl_xi2c_reg_value_handler,
		    "IU", "xi2c register value");
	}

	if (pdata->vdata->an_cdr_workaround) {
		SYSCTL_ADD_PROC(clist, top, OID_AUTO, "an_cdr_workaround",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    pdata, 0, sysctl_an_cdr_wr_handler, "IU",
		    "an cdr workaround");

		SYSCTL_ADD_PROC(clist, top, OID_AUTO, "an_cdr_track_early",
		    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
		    pdata, 0, sysctl_an_cdr_track_early_handler, "IU",
		    "an cdr track early");
	}

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "drv_info",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_get_drv_info_handler, "IU",
	    "xgbe drv info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "link_info",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_get_link_info_handler, "IU",
	    "xgbe link info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "coalesce_info",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_coalesce_handler, "IU",
	    "xgbe coalesce info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "pauseparam_info",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_pauseparam_handler, "IU",
	    "xgbe pauseparam info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "link_ksettings_info",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_link_ksettings_handler, "IU",
	    "xgbe link_ksettings info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "ringparam_info",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_ringparam_handler, "IU",
	    "xgbe ringparam info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "channels_info",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_channels_handler, "IU",
	    "xgbe channels info");

	SYSCTL_ADD_PROC(clist, top, OID_AUTO, "mac_stats",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    pdata, 0, sysctl_mac_stats_handler, "IU",
	    "xgbe mac stats");
}
