/*-
 * Copyright (c) 2014 Alexander V. Chernikov. All rights reserved.
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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

struct i2c_info;
typedef int (read_i2c)(struct i2c_info *ii, uint8_t addr, uint8_t off,
    uint8_t len, caddr_t buf);

struct i2c_info {
	int s;
	int error;
	int bshift;
	int qsfp;
	int do_diag;
	struct ifreq *ifr;
	read_i2c *f;
	char *textbuf;
	size_t bufsize;
	int cfd;
	int port_id;
	int chip_id;
};

struct _nv {
	int v;
	const char *n;
};

const char *find_value(struct _nv *x, int value);
const char *find_zero_bit(struct _nv *x, int value, int sz);

/* SFF-8472 Rev. 11.4 table 3.4: Connector values */
static struct _nv conn[] = {
	{ 0x00, "Unknown" },
	{ 0x01, "SC" },
	{ 0x02, "Fibre Channel Style 1 copper" },
	{ 0x03, "Fibre Channel Style 2 copper" },
	{ 0x04, "BNC/TNC" },
	{ 0x05, "Fibre Channel coaxial" },
	{ 0x06, "FiberJack" },
	{ 0x07, "LC" },
	{ 0x08, "MT-RJ" },
	{ 0x09, "MU" },
	{ 0x0A, "SG" },
	{ 0x0B, "Optical pigtail" },
	{ 0x0C, "MPO Parallel Optic" },
	{ 0x20, "HSSDC II" },
	{ 0x21, "Copper pigtail" },
	{ 0x22, "RJ45" },
	{ 0x23, "No separate connector" }, /* SFF-8436 */
	{ 0, NULL }
};

/* SFF-8472 Rev. 11.4 table 3.5: Transceiver codes */
/* 10G Ethernet/IB compliance codes, byte 3 */
static struct _nv eth_10g[] = {
	{ 0x80, "10G Base-ER" },
	{ 0x40, "10G Base-LRM" },
	{ 0x20, "10G Base-LR" },
	{ 0x10, "10G Base-SR" },
	{ 0x08, "1X SX" },
	{ 0x04, "1X LX" },
	{ 0x02, "1X Copper Active" },
	{ 0x01, "1X Copper Passive" },
	{ 0, NULL }
};

/* Ethernet compliance codes, byte 6 */
static struct _nv eth_compat[] = {
	{ 0x80, "BASE-PX" },
	{ 0x40, "BASE-BX10" },
	{ 0x20, "100BASE-FX" },
	{ 0x10, "100BASE-LX/LX10" },
	{ 0x08, "1000BASE-T" },
	{ 0x04, "1000BASE-CX" },
	{ 0x02, "1000BASE-LX" },
	{ 0x01, "1000BASE-SX" },
	{ 0, NULL }
};

/* FC link length, byte 7 */
static struct _nv fc_len[] = {
	{ 0x80, "very long distance" },
	{ 0x40, "short distance" },
	{ 0x20, "intermediate distance" },
	{ 0x10, "long distance" },
	{ 0x08, "medium distance" },
	{ 0, NULL }
};

/* Channel/Cable technology, byte 7-8 */
static struct _nv cab_tech[] = {
	{ 0x0400, "Shortwave laser (SA)" },
	{ 0x0200, "Longwave laser (LC)" },
	{ 0x0100, "Electrical inter-enclosure (EL)" },
	{ 0x80, "Electrical intra-enclosure (EL)" },
	{ 0x40, "Shortwave laser (SN)" },
	{ 0x20, "Shortwave laser (SL)" },
	{ 0x10, "Longwave laser (LL)" },
	{ 0x08, "Active Cable" },
	{ 0x04, "Passive Cable" },
	{ 0, NULL }
};

/* FC Transmission media, byte 9 */
static struct _nv fc_media[] = {
	{ 0x80, "Twin Axial Pair" },
	{ 0x40, "Twisted Pair" },
	{ 0x20, "Miniature Coax" },
	{ 0x10, "Viao Coax" },
	{ 0x08, "Miltimode, 62.5um" },
	{ 0x04, "Multimode, 50um" },
	{ 0x02, "" },
	{ 0x01, "Single Mode" },
	{ 0, NULL }
};

