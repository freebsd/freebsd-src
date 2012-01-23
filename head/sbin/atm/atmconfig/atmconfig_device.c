/*
 * Copyright (c) 2001-2002
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
 * Copyright (c) 2003-2004
 *	Hartmut Brandt.
 * 	All rights reserved.
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "atmconfig.h"
#include "atmconfig_device.h"
#include "private.h"
#include "oid.h"

#include <bsnmp/asn1.h>
#include <bsnmp/snmp.h>
#include <bsnmp/snmpclient.h>

/*
 * Description of the begemotAtmIfTable
 */
static const struct snmp_table atmif_table = {
	OIDX_begemotAtmIfTable,
	OIDX_begemotAtmIfTableLastChange, 2,
	sizeof(struct atmif),
	1, 0x7ffULL,
	{
	  { 0,
	    SNMP_SYNTAX_INTEGER, offsetof(struct atmif, index) },
	  { OID_begemotAtmIfName,
	    SNMP_SYNTAX_OCTETSTRING, offsetof(struct atmif, ifname) },
	  { OID_begemotAtmIfPcr,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmif, pcr) },
	  { OID_begemotAtmIfMedia,
	    SNMP_SYNTAX_INTEGER, offsetof(struct atmif, media) },
	  { OID_begemotAtmIfVpiBits,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmif, vpi_bits) },
	  { OID_begemotAtmIfVciBits,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmif, vci_bits) },
	  { OID_begemotAtmIfMaxVpcs,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmif, max_vpcs) },
	  { OID_begemotAtmIfMaxVccs,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmif, max_vccs) },
	  { OID_begemotAtmIfEsi,
	    SNMP_SYNTAX_OCTETSTRING, offsetof(struct atmif, esi) },
	  { OID_begemotAtmIfCarrierStatus,
	    SNMP_SYNTAX_INTEGER, offsetof(struct atmif, carrier) },
	  { OID_begemotAtmIfMode,
	    SNMP_SYNTAX_INTEGER, offsetof(struct atmif, mode) },
          { 0, SNMP_SYNTAX_NULL, 0 }
	}
};

/* List of all ATM interfaces */
struct atmif_list atmif_list = TAILQ_HEAD_INITIALIZER(atmif_list);

/*
 * ATM hardware table
 */
struct atmhw {
	TAILQ_ENTRY(atmhw) link;
	uint64_t	found;
	int32_t		index;
	u_char		*vendor;
	size_t		vendorlen;
	u_char		*device;
	size_t		devicelen;
	uint32_t	serial;
	uint32_t	version;
	uint32_t	soft_version;
};
TAILQ_HEAD(atmhw_list, atmhw);

/* list of ATM hardware */
static struct atmhw_list atmhw_list;

/*
 * Read ATM hardware table
 */
static const struct snmp_table atmhw_table = {
	OIDX_begemotAtmHWTable,
	OIDX_begemotAtmIfTableLastChange, 2,
	sizeof(struct atmhw),
	1, 0x3fULL,
	{
	  { 0,
	    SNMP_SYNTAX_INTEGER, offsetof(struct atmhw, index) },
	  { OID_begemotAtmHWVendor,
	    SNMP_SYNTAX_OCTETSTRING, offsetof(struct atmhw, vendor) },
	  { OID_begemotAtmHWDevice,
	    SNMP_SYNTAX_OCTETSTRING, offsetof(struct atmhw, device) },
	  { OID_begemotAtmHWSerial,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmhw, serial) },
	  { OID_begemotAtmHWVersion,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmhw, version) },
	  { OID_begemotAtmHWSoftVersion,
	    SNMP_SYNTAX_GAUGE, offsetof(struct atmhw, soft_version) },
          { 0, SNMP_SYNTAX_NULL, 0 }
	}
};

static void device_status(int, char *[]);
static void device_hardware(int, char *[]);
static void device_modify(int, char *[]);

