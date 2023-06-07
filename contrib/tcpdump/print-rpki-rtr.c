/*
 * Copyright (c) 1998-2011 The TCPDUMP project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Hannes Gredler (hannes@gredler.at)
 */

/* \summary: Resource Public Key Infrastructure (RPKI) to Router Protocol printer */

/* specification: RFC 6810 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"


/*
 * RPKI/Router PDU header
 *
 * Here's what the PDU header looks like.
 * The length does include the version and length fields.
 */
typedef struct rpki_rtr_pdu_ {
    nd_uint8_t version;		/* Version number */
    nd_uint8_t pdu_type;		/* PDU type */
    union {
	nd_uint16_t session_id;	/* Session id */
	nd_uint16_t error_code;	/* Error code */
    } u;
    nd_uint32_t length;
} rpki_rtr_pdu;

/*
 * IPv4 Prefix PDU.
 */
typedef struct rpki_rtr_pdu_ipv4_prefix_ {
    rpki_rtr_pdu pdu_header;
    nd_uint8_t flags;
    nd_uint8_t prefix_length;
    nd_uint8_t max_length;
    nd_uint8_t zero;
    nd_ipv4 prefix;
    nd_uint32_t as;
} rpki_rtr_pdu_ipv4_prefix;

/*
 * IPv6 Prefix PDU.
 */
typedef struct rpki_rtr_pdu_ipv6_prefix_ {
    rpki_rtr_pdu pdu_header;
    nd_uint8_t flags;
    nd_uint8_t prefix_length;
    nd_uint8_t max_length;
    nd_uint8_t zero;
    nd_ipv6 prefix;
    nd_uint32_t as;
} rpki_rtr_pdu_ipv6_prefix;

/*
 * Error report PDU.
 */
typedef struct rpki_rtr_pdu_error_report_ {
    rpki_rtr_pdu pdu_header;
    nd_uint32_t encapsulated_pdu_length; /* Encapsulated PDU length */
    /* Copy of Erroneous PDU (variable, optional) */
    /* Length of Error Text (4 octets in network byte order) */
    /* Arbitrary Text of Error Diagnostic Message (variable, optional) */
} rpki_rtr_pdu_error_report;

/*
 * PDU type codes
 */
#define RPKI_RTR_SERIAL_NOTIFY_PDU	0
#define RPKI_RTR_SERIAL_QUERY_PDU	1
#define RPKI_RTR_RESET_QUERY_PDU	2
#define RPKI_RTR_CACHE_RESPONSE_PDU	3
#define RPKI_RTR_IPV4_PREFIX_PDU	4
#define RPKI_RTR_IPV6_PREFIX_PDU	6
#define RPKI_RTR_END_OF_DATA_PDU	7
#define RPKI_RTR_CACHE_RESET_PDU	8
#define RPKI_RTR_ERROR_REPORT_PDU	10

static const struct tok rpki_rtr_pdu_values[] = {
    { RPKI_RTR_SERIAL_NOTIFY_PDU, "Serial Notify" },
    { RPKI_RTR_SERIAL_QUERY_PDU, "Serial Query" },
    { RPKI_RTR_RESET_QUERY_PDU, "Reset Query" },
    { RPKI_RTR_CACHE_RESPONSE_PDU, "Cache Response" },
    { RPKI_RTR_IPV4_PREFIX_PDU, "IPV4 Prefix" },
    { RPKI_RTR_IPV6_PREFIX_PDU, "IPV6 Prefix" },
    { RPKI_RTR_END_OF_DATA_PDU, "End of Data" },
    { RPKI_RTR_CACHE_RESET_PDU, "Cache Reset" },
    { RPKI_RTR_ERROR_REPORT_PDU, "Error Report" },
    { 0, NULL}
};

static const struct tok rpki_rtr_error_codes[] = {
    { 0, "Corrupt Data" },
    { 1, "Internal Error" },
    { 2, "No Data Available" },
    { 3, "Invalid Request" },
    { 4, "Unsupported Protocol Version" },
    { 5, "Unsupported PDU Type" },
    { 6, "Withdrawal of Unknown Record" },
    { 7, "Duplicate Announcement Received" },
    { 0, NULL}
};

/*
 * Build a indentation string for a given indentation level.
 * XXX this should be really in util.c
 */