/* FC Speed, byte 10 */
static struct _nv fc_speed[] = {
	{ 0x80, "1200 MBytes/sec" },
	{ 0x40, "800 MBytes/sec" },
	{ 0x20, "1600 MBytes/sec" },
	{ 0x10, "400 MBytes/sec" },
	{ 0x08, "3200 MBytes/sec" },
	{ 0x04, "200 MBytes/sec" },
	{ 0x01, "100 MBytes/sec" },
	{ 0, NULL }
};

/* SFF-8436 Rev. 4.8 table 33: Specification compliance  */

/* 10/40G Ethernet compliance codes, byte 128 + 3 */
static struct _nv eth_1040g[] = {
	{ 0x80, "Reserved" },
	{ 0x40, "10GBASE-LRM" },
	{ 0x20, "10GBASE-LR" },
	{ 0x10, "10GBASE-SR" },
	{ 0x08, "40GBASE-CR4" },
	{ 0x04, "40GBASE-SR4" },
	{ 0x02, "40GBASE-LR4" },
	{ 0x01, "40G Active Cable" },
	{ 0, NULL }
};

const char *
find_value(struct _nv *x, int value)
{
	for (; x->n != NULL; x++)
		if (x->v == value)
			return (x->n);
	return (NULL);
}

const char *
find_zero_bit(struct _nv *x, int value, int sz)
{
	int v, m;
	const char *s;

	v = 1;
	for (v = 1, m = 1 << (8 * sz); v < m; v *= 2) {
		if ((value & v) == 0)
			continue;
		if ((s = find_value(x, value & v)) != NULL) {
			value &= ~v;
			return (s);
		}
	}

	return (NULL);
}

static void
convert_sff_identifier(char *buf, size_t size, uint8_t value)
{
	const char *x;

	x = NULL;
	if (value <= SFF_8024_ID_LAST)
		x = sff_8024_id[value];
	else {
		if (value > 0x80)
			x = "Vendor specific";
		else
			x = "Reserved";
	}

	snprintf(buf, size, "%s", x);
}

static void
convert_sff_connector(char *buf, size_t size, uint8_t value)
{
	const char *x;

	if ((x = find_value(conn, value)) == NULL) {
		if (value >= 0x0D && value <= 0x1F)
			x = "Unallocated";
		else if (value >= 0x24 && value <= 0x7F)
			x = "Unallocated";
		else
			x = "Vendor specific";
	}

	snprintf(buf, size, "%s", x);
}

static void
get_sfp_identifier(struct i2c_info *ii, char *buf, size_t size)
{
	uint8_t data;

	ii->f(ii, SFF_8472_BASE, SFF_8472_ID, 1, (caddr_t)&data);
	convert_sff_identifier(buf, size, data);
}

static void
get_sfp_connector(struct i2c_info *ii, char *buf, size_t size)
{
	uint8_t data;

	ii->f(ii, SFF_8472_BASE, SFF_8472_CONNECTOR, 1, (caddr_t)&data);
	convert_sff_connector(buf, size, data);
}

static void
get_qsfp_identifier(struct i2c_info *ii, char *buf, size_t size)
{
	uint8_t data;

	ii->f(ii, SFF_8436_BASE, SFF_8436_ID, 1, (caddr_t)&data);
	convert_sff_identifier(buf, size, data);
}

static void
get_qsfp_connector(struct i2c_info *ii, char *buf, size_t size)
{
	uint8_t data;

	ii->f(ii, SFF_8436_BASE, SFF_8436_CONNECTOR, 1, (caddr_t)&data);
	convert_sff_connector(buf, size, data);
}

static void
printf_sfp_transceiver_descr(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[12];
	const char *tech_class, *tech_len, *tech_tech, *tech_media, *tech_speed;

	tech_class = NULL;
	tech_len = NULL;
	tech_tech = NULL;
	tech_media = NULL;
	tech_speed = NULL;

	/* Read bytes 3-10 at once */
	ii->f(ii, SFF_8472_BASE, SFF_8472_TRANS_START, 8, &xbuf[3]);

	/* Check 10G ethernet first */
	tech_class = find_zero_bit(eth_10g, xbuf[3], 1);
	if (tech_class == NULL) {
		/* No match. Try 1G */
		tech_class = find_zero_bit(eth_compat, xbuf[6], 1);
	}

	tech_len = find_zero_bit(fc_len, xbuf[7], 1);
	tech_tech = find_zero_bit(cab_tech, xbuf[7] << 8 | xbuf[8], 2);
	tech_media = find_zero_bit(fc_media, xbuf[9], 1);
	tech_speed = find_zero_bit(fc_speed, xbuf[10], 1);

	printf("Class: %s\n", tech_class);
	printf("Length: %s\n", tech_len);
	printf("Tech: %s\n", tech_tech);
	printf("Media: %s\n", tech_media);
	printf("Speed: %s\n", tech_speed);
}

