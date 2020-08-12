/*-
 * Copyright (c) 2014, Alexander V. Chernikov
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/sff8436.h>
#include <net/sff8472.h>

#include <math.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libifconfig.h>
#include <libifconfig_internal.h>
#include <libifconfig_sfp.h>
#include <libifconfig_sfp_tables_internal.h>

#define     SFF_8636_EXT_COMPLIANCE 0x80

struct i2c_info {
	struct ifreq ifr;
	ifconfig_handle_t *h;
	int error;		/* Store first error */
	enum sfp_id id;		/* Module type */
};

static uint8_t
find_zero_bit(const struct sfp_enum_metadata *table, int value, int sz)
{
	int v, m;

	for (v = 1, m = 1 << (8 * sz); v < m; v <<= 1) {
		if ((value & v) == 0)
			continue;
		if (find_metadata(table, value & v) != NULL) {
			return (value & v);
		}
	}
	return (0);
}

/*
 * Reads i2c data from opened kernel socket.
 */
static int
read_i2c(struct i2c_info *ii, uint8_t addr, uint8_t off, uint8_t len,
    uint8_t *buf)
{
	struct ifi2creq req;
	int i, l;

	if (ii->error != 0)
		return (ii->error);

	ii->ifr.ifr_data = (caddr_t)&req;

	i = 0;
	l = 0;
	memset(&req, 0, sizeof(req));
	req.dev_addr = addr;
	req.offset = off;
	req.len = len;

	while (len > 0) {
		l = MIN(sizeof(req.data), len);
		req.len = l;
		if (ifconfig_ioctlwrap(ii->h, AF_LOCAL, SIOCGI2C,
		    &ii->ifr) != 0) {
			ii->error = errno;
			return (errno);
		}

		memcpy(&buf[i], req.data, l);
		len -= l;
		i += l;
		req.offset += l;
	}

	return (0);
}

static int
i2c_info_init(struct i2c_info *ii, ifconfig_handle_t *h, const char *name)
{
	uint8_t id_byte;

	memset(ii, 0, sizeof(*ii));
	strlcpy(ii->ifr.ifr_name, name, sizeof(ii->ifr.ifr_name));
	ii->h = h;

	/*
	 * Try to read byte 0 from i2c:
	 * Both SFF-8472 and SFF-8436 use it as
	 * 'identification byte'.
	 * Stop reading status on zero as value -
	 * this might happen in case of empty transceiver slot.
	 */
	id_byte = 0;
	read_i2c(ii, SFF_8472_BASE, SFF_8472_ID, 1, &id_byte);
	if (ii->error != 0)
		return (-1);
	if (id_byte == 0) {
		h->error.errtype = OTHER;
		h->error.errcode = ENOENT;
		return (-1);
	}
	ii->id = id_byte;
	return (0);
}

static int
get_sfp_info(struct i2c_info *ii, struct ifconfig_sfp_info *sfp)
{
	uint8_t code;

	read_i2c(ii, SFF_8472_BASE, SFF_8472_ID, 1, &sfp->sfp_id);
	read_i2c(ii, SFF_8472_BASE, SFF_8472_CONNECTOR, 1, &sfp->sfp_conn);

	/* Use extended compliance code if it's valid */
	read_i2c(ii, SFF_8472_BASE, SFF_8472_TRANS, 1, &sfp->sfp_eth_ext);
	if (sfp->sfp_eth_ext == 0) {
		/* Next, check 10G Ethernet/IB CCs */
		read_i2c(ii, SFF_8472_BASE, SFF_8472_TRANS_START, 1, &code);
		sfp->sfp_eth_10g = find_zero_bit(sfp_eth_10g_table, code, 1);
		if (sfp->sfp_eth_10g == 0) {
			/* No match. Try Ethernet 1G */
			read_i2c(ii, SFF_8472_BASE, SFF_8472_TRANS_START + 3,
			    1, &code);
			sfp->sfp_eth = find_zero_bit(sfp_eth_table, code, 1);
		}
	}

	return (ii->error);
}

static int
get_qsfp_info(struct i2c_info *ii, struct ifconfig_sfp_info *sfp)
{
	uint8_t code;

	read_i2c(ii, SFF_8436_BASE, SFF_8436_ID, 1, &sfp->sfp_id);
	read_i2c(ii, SFF_8436_BASE, SFF_8436_CONNECTOR, 1, &sfp->sfp_conn);

	read_i2c(ii, SFF_8436_BASE, SFF_8436_STATUS, 1, &sfp->sfp_rev);

	/* Check for extended specification compliance */
	read_i2c(ii, SFF_8436_BASE, SFF_8436_CODE_E1040100G, 1, &code);
	if (code & SFF_8636_EXT_COMPLIANCE) {
		read_i2c(ii, SFF_8436_BASE, SFF_8436_OPTIONS_START, 1,
		    &sfp->sfp_eth_ext);
	} else {
		/* Check 10/40G Ethernet class only */
		sfp->sfp_eth_1040g =
		    find_zero_bit(sfp_eth_1040g_table, code, 1);
	}

	return (ii->error);
}

