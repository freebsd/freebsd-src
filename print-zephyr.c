/*
 * Decode and print Zephyr packets.
 *
 *	https://web.mit.edu/zephyr/doc/protocol
 *
 * Copyright (c) 2001 Nickolai Zeldovich <kolya@MIT.EDU>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * The name of the author(s) may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.
 */

/* \summary: Zephyr printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "netdissect-ctype.h"

#include "netdissect.h"
#include "extract.h"

struct z_packet {
    const char *version;
    int numfields;
    int kind;
    const char *uid;
    int port;
    int auth;
    int authlen;
    const char *authdata;
    const char *class;
    const char *inst;
    const char *opcode;
    const char *sender;
    const char *recipient;
    const char *format;
    int cksum;
    int multi;
    const char *multi_uid;
    /* Other fields follow here.. */
};

enum z_packet_type {
    Z_PACKET_UNSAFE = 0,
    Z_PACKET_UNACKED,
    Z_PACKET_ACKED,
    Z_PACKET_HMACK,
    Z_PACKET_HMCTL,
    Z_PACKET_SERVACK,
    Z_PACKET_SERVNAK,
    Z_PACKET_CLIENTACK,
    Z_PACKET_STAT
};

static const struct tok z_types[] = {
    { Z_PACKET_UNSAFE,		"unsafe" },
    { Z_PACKET_UNACKED,		"unacked" },
    { Z_PACKET_ACKED,		"acked" },
    { Z_PACKET_HMACK,		"hm-ack" },
    { Z_PACKET_HMCTL,		"hm-ctl" },
    { Z_PACKET_SERVACK,		"serv-ack" },
    { Z_PACKET_SERVNAK,		"serv-nak" },
    { Z_PACKET_CLIENTACK,	"client-ack" },
    { Z_PACKET_STAT,		"stat" },
    { 0,			NULL }
};

static char z_buf[256];

static const char *
parse_field(netdissect_options *ndo, const char **pptr, int *len)
{
    const char *s;

    /* Start of string */
    s = *pptr;
    /* Scan for the NUL terminator */
    for (;;) {
	if (*len == 0) {
	    /* Ran out of packet data without finding it */
	    return NULL;
	}
	if (GET_U_1(*pptr) == '\0') {
	    /* Found it */
	    break;
	}
	/* Keep scanning */
	(*pptr)++;
	(*len)--;
    }
    /* Skip the NUL terminator */
    (*pptr)++;
    (*len)--;
    return s;
}

static const char *
z_triple(const char *class, const char *inst, const char *recipient)
{
    if (!*recipient)
	recipient = "*";
    snprintf(z_buf, sizeof(z_buf), "<%s,%s,%s>", class, inst, recipient);
    z_buf[sizeof(z_buf)-1] = '\0';
    return z_buf;
}

static const char *
str_to_lower(const char *string)
{
    char *zb_string;

    strncpy(z_buf, string, sizeof(z_buf));
    z_buf[sizeof(z_buf)-1] = '\0';

    zb_string = z_buf;
    while (*zb_string) {
	*zb_string = ND_ASCII_TOLOWER(*zb_string);
	zb_string++;
    }

    return z_buf;
}

#define ZEPHYR_PRINT(str1,str2) \
{ ND_PRINT("%s", (str1)); fn_print_str(ndo, (const u_char *)(str2)); }