static void
get_sfp_transceiver_class(struct i2c_info *ii, char *buf, size_t size)
{
	const char *tech_class;
	uint8_t code;

	/* Check 10G Ethernet/IB first */
	ii->f(ii, SFF_8472_BASE, SFF_8472_TRANS_START, 1, (caddr_t)&code);
	tech_class = find_zero_bit(eth_10g, code, 1);
	if (tech_class == NULL) {
		/* No match. Try Ethernet 1G */
		ii->f(ii, SFF_8472_BASE, SFF_8472_TRANS_START + 3,
		    1, (caddr_t)&code);
		tech_class = find_zero_bit(eth_compat, code, 1);
	}

	if (tech_class == NULL)
		tech_class = "Unknown";

	snprintf(buf, size, "%s", tech_class);
}

static void
get_qsfp_transceiver_class(struct i2c_info *ii, char *buf, size_t size)
{
	const char *tech_class;
	uint8_t code;

	/* Check 10/40G Ethernet class only */
	ii->f(ii, SFF_8436_BASE, SFF_8436_CODE_E1040G, 1, (caddr_t)&code);
	tech_class = find_zero_bit(eth_1040g, code, 1);
	if (tech_class == NULL)
		tech_class = "Unknown";

	snprintf(buf, size, "%s", tech_class);
}

/*
 * Print SFF-8472/SFF-8436 string to supplied buffer.
 * All (vendor-specific) strings are padded right with '0x20'.
 */
static void
convert_sff_name(char *buf, size_t size, char *xbuf)
{
	char *p;

	for (p = &xbuf[16]; *(p - 1) == 0x20; p--)
		;
	*p = '\0';
	snprintf(buf, size, "%s", xbuf);
}

static void
convert_sff_date(char *buf, size_t size, char *xbuf)
{

	snprintf(buf, size, "20%c%c-%c%c-%c%c", xbuf[0], xbuf[1],
	    xbuf[2], xbuf[3], xbuf[4], xbuf[5]);
}

static void
get_sfp_vendor_name(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[17];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8472_BASE, SFF_8472_VENDOR_START, 16, xbuf);
	convert_sff_name(buf, size, xbuf);
}

static void
get_sfp_vendor_pn(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[17];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8472_BASE, SFF_8472_PN_START, 16, xbuf);
	convert_sff_name(buf, size, xbuf);
}

static void
get_sfp_vendor_sn(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[17];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8472_BASE, SFF_8472_SN_START, 16, xbuf);
	convert_sff_name(buf, size, xbuf);
}

static void
get_sfp_vendor_date(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[6];

	memset(xbuf, 0, sizeof(xbuf));
	/* Date code, see Table 3.8 for description */
	ii->f(ii, SFF_8472_BASE, SFF_8472_DATE_START, 6, xbuf);
	convert_sff_date(buf, size, xbuf);
}

static void
get_qsfp_vendor_name(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[17];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_VENDOR_START, 16, xbuf);
	convert_sff_name(buf, size, xbuf);
}

static void
get_qsfp_vendor_pn(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[17];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_PN_START, 16, xbuf);
	convert_sff_name(buf, size, xbuf);
}

static void
get_qsfp_vendor_sn(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[17];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_SN_START, 16, xbuf);
	convert_sff_name(buf, size, xbuf);
}

static void
get_qsfp_vendor_date(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[6];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_DATE_START, 6, xbuf);
	convert_sff_date(buf, size, xbuf);
}

static void
print_sfp_vendor(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[80];

	memset(xbuf, 0, sizeof(xbuf));
	if (ii->qsfp != 0) {
		get_qsfp_vendor_name(ii, xbuf, 20);
		get_qsfp_vendor_pn(ii, &xbuf[20], 20);
		get_qsfp_vendor_sn(ii, &xbuf[40], 20);
		get_qsfp_vendor_date(ii, &xbuf[60], 20);
	} else {
		get_sfp_vendor_name(ii, xbuf, 20);
		get_sfp_vendor_pn(ii, &xbuf[20], 20);
		get_sfp_vendor_sn(ii, &xbuf[40], 20);
		get_sfp_vendor_date(ii, &xbuf[60], 20);
	}

	snprintf(buf, size, "vendor: %s PN: %s SN: %s DATE: %s",
	    xbuf, &xbuf[20],  &xbuf[40], &xbuf[60]);
}