int
ifconfig_sfp_get_sfp_info(ifconfig_handle_t *h,
    const char *name, struct ifconfig_sfp_info *sfp)
{
	struct i2c_info ii;
	char buf[8];

	memset(sfp, 0, sizeof(*sfp));

	if (i2c_info_init(&ii, h, name) != 0)
		return (-1);

	/* Read bytes 3-10 at once */
	read_i2c(&ii, SFF_8472_BASE, SFF_8472_TRANS_START, 8, buf);
	if (ii.error != 0)
		return (ii.error);

	/* Check 10G ethernet first */
	sfp->sfp_eth_10g = find_zero_bit(sfp_eth_10g_table, buf[0], 1);
	if (sfp->sfp_eth_10g == 0) {
		/* No match. Try 1G */
		sfp->sfp_eth = find_zero_bit(sfp_eth_table, buf[3], 1);
	}
	sfp->sfp_fc_len = find_zero_bit(sfp_fc_len_table, buf[4], 1);
	sfp->sfp_fc_media = find_zero_bit(sfp_fc_media_table, buf[6], 1);
	sfp->sfp_fc_speed = find_zero_bit(sfp_fc_speed_table, buf[7], 1);
	sfp->sfp_cab_tech =
	    find_zero_bit(sfp_cab_tech_table, (buf[4] << 8) | buf[5], 2);

	if (ifconfig_sfp_id_is_qsfp(ii.id))
		return (get_qsfp_info(&ii, sfp));
	return (get_sfp_info(&ii, sfp));
}

static size_t
channel_count(enum sfp_id id)
{
	/* TODO: other ids */
	switch (id) {
	case SFP_ID_UNKNOWN:
		return (0);
	case SFP_ID_QSFP:
	case SFP_ID_QSFPPLUS:
	case SFP_ID_QSFP28:
		return (4);
	default:
		return (1);
	}
}

size_t
ifconfig_sfp_channel_count(const struct ifconfig_sfp_info *sfp)
{
	return (channel_count(sfp->sfp_id));
}

/*
 * Print SFF-8472/SFF-8436 string to supplied buffer.
 * All (vendor-specific) strings are padded right with '0x20'.
 */
static void
get_sff_string(struct i2c_info *ii, uint8_t addr, uint8_t off, char *dst)
{
	read_i2c(ii, addr, off, SFF_VENDOR_STRING_SIZE, dst);
	dst += SFF_VENDOR_STRING_SIZE;
	do { *dst-- = '\0'; } while (*dst == 0x20);
}

static void
get_sff_date(struct i2c_info *ii, uint8_t addr, uint8_t off, char *dst)
{
	char buf[SFF_VENDOR_DATE_SIZE];

	read_i2c(ii, addr, off, SFF_VENDOR_DATE_SIZE, buf);
	sprintf(dst, "20%c%c-%c%c-%c%c", buf[0], buf[1], buf[2], buf[3],
	    buf[4], buf[5]);
}

static int
get_sfp_vendor_info(struct i2c_info *ii, struct ifconfig_sfp_vendor_info *vi)
{
	get_sff_string(ii, SFF_8472_BASE, SFF_8472_VENDOR_START, vi->name);
	get_sff_string(ii, SFF_8472_BASE, SFF_8472_PN_START, vi->pn);
	get_sff_string(ii, SFF_8472_BASE, SFF_8472_SN_START, vi->sn);
	get_sff_date(ii, SFF_8472_BASE, SFF_8472_DATE_START, vi->date);
	return (ii->error);
}

static int
get_qsfp_vendor_info(struct i2c_info *ii, struct ifconfig_sfp_vendor_info *vi)
{
	get_sff_string(ii, SFF_8436_BASE, SFF_8436_VENDOR_START, vi->name);
	get_sff_string(ii, SFF_8436_BASE, SFF_8436_PN_START, vi->pn);
	get_sff_string(ii, SFF_8436_BASE, SFF_8436_SN_START, vi->sn);
	get_sff_date(ii, SFF_8436_BASE, SFF_8436_DATE_START, vi->date);
	return (ii->error);
}

int
ifconfig_sfp_get_sfp_vendor_info(ifconfig_handle_t *h,
    const char *name, struct ifconfig_sfp_vendor_info *vi)
{
	struct i2c_info ii;

	memset(vi, 0, sizeof(*vi));

	if (i2c_info_init(&ii, h, name) != 0)
		return (-1);

	if (ifconfig_sfp_id_is_qsfp(ii.id))
		return (get_qsfp_vendor_info(&ii, vi));
	return (get_sfp_vendor_info(&ii, vi));
}