void
zephyr_print(netdissect_options *ndo, const u_char *cp, u_int length)
{
    struct z_packet z = {
	NULL,	/* version */
	0,	/* numfields */
	0,	/* kind */
	NULL,	/* uid */
	0,	/* port */
	0,	/* auth */
	0,	/* authlen */
	NULL,	/* authdata */
	NULL,	/* class */
	NULL,	/* inst */
	NULL,	/* opcode */
	NULL,	/* sender */
	NULL,	/* recipient */
	NULL,	/* format */
	0,	/* cksum */
	0,	/* multi */
	NULL	/* multi_uid */
    };
    const char *parse = (const char *) cp;
    int parselen = length;
    const char *s;
    int lose = 0;

    ndo->ndo_protocol = "zephyr";
    /* squelch compiler warnings */

#define PARSE_STRING						\
	s = parse_field(ndo, &parse, &parselen);	\
	if (!s) lose = 1;

#define PARSE_FIELD_INT(field)			\
	PARSE_STRING				\
	if (!lose) field = strtol(s, 0, 16);

#define PARSE_FIELD_STR(field)			\
	PARSE_STRING				\
	if (!lose) field = s;

    PARSE_FIELD_STR(z.version);
    if (lose)
	goto invalid;

    if (strncmp(z.version, "ZEPH", 4))
	return;

    PARSE_FIELD_INT(z.numfields);
    PARSE_FIELD_INT(z.kind);
    PARSE_FIELD_STR(z.uid);
    PARSE_FIELD_INT(z.port);
    PARSE_FIELD_INT(z.auth);
    PARSE_FIELD_INT(z.authlen);
    PARSE_FIELD_STR(z.authdata);
    PARSE_FIELD_STR(z.class);
    PARSE_FIELD_STR(z.inst);
    PARSE_FIELD_STR(z.opcode);
    PARSE_FIELD_STR(z.sender);
    PARSE_FIELD_STR(z.recipient);
    PARSE_FIELD_STR(z.format);
    PARSE_FIELD_INT(z.cksum);
    PARSE_FIELD_INT(z.multi);
    PARSE_FIELD_STR(z.multi_uid);

    if (lose)
	goto invalid;

    ND_PRINT(" zephyr");
    if (strncmp(z.version+4, "0.2", 3)) {
	ZEPHYR_PRINT(" v", z.version+4)
	return;
    }

    ND_PRINT(" %s", tok2str(z_types, "type %d", z.kind));
    if (z.kind == Z_PACKET_SERVACK) {
	/* Initialization to silence warnings */
	const char *ackdata = NULL;
	PARSE_FIELD_STR(ackdata);
	if (!lose && strcmp(ackdata, "SENT"))
	    ZEPHYR_PRINT("/", str_to_lower(ackdata))
    }
    if (*z.sender) ZEPHYR_PRINT(" ", z.sender);

    if (!strcmp(z.class, "USER_LOCATE")) {
	if (!strcmp(z.opcode, "USER_HIDE"))
	    ND_PRINT(" hide");
	else if (!strcmp(z.opcode, "USER_UNHIDE"))
	    ND_PRINT(" unhide");
	else
	    ZEPHYR_PRINT(" locate ", z.inst);
	return;
    }

    if (!strcmp(z.class, "ZEPHYR_ADMIN")) {
	ZEPHYR_PRINT(" zephyr-admin ", str_to_lower(z.opcode));
	return;
    }

    if (!strcmp(z.class, "ZEPHYR_CTL")) {
	if (!strcmp(z.inst, "CLIENT")) {
	    if (!strcmp(z.opcode, "SUBSCRIBE") ||
		!strcmp(z.opcode, "SUBSCRIBE_NODEFS") ||
		!strcmp(z.opcode, "UNSUBSCRIBE")) {

		ND_PRINT(" %ssub%s", strcmp(z.opcode, "SUBSCRIBE") ? "un" : "",
				   strcmp(z.opcode, "SUBSCRIBE_NODEFS") ? "" :
								   "-nodefs");
		if (z.kind != Z_PACKET_SERVACK) {
		    /* Initialization to silence warnings */
		    const char *c = NULL, *i = NULL, *r = NULL;
		    PARSE_FIELD_STR(c);
		    PARSE_FIELD_STR(i);
		    PARSE_FIELD_STR(r);
		    if (!lose) ZEPHYR_PRINT(" ", z_triple(c, i, r));
		}
		return;
	    }

	    if (!strcmp(z.opcode, "GIMME")) {
		ND_PRINT(" ret");
		return;
	    }

	    if (!strcmp(z.opcode, "GIMMEDEFS")) {
		ND_PRINT(" gimme-defs");
		return;
	    }

	    if (!strcmp(z.opcode, "CLEARSUB")) {
		ND_PRINT(" clear-subs");
		return;
	    }

	    ZEPHYR_PRINT(" ", str_to_lower(z.opcode));
	    return;
	}

	if (!strcmp(z.inst, "HM")) {
	    ZEPHYR_PRINT(" ", str_to_lower(z.opcode));
	    return;
	}

	if (!strcmp(z.inst, "REALM")) {
	    if (!strcmp(z.opcode, "ADD_SUBSCRIBE"))
		ND_PRINT(" realm add-subs");
	    if (!strcmp(z.opcode, "REQ_SUBSCRIBE"))
		ND_PRINT(" realm req-subs");
	    if (!strcmp(z.opcode, "RLM_SUBSCRIBE"))
		ND_PRINT(" realm rlm-sub");
	    if (!strcmp(z.opcode, "RLM_UNSUBSCRIBE"))
		ND_PRINT(" realm rlm-unsub");
	    return;
	}
    }

    if (!strcmp(z.class, "HM_CTL")) {
	ZEPHYR_PRINT(" hm_ctl ", str_to_lower(z.inst));
	ZEPHYR_PRINT(" ", str_to_lower(z.opcode));
	return;
    }

    if (!strcmp(z.class, "HM_STAT")) {
	if (!strcmp(z.inst, "HMST_CLIENT") && !strcmp(z.opcode, "GIMMESTATS")) {
	    ND_PRINT(" get-client-stats");
	    return;
	}
    }

    if (!strcmp(z.class, "WG_CTL")) {
	ZEPHYR_PRINT(" wg_ctl ", str_to_lower(z.inst));
	ZEPHYR_PRINT(" ", str_to_lower(z.opcode));
	return;
    }

    if (!strcmp(z.class, "LOGIN")) {
	if (!strcmp(z.opcode, "USER_FLUSH")) {
	    ND_PRINT(" flush_locs");
	    return;
	}

	if (!strcmp(z.opcode, "NONE") ||
	    !strcmp(z.opcode, "OPSTAFF") ||
	    !strcmp(z.opcode, "REALM-VISIBLE") ||
	    !strcmp(z.opcode, "REALM-ANNOUNCED") ||
	    !strcmp(z.opcode, "NET-VISIBLE") ||
	    !strcmp(z.opcode, "NET-ANNOUNCED")) {
	    ZEPHYR_PRINT(" set-exposure ", str_to_lower(z.opcode));
	    return;
	}
    }

    if (!*z.recipient)
	z.recipient = "*";

    ZEPHYR_PRINT(" to ", z_triple(z.class, z.inst, z.recipient));
    if (*z.opcode)
	ZEPHYR_PRINT(" op ", z.opcode);
    return;

invalid:
    nd_print_invalid(ndo);
}