static const struct cmdtab device_tab[] = {
	{ "hardware",	NULL,	device_hardware },
 	{ "status",	NULL,	device_status },
 	{ "modify",	NULL,	device_modify },
	{ NULL,		NULL,	NULL }
};

static const struct cmdtab entry =
	{ "device",	device_tab,	NULL };

static DEF_MODULE(&entry);

/*
 * Carrier state to string
 */
static const struct penum strcarrier[] = {
	{ 1, "on" },
	{ 2, "off" },
	{ 3, "unknown" },
	{ 4, "none" },
	{ 0, NULL }
};
/*
 * SUNI mode to string
 */
static const struct penum strsunimode[] = {
	{ 1, "sonet" },
	{ 2, "sdh" },
	{ 3, "unknown" },
	{ 0, NULL }
};

/*
 * OIDs
 */
static const struct asn_oid
	oid_begemotAtmIfMode = OIDX_begemotAtmIfMode;

/*
 * Print 1st status line
 */
static void
dev_status1(const struct atmif *aif)
{
	char buf[100];

	printf("%-5u %-8s %-6u %-4u %-5u %-4u %-5u "
	    "%02x:%02x:%02x:%02x:%02x:%02x %s\n", aif->index,
	    aif->ifname, aif->pcr,
	    (1 << aif->vpi_bits) - 1, (1 << aif->vci_bits) - 1,
	    aif->max_vpcs, aif->max_vccs, aif->esi[0],
	    aif->esi[1], aif->esi[2], aif->esi[3], aif->esi[4], aif->esi[5],
	    penum(aif->carrier, strcarrier, buf));
}

/*
 * Print 2nd status line
 */
static void
dev_status2(const struct atmif *aif)
{
	char buf[100];

	printf("%-5u %-8s %s\n", aif->index, aif->ifname,
	    penum(aif->mode, strsunimode, buf));
}

/*
 * Implement the 'device status' command
 */
static void
device_status(int argc, char *argv[])
{
	int opt, i;
	struct atmif *aif;
	static const struct option opts[] = {
	    { NULL, 0, NULL }
	};

	const char dev1[] =
	    "Interface             Max        Max\n"
	    "Index Name     PCR    VPI  VCI   VPCs VCCs  ESI               Carrier\n";
	const char dev2[] =
	    "Interface\n"
	    "Index Name     Mode\n";

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	snmp_open(NULL, NULL, NULL, NULL);
	atexit(snmp_close);

	atmif_fetchtable();

	if (TAILQ_EMPTY(&atmif_list))
		errx(1, "no ATM interfaces found");

	if (argc > 0) {
		heading_init();
		for (i = 0; i < argc; i++) {
			if ((aif = atmif_find_name(argv[i])) == NULL) {
				warnx("%s: no such ATM interface", argv[i]);
				continue;
			}
			heading(dev1);
			dev_status1(aif);
		}
		heading_init();
		for (i = 0; i < argc; i++) {
			if ((aif = atmif_find_name(argv[i])) == NULL)
				continue;
			heading(dev2);
			dev_status2(aif);
		}
	} else {
		heading_init();
		TAILQ_FOREACH(aif, &atmif_list, link) {
			heading(dev1);
			dev_status1(aif);
		}
		heading_init();
		TAILQ_FOREACH(aif, &atmif_list, link) {
			heading(dev2);
			dev_status2(aif);
		}
	}
}

/*
 * Print hardware info line
 */
static void
dev_hardware(const struct atmif *aif)
{
	const struct atmhw *hw;

	TAILQ_FOREACH(hw, &atmhw_list, link)
		if (aif->index == hw->index)
			break;
	if (hw == NULL) {
		warnx("hardware info not found for '%s'", aif->ifname);
		return;
	}

	printf("%-5u %-8s %-16s%-10s %-10u %-10u %u\n", aif->index,
	    aif->ifname, hw->vendor, hw->device, hw->serial,
	    hw->version, hw->soft_version);
}

/*
 * Show hardware configuration
 */