/*
 * Converts internal templerature (SFF-8472, SFF-8436)
 * 16-bit unsigned value to human-readable representation:
 * 
 * Internally measured Module temperature are represented
 * as a 16-bit signed twos complement value in increments of
 * 1/256 degrees Celsius, yielding a total range of –128C to +128C
 * that is considered valid between –40 and +125C.
 *
 */
static void
convert_sff_temp(char *buf, size_t size, char *xbuf)
{
	double d;

	d = (double)(int8_t)xbuf[0];
	d += (double)(uint8_t)xbuf[1] / 256;

	snprintf(buf, size, "%.2f C", d);
}

/*
 * Retrieves supplied voltage (SFF-8472, SFF-8436).
 * 16-bit usigned value, treated as range 0..+6.55 Volts
 */
static void
convert_sff_voltage(char *buf, size_t size, char *xbuf)
{
	double d;

	d = (double)(((uint8_t)xbuf[0] << 8) | (uint8_t)xbuf[1]);
	snprintf(buf, size, "%.2f Volts", d / 10000);
}

/*
 * Converts value in @xbuf to both milliwats and dBm
 * human representation.
 */
static void
convert_sff_power(struct i2c_info *ii, char *buf, size_t size, char *xbuf)
{
	uint16_t mW;
	double dbm;

	mW = ((uint8_t)xbuf[0] << 8) + (uint8_t)xbuf[1];

	/* Convert mw to dbm */
	dbm = 10.0 * log10(1.0 * mW / 10000);

	/*
	 * Assume internally-calibrated data.
	 * This is always true for SFF-8346, and explicitly
	 * checked for SFF-8472.
	 */

	/* Table 3.9, bit 5 is set, internally calibrated */
	snprintf(buf, size, "%d.%02d mW (%.2f dBm)",
    	    mW / 10000, (mW % 10000) / 100, dbm);
}

static void
get_sfp_temp(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8472_DIAG, SFF_8472_TEMP, 2, xbuf);
	convert_sff_temp(buf, size, xbuf);
}

static void
get_sfp_voltage(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8472_DIAG, SFF_8472_VCC, 2, xbuf);
	convert_sff_voltage(buf, size, xbuf);
}

static void
get_qsfp_temp(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_TEMP, 2, xbuf);
	convert_sff_temp(buf, size, xbuf);
}

static void
get_qsfp_voltage(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_VCC, 2, xbuf);
	convert_sff_voltage(buf, size, xbuf);
}

static void
get_sfp_rx_power(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8472_DIAG, SFF_8472_RX_POWER, 2, xbuf);
	convert_sff_power(ii, buf, size, xbuf);
}

static void
get_sfp_tx_power(struct i2c_info *ii, char *buf, size_t size)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8472_DIAG, SFF_8472_TX_POWER, 2, xbuf);
	convert_sff_power(ii, buf, size, xbuf);
}

static void
get_qsfp_rx_power(struct i2c_info *ii, char *buf, size_t size, int chan)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_RX_CH1_MSB + (chan - 1) * 2, 2, xbuf);
	convert_sff_power(ii, buf, size, xbuf);
}

static void
get_qsfp_tx_power(struct i2c_info *ii, char *buf, size_t size, int chan)
{
	char xbuf[2];

	memset(xbuf, 0, sizeof(xbuf));
	ii->f(ii, SFF_8436_BASE, SFF_8436_TX_CH1_MSB + (chan -1) * 2, 2, xbuf);
	convert_sff_power(ii, buf, size, xbuf);
}