/*
 * Converts internal temperature (SFF-8472, SFF-8436)
 * 16-bit unsigned value to human-readable representation:
 *
 * Internally measured Module temperature are represented
 * as a 16-bit signed twos complement value in increments of
 * 1/256 degrees Celsius, yielding a total range of –128C to +128C
 * that is considered valid between –40 and +125C.
 */
static double
get_sff_temp(struct i2c_info *ii, uint8_t addr, uint8_t off)
{
	double d;
	uint8_t buf[2];

	read_i2c(ii, addr, off, 2, buf);
	d = (double)buf[0];
	d += (double)buf[1] / 256;
	return (d);
}

/*
 * Retrieves supplied voltage (SFF-8472, SFF-8436).
 * 16-bit usigned value, treated as range 0..+6.55 Volts
 */
static double
get_sff_voltage(struct i2c_info *ii, uint8_t addr, uint8_t off)
{
	double d;
	uint8_t buf[2];

	read_i2c(ii, addr, off, 2, buf);
	d = (double)((buf[0] << 8) | buf[1]);
	return (d / 10000);
}

/*
 * The following conversions assume internally-calibrated data.
 * This is always true for SFF-8346, and explicitly checked for SFF-8472.
 */

double
power_mW(uint16_t power)
{
	/* Power is specified in units of 0.1 uW. */
	return (1.0 * power / 10000);
}

double
power_dBm(uint16_t power)
{
	return (10.0 * log10(power_mW(power)));
}

double
bias_mA(uint16_t bias)
{
	/* Bias current is specified in units of 2 uA. */
	return (1.0 * bias / 500);
}

static uint16_t
get_sff_channel(struct i2c_info *ii, uint8_t addr, uint8_t off)
{
	uint8_t buf[2];

	read_i2c(ii, addr, off, 2, buf);
	if (ii->error != 0)
		return (0);

	return ((buf[0] << 8) + buf[1]);
}

static int
get_sfp_status(struct i2c_info *ii, struct ifconfig_sfp_status *ss)
{
	uint8_t diag_type, flags;

	/* Read diagnostic monitoring type */
	read_i2c(ii, SFF_8472_BASE, SFF_8472_DIAG_TYPE, 1, (caddr_t)&diag_type);
	if (ii->error != 0)
		return (-1);

	/*
	 * Read monitoring data IFF it is supplied AND is
	 * internally calibrated
	 */
	flags = SFF_8472_DDM_DONE | SFF_8472_DDM_INTERNAL;
	if ((diag_type & flags) != flags) {
		ii->h->error.errtype = OTHER;
		ii->h->error.errcode = ENXIO;
		return (-1);
	}

	ss->temp = get_sff_temp(ii, SFF_8472_DIAG, SFF_8472_TEMP);
	ss->voltage = get_sff_voltage(ii, SFF_8472_DIAG, SFF_8472_VCC);
	ss->channel = calloc(channel_count(ii->id), sizeof(*ss->channel));
	if (ss->channel == NULL) {
		ii->h->error.errtype = OTHER;
		ii->h->error.errcode = ENOMEM;
		return (-1);
	}
	ss->channel[0].rx = get_sff_channel(ii, SFF_8472_DIAG, SFF_8472_RX_POWER);
	ss->channel[0].tx = get_sff_channel(ii, SFF_8472_DIAG, SFF_8472_TX_BIAS);
	return (ii->error);
}

static uint32_t
get_qsfp_bitrate(struct i2c_info *ii)
{
	uint8_t code;
	uint32_t rate;

	code = 0;
	read_i2c(ii, SFF_8436_BASE, SFF_8436_BITRATE, 1, &code);
	rate = code * 100;
	if (code == 0xFF) {
		read_i2c(ii, SFF_8436_BASE, SFF_8636_BITRATE, 1, &code);
		rate = code * 250;
	}

	return (rate);
}

static int
get_qsfp_status(struct i2c_info *ii, struct ifconfig_sfp_status *ss)
{
	size_t channels;

	ss->temp = get_sff_temp(ii, SFF_8436_BASE, SFF_8436_TEMP);
	ss->voltage = get_sff_voltage(ii, SFF_8436_BASE, SFF_8436_VCC);
	channels = channel_count(ii->id);
	ss->channel = calloc(channels, sizeof(*ss->channel));
	if (ss->channel == NULL) {
		ii->h->error.errtype = OTHER;
		ii->h->error.errcode = ENOMEM;
		return (-1);
	}
	for (size_t chan = 0; chan < channels; ++chan) {
		uint8_t rxoffs = SFF_8436_RX_CH1_MSB + chan * sizeof(uint16_t);
		uint8_t txoffs = SFF_8436_TX_CH1_MSB + chan * sizeof(uint16_t);
		ss->channel[chan].rx =
		    get_sff_channel(ii, SFF_8436_BASE, rxoffs);
		ss->channel[chan].tx =
		    get_sff_channel(ii, SFF_8436_BASE, txoffs);
	}
	ss->bitrate = get_qsfp_bitrate(ii);
	return (ii->error);
}