static void
device_hardware(int argc, char *argv[])
{
	int opt, i;
	struct atmif *aif;

	static const struct option opts[] = {
	    { NULL, 0, NULL }
	};

	static const char headline[] =
	    "Interface      \n"
	    "Index Name     Vendor          Card       Serial     HW         SW\n";

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	snmp_open(NULL, NULL, NULL, NULL);
	atexit(snmp_close);

	atmif_fetchtable();

	if (snmp_table_fetch(&atmhw_table, &atmhw_list) != 0)
		errx(1, "AtmHW table: %s", snmp_client.error);

	if (argc > 0) {
		heading_init();
		for (i = 0; i < argc; i++) {
			if ((aif = atmif_find_name(argv[i])) == NULL) {
				warnx("interface not found '%s'", argv[i]);
				continue;
			}
			heading(headline);
			dev_hardware(aif);
		}
	} else {
		heading_init();
		TAILQ_FOREACH(aif, &atmif_list, link) {
			heading(headline);
			dev_hardware(aif);
		}
	}
}

/*
 * Change device parameters
 */
static void
device_modify(int argc, char *argv[])
{
	int opt;
	struct atmif *aif;
	int mode = 0;
	int n;
	struct snmp_pdu pdu, resp;

	static const struct option opts[] = {
#define MODIFY_MODE	0
	    { "mode", OPT_STRING, NULL },
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {

		  case MODIFY_MODE:
			if (pparse(&mode, strsunimode, optarg) == -1 ||
			    mode == 3)
				errx(1, "illegal mode for -m '%s'", optarg);
			break;
		}

	if (argc != 1)
		errx(1, "device modify needs one argument");

	snmp_open(NULL, NULL, NULL, NULL);

	atexit(snmp_close);
	atmif_fetchtable();

	if ((aif = atmif_find_name(argv[0])) == NULL)
		errx(1, "%s: no such ATM interface", argv[0]);

	snmp_pdu_create(&pdu, SNMP_PDU_SET);
	if (mode != 0) {
		n = snmp_add_binding(&pdu,
		    &oid_begemotAtmIfMode, SNMP_SYNTAX_INTEGER,
		    NULL);
		snmp_oid_append(&pdu.bindings[n + 0].var, "i",
		    (asn_subid_t)aif->index);
		pdu.bindings[n + 0].v.integer = mode;
	}

	if (pdu.nbindings == 0)
		errx(1, "must specify something to modify");

	if (snmp_dialog(&pdu, &resp))
		errx(1, "No response from '%s': %s", snmp_client.chost,
		    snmp_client.error);

	if (snmp_pdu_check(&pdu, &resp) <= 0)
		errx(1, "Error modifying device");

	snmp_pdu_free(&resp);
	snmp_pdu_free(&pdu);
}

/* XXX while this is compiled in */
void
device_register(void)
{
	register_module(&amodule_1);
}

/*
 * Fetch the ATM interface table
 */
void
atmif_fetchtable(void)
{
	struct atmif *aif;

	while ((aif = TAILQ_FIRST(&atmif_list)) != NULL) {
		free(aif->ifname);
		free(aif->esi);
		TAILQ_REMOVE(&atmif_list, aif, link);
		free(aif);
	}

	if (snmp_table_fetch(&atmif_table, &atmif_list) != 0)
		errx(1, "AtmIf table: %s", snmp_client.error);
}

/*
 * Find a named ATM interface
 */
struct atmif *
atmif_find_name(const char *ifname)
{
	struct atmif *atmif;

	TAILQ_FOREACH(atmif, &atmif_list, link)
		if (strcmp(atmif->ifname, ifname) == 0)
			return (atmif);
	return (NULL);
}
/*
 * find an ATM interface by index
 */
struct atmif *
atmif_find(u_int idx)
{
	struct atmif *atmif;

	TAILQ_FOREACH(atmif, &atmif_list, link)
		if (atmif->index == (int32_t)idx)
			return (atmif);
	return (NULL);
}