/* Generic handler */
static int
read_i2c_generic(struct i2c_info *ii, uint8_t addr, uint8_t off, uint8_t len,
    caddr_t buf)
{
	struct ifi2creq req;
	int i, l;

	if (ii->error != 0)
		return (ii->error);

	ii->ifr->ifr_data = (caddr_t)&req;

	i = 0;
	l = 0;
	memset(&req, 0, sizeof(req));
	req.dev_addr = addr;
	req.offset = off;
	req.len = len;

	while (len > 0) {
		l = (len > sizeof(req.data)) ? sizeof(req.data) : len;
		req.len = l;
		if (ioctl(ii->s, SIOCGI2C, ii->ifr) != 0) {
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

static void
print_qsfp_status(struct i2c_info *ii, int verbose)
{
	char buf[80], buf2[40], buf3[40];
	uint8_t diag_type;
	int i;

	/* Read diagnostic monitoring type */
	ii->f(ii, SFF_8436_BASE, SFF_8436_DIAG_TYPE, 1, (caddr_t)&diag_type);
	if (ii->error != 0)
		return;

	/*
	 * Read monitoring data it is supplied.
	 * XXX: It is not exactly clear from standard
	 * how one can specify lack of measurements (passive cables case).
	 */
	if (diag_type != 0)
		ii->do_diag = 1;
	ii->qsfp = 1;

	/* Transceiver type */
	get_qsfp_identifier(ii, buf, sizeof(buf));
	get_qsfp_transceiver_class(ii, buf2, sizeof(buf2));
	get_qsfp_connector(ii, buf3, sizeof(buf3));
	if (ii->error == 0)
		printf("\tplugged: %s %s (%s)\n", buf, buf2, buf3);
	print_sfp_vendor(ii, buf, sizeof(buf));
	if (ii->error == 0)
		printf("\t%s\n", buf);

	/* Request current measurements if they are provided: */
	if (ii->do_diag != 0) {
		get_qsfp_temp(ii, buf, sizeof(buf));
		get_qsfp_voltage(ii, buf2, sizeof(buf2));
		printf("\tmodule temperature: %s voltage: %s\n", buf, buf2);
		for (i = 1; i <= 4; i++) {
			get_qsfp_rx_power(ii, buf, sizeof(buf), i);
			get_qsfp_tx_power(ii, buf2, sizeof(buf2), i);
			printf("\tlane %d: RX: %s TX: %s\n", i, buf, buf2);
		}
	}
}

static void
print_sfp_status(struct i2c_info *ii, int verbose)
{
	char buf[80], buf2[40], buf3[40];
	uint8_t diag_type, flags;

	/* Read diagnostic monitoring type */
	ii->f(ii, SFF_8472_BASE, SFF_8472_DIAG_TYPE, 1, (caddr_t)&diag_type);
	if (ii->error != 0)
		return;

	/*
	 * Read monitoring data IFF it is supplied AND is
	 * internally calibrated
	 */
	flags = SFF_8472_DDM_DONE | SFF_8472_DDM_INTERNAL;
	if ((diag_type & flags) == flags)
		ii->do_diag = 1;

	/* Transceiver type */
	get_sfp_identifier(ii, buf, sizeof(buf));
	get_sfp_transceiver_class(ii, buf2, sizeof(buf2));
	get_sfp_connector(ii, buf3, sizeof(buf3));
	if (ii->error == 0)
		printf("\tplugged: %s %s (%s)\n", buf, buf2, buf3);
	if (verbose > 2)
		printf_sfp_transceiver_descr(ii, buf, sizeof(buf));
	print_sfp_vendor(ii, buf, sizeof(buf));
	if (ii->error == 0)
		printf("\t%s\n", buf);

	/*
	 * Request current measurements iff they are provided:
	 */
	if (ii->do_diag != 0) {
		get_sfp_temp(ii, buf, sizeof(buf));
		get_sfp_voltage(ii, buf2, sizeof(buf2));
		printf("\tmodule temperature: %s Voltage: %s\n", buf, buf2);
		get_sfp_rx_power(ii, buf, sizeof(buf));
		get_sfp_tx_power(ii, buf2, sizeof(buf2));
		printf("\tRX: %s TX: %s\n", buf, buf2);
	}
}

void
sfp_status(int s, struct ifreq *ifr, int verbose)
{
	struct i2c_info ii;
	uint8_t id_byte;

	memset(&ii, 0, sizeof(ii));
	/* Prepare necessary into to pass to NIC handler */
	ii.s = s;
	ii.ifr = ifr;
	ii.f = read_i2c_generic;

	/*
	 * Try to read byte 0 from i2c:
	 * Both SFF-8472 and SFF-8436 use it as
	 * 'identification byte'
	 */
	id_byte = 0;
	ii.f(&ii, SFF_8472_BASE, SFF_8472_ID, 1, (caddr_t)&id_byte);
	if (ii.error != 0)
		return;

	switch (id_byte) {
	case SFF_8024_ID_QSFP:
	case SFF_8024_ID_QSFPPLUS:
		print_qsfp_status(&ii, verbose);
		break;
	default:
		print_sfp_status(&ii, verbose);
	};
}