int
ifconfig_sfp_get_sfp_status(ifconfig_handle_t *h, const char *name,
    struct ifconfig_sfp_status *ss)
{
	struct i2c_info ii;

	memset(ss, 0, sizeof(*ss));

	if (i2c_info_init(&ii, h, name) != 0)
		return (-1);

	if (ifconfig_sfp_id_is_qsfp(ii.id))
		return (get_qsfp_status(&ii, ss));
	return (get_sfp_status(&ii, ss));
}

void
ifconfig_sfp_free_sfp_status(struct ifconfig_sfp_status *ss)
{
	if (ss != NULL)
		free(ss->channel);
}

static const char *
sfp_id_string_alt(uint8_t value)
{
	const char *id;

	if (value <= SFF_8024_ID_LAST)
		id = sff_8024_id[value];
	else if (value > 0x80)
		id = "Vendor specific";
	else
		id = "Reserved";

	return (id);
}

static const char *
sfp_conn_string_alt(uint8_t value)
{
	const char *conn;

	if (value >= 0x0D && value <= 0x1F)
		conn = "Unallocated";
	else if (value >= 0x24 && value <= 0x7F)
		conn = "Unallocated";
	else
		conn = "Vendor specific";

	return (conn);
}

void
ifconfig_sfp_get_sfp_info_strings(const struct ifconfig_sfp_info *sfp,
    struct ifconfig_sfp_info_strings *strings)
{
	get_sfp_info_strings(sfp, strings);
	if (strings->sfp_id == NULL)
		strings->sfp_id = sfp_id_string_alt(sfp->sfp_id);
	if (strings->sfp_conn == NULL)
		strings->sfp_conn = sfp_conn_string_alt(sfp->sfp_conn);
	if (strings->sfp_rev == NULL)
		strings->sfp_rev = "Unallocated";
}

const char *
ifconfig_sfp_physical_spec(const struct ifconfig_sfp_info *sfp,
    const struct ifconfig_sfp_info_strings *strings)
{
	switch (sfp->sfp_id) {
	case SFP_ID_UNKNOWN:
		break;
	case SFP_ID_QSFP:
	case SFP_ID_QSFPPLUS:
	case SFP_ID_QSFP28:
		if (sfp->sfp_eth_1040g & SFP_ETH_1040G_EXTENDED)
			return (strings->sfp_eth_ext);
		else if (sfp->sfp_eth_1040g)
			return (strings->sfp_eth_1040g);
		break;
	default:
		if (sfp->sfp_eth_ext)
			return (strings->sfp_eth_ext);
		else if (sfp->sfp_eth_10g)
			return (strings->sfp_eth_10g);
		else if (sfp->sfp_eth)
			return (strings->sfp_eth);
		break;
	}
	return ("Unknown");
}

int
ifconfig_sfp_get_sfp_dump(ifconfig_handle_t *h, const char *name,
    struct ifconfig_sfp_dump *dump)
{
	struct i2c_info ii;
	uint8_t *buf = dump->data;

	memset(dump->data, 0, sizeof(dump->data));

	if (i2c_info_init(&ii, h, name) != 0)
		return (-1);

	if (ifconfig_sfp_id_is_qsfp(ii.id)) {
		read_i2c(&ii, SFF_8436_BASE, QSFP_DUMP0_START, QSFP_DUMP0_SIZE,
		    buf + QSFP_DUMP0_START);
		read_i2c(&ii, SFF_8436_BASE, QSFP_DUMP1_START, QSFP_DUMP1_SIZE,
		    buf + QSFP_DUMP1_START);
	} else {
		read_i2c(&ii, SFF_8472_BASE, SFP_DUMP_START, SFP_DUMP_SIZE,
		    buf + SFP_DUMP_START);
	}

	return (ii.error != 0 ? -1 : 0);
}

size_t
ifconfig_sfp_dump_region_count(const struct ifconfig_sfp_dump *dp)
{
	uint8_t id_byte = dp->data[0];

	switch ((enum sfp_id)id_byte) {
	case SFP_ID_UNKNOWN:
		return (0);
	case SFP_ID_QSFP:
	case SFP_ID_QSFPPLUS:
	case SFP_ID_QSFP28:
		return (2);
	default:
		return (1);
	}
}