static char *
indent_string (u_int indent)
{
    static char buf[20];
    u_int idx;

    idx = 0;
    buf[idx] = '\0';

    /*
     * Does the static buffer fit ?
     */
    if (sizeof(buf) < ((indent/8) + (indent %8) + 2)) {
	return buf;
    }

    /*
     * Heading newline.
     */
    buf[idx] = '\n';
    idx++;

    while (indent >= 8) {
	buf[idx] = '\t';
	idx++;
	indent -= 8;
    }

    while (indent > 0) {
	buf[idx] = ' ';
	idx++;
	indent--;
    }

    /*
     * Trailing zero.
     */
    buf[idx] = '\0';

    return buf;
}

/*
 * Print a single PDU.
 */
static u_int
rpki_rtr_pdu_print(netdissect_options *ndo, const u_char *tptr, const u_int len,
		   const u_char recurse, const u_int indent)
{
    const rpki_rtr_pdu *pdu_header;
    u_int pdu_type, pdu_len, hexdump;
    const u_char *msg;

    /* Protocol Version */
    if (GET_U_1(tptr) != 0) {
	/* Skip the rest of the input buffer because even if this is
	 * a well-formed PDU of a future RPKI-Router protocol version
	 * followed by a well-formed PDU of RPKI-Router protocol
	 * version 0, there is no way to know exactly how to skip the
	 * current PDU.
	 */
	ND_PRINT("%sRPKI-RTRv%u (unknown)", indent_string(8), GET_U_1(tptr));
	return len;
    }
    if (len < sizeof(rpki_rtr_pdu)) {
	ND_PRINT("(%u bytes is too few to decode)", len);
	goto invalid;
    }
    ND_TCHECK_LEN(tptr, sizeof(rpki_rtr_pdu));
    pdu_header = (const rpki_rtr_pdu *)tptr;
    pdu_type = GET_U_1(pdu_header->pdu_type);
    pdu_len = GET_BE_U_4(pdu_header->length);
    /* Do not check bounds with pdu_len yet, do it in the case blocks
     * below to make it possible to decode at least the beginning of
     * a truncated Error Report PDU or a truncated encapsulated PDU.
     */
    hexdump = FALSE;

    ND_PRINT("%sRPKI-RTRv%u, %s PDU (%u), length: %u",
	   indent_string(8),
	   GET_U_1(pdu_header->version),
	   tok2str(rpki_rtr_pdu_values, "Unknown", pdu_type),
	   pdu_type, pdu_len);
    if (pdu_len < sizeof(rpki_rtr_pdu) || pdu_len > len)
	goto invalid;

    switch (pdu_type) {

	/*
	 * The following PDUs share the message format.
	 */
    case RPKI_RTR_SERIAL_NOTIFY_PDU:
    case RPKI_RTR_SERIAL_QUERY_PDU:
    case RPKI_RTR_END_OF_DATA_PDU:
	if (pdu_len != sizeof(rpki_rtr_pdu) + 4)
	    goto invalid;
        msg = (const u_char *)(pdu_header + 1);
	ND_PRINT("%sSession ID: 0x%04x, Serial: %u",
	       indent_string(indent+2),
	       GET_BE_U_2(pdu_header->u.session_id),
	       GET_BE_U_4(msg));
	break;

	/*
	 * The following PDUs share the message format.
	 */
    case RPKI_RTR_RESET_QUERY_PDU:
    case RPKI_RTR_CACHE_RESET_PDU:
	if (pdu_len != sizeof(rpki_rtr_pdu))
	    goto invalid;
	/* no additional boundary to check */

	/*
	 * Zero payload PDUs.
	 */
	break;

    case RPKI_RTR_CACHE_RESPONSE_PDU:
	if (pdu_len != sizeof(rpki_rtr_pdu))
	    goto invalid;
	/* no additional boundary to check */
	ND_PRINT("%sSession ID: 0x%04x",
	       indent_string(indent+2),
	       GET_BE_U_2(pdu_header->u.session_id));
	break;

    case RPKI_RTR_IPV4_PREFIX_PDU:
	{
	    const rpki_rtr_pdu_ipv4_prefix *pdu;

	    if (pdu_len != sizeof(rpki_rtr_pdu_ipv4_prefix))
		goto invalid;
	    pdu = (const rpki_rtr_pdu_ipv4_prefix *)tptr;
	    ND_PRINT("%sIPv4 Prefix %s/%u-%u, origin-as %u, flags 0x%02x",
		   indent_string(indent+2),
		   GET_IPADDR_STRING(pdu->prefix),
		   GET_U_1(pdu->prefix_length), GET_U_1(pdu->max_length),
		   GET_BE_U_4(pdu->as), GET_U_1(pdu->flags));
	}
	break;

    case RPKI_RTR_IPV6_PREFIX_PDU:
	{
	    const rpki_rtr_pdu_ipv6_prefix *pdu;

	    if (pdu_len != sizeof(rpki_rtr_pdu_ipv6_prefix))
		goto invalid;
	    pdu = (const rpki_rtr_pdu_ipv6_prefix *)tptr;
	    ND_PRINT("%sIPv6 Prefix %s/%u-%u, origin-as %u, flags 0x%02x",
		   indent_string(indent+2),
		   GET_IP6ADDR_STRING(pdu->prefix),
		   GET_U_1(pdu->prefix_length), GET_U_1(pdu->max_length),
		   GET_BE_U_4(pdu->as), GET_U_1(pdu->flags));
	}
	break;

    case RPKI_RTR_ERROR_REPORT_PDU:
	{
	    const rpki_rtr_pdu_error_report *pdu;
	    u_int encapsulated_pdu_length, text_length, tlen, error_code;

	    tlen = sizeof(rpki_rtr_pdu);
	    /* Do not test for the "Length of Error Text" data element yet. */
	    if (pdu_len < tlen + 4)
		goto invalid;
	    ND_TCHECK_LEN(tptr, tlen + 4);
	    /* Safe up to and including the "Length of Encapsulated PDU"
	     * data element, more data elements may be present.
	     */
	    pdu = (const rpki_rtr_pdu_error_report *)tptr;
	    encapsulated_pdu_length = GET_BE_U_4(pdu->encapsulated_pdu_length);
	    tlen += 4;

	    error_code = GET_BE_U_2(pdu->pdu_header.u.error_code);
	    ND_PRINT("%sError code: %s (%u), Encapsulated PDU length: %u",
		   indent_string(indent+2),
		   tok2str(rpki_rtr_error_codes, "Unknown", error_code),
		   error_code, encapsulated_pdu_length);

	    if (encapsulated_pdu_length) {
		/* Section 5.10 of RFC 6810 says:
		 * "An Error Report PDU MUST NOT be sent for an Error Report PDU."
		 *
		 * However, as far as the protocol encoding goes Error Report PDUs can
		 * happen to be nested in each other, however many times, in which case
		 * the decoder should still print such semantically incorrect PDUs.
		 *
		 * That said, "the Erroneous PDU field MAY be truncated" (ibid), thus
		 * to keep things simple this implementation decodes only the two
		 * outermost layers of PDUs and makes bounds checks in the outer and
		 * the inner PDU independently.
		 */
		if (pdu_len < tlen + encapsulated_pdu_length)
		    goto invalid;
		if (! recurse) {
		    ND_TCHECK_LEN(tptr, tlen + encapsulated_pdu_length);
		}
		else {
		    ND_PRINT("%s-----encapsulated PDU-----", indent_string(indent+4));
		    rpki_rtr_pdu_print(ndo, tptr + tlen,
			encapsulated_pdu_length, 0, indent + 2);
		}
		tlen += encapsulated_pdu_length;
	    }

	    if (pdu_len < tlen + 4)
		goto invalid;
	    ND_TCHECK_LEN(tptr, tlen + 4);
	    /* Safe up to and including the "Length of Error Text" data element,
	     * one more data element may be present.
	     */

	    /*
	     * Extract, trail-zero and print the Error message.
	     */
	    text_length = GET_BE_U_4(tptr + tlen);
	    tlen += 4;

	    if (text_length) {
		if (pdu_len < tlen + text_length)
		    goto invalid;
		/* nd_printn() makes the bounds check */
		ND_PRINT("%sError text: ", indent_string(indent+2));
		(void)nd_printn(ndo, tptr + tlen, text_length, NULL);
	    }
	}
	break;

    default:
	ND_TCHECK_LEN(tptr, pdu_len);

	/*
	 * Unknown data, please hexdump.
	 */
	hexdump = TRUE;
    }

    /* do we also want to see a hex dump ? */
    if (ndo->ndo_vflag > 1 || (ndo->ndo_vflag && hexdump)) {
	print_unknown_data(ndo,tptr,"\n\t  ", pdu_len);
    }
    return pdu_len;

invalid:
    nd_print_invalid(ndo);
    ND_TCHECK_LEN(tptr, len);
    return len;
}

void
rpki_rtr_print(netdissect_options *ndo, const u_char *pptr, u_int len)
{
    ndo->ndo_protocol = "rpki_rtr";
    if (!ndo->ndo_vflag) {
	ND_PRINT(", RPKI-RTR");
	return;
    }
    while (len) {
	u_int pdu_len = rpki_rtr_pdu_print(ndo, pptr, len, 1, 8);
	len -= pdu_len;
	pptr += pdu_len;
    }
}
