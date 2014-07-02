/*
 * Decode and print Zephyr packets.
 *
 *	http://web.mit.edu/zephyr/doc/protocol
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

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-zephyr.c,v 1.10 2007-08-09 18:47:27 hannes Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "interface.h"

struct z_packet {
    packetbody_t version;
    int numfields;
    int kind;
    packetbody_t uid;
    int port;
    int auth;
    int authlen;
    packetbody_t authdata;
    packetbody_t class;
    packetbody_t inst;
    packetbody_t opcode;
    packetbody_t sender;
    packetbody_t recipient;
    packetbody_t format;
    int cksum;
    int multi;
    packetbody_t multi_uid;
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

static struct tok z_types[] = {
    { Z_PACKET_UNSAFE,		"unsafe" },
    { Z_PACKET_UNACKED,		"unacked" },
    { Z_PACKET_ACKED,		"acked" },
    { Z_PACKET_HMACK,		"hm-ack" },
    { Z_PACKET_HMCTL,		"hm-ctl" },
    { Z_PACKET_SERVACK,		"serv-ack" },
    { Z_PACKET_SERVNAK,		"serv-nak" },
    { Z_PACKET_CLIENTACK,	"client-ack" },
    { Z_PACKET_STAT,		"stat" }
};

char z_buf[256];

static packetbody_t
parse_field(packetbody_t *pptr, int *len)
{
    packetbody_t s;

    if (*len <= 0 || !pptr || !*pptr)
	return NULL;
    if (*pptr > snapend)
	return NULL;

    s = *pptr;
    while (*pptr <= snapend && *len >= 0 && **pptr) {
	(*pptr)++;
	(*len)--;
    }
    (*pptr)++;
    (*len)--;
    if (*len < 0 || *pptr > snapend)
	return NULL;
    return s;
}

static const char *
z_triple(packetbody_t class, packetbody_t inst, packetbody_t recipient)
{
    char *class_str, *inst_str, *recipient_str;
    class_str = p_strdup(class);
    inst_str = p_strdup(inst);
    if (!*recipient)
	recipient_str = (char *)"*";
    else
	recipient_str = p_strdup(recipient);
    snprintf(z_buf, sizeof(z_buf), "<%s,%s,%s>", class_str, inst_str,
	recipient_str);
    z_buf[sizeof(z_buf)-1] = '\0';
    p_strfree(class_str);
    p_strfree(inst_str);
    if (!*recipient)
	p_strfree(recipient_str);
    return z_buf;
}

static const char *
str_to_lower(packetbody_t string)
{
    char *s;

    p_strncpy(z_buf, string, sizeof(z_buf));
    z_buf[sizeof(z_buf)-1] = '\0';

    s = z_buf;
    while (*s) {
	*s = tolower((unsigned char)(*s));
	s++;
    }

    return z_buf;
}

void
zephyr_print(packetbody_t cp, int length)
{
    char *buf;
    struct z_packet z;
    packetbody_t parse = cp;
    int parselen = length;
    packetbody_t s;
    int lose = 0;

    /* squelch compiler warnings */

    z.kind = 0;
    z.class = NULL;
    z.inst = NULL;
    z.opcode = NULL;
    z.sender = NULL;
    z.recipient = NULL;

#define PARSE_STRING				\
	s = parse_field(&parse, &parselen);	\
	if (!s) lose = 1;

#define PARSE_FIELD_INT(field)			\
	PARSE_STRING				\
	if (!lose) field = p_strtol(s, 0, 16);

#define PARSE_FIELD_STR(field)			\
	PARSE_STRING				\
	if (!lose) field = s;

    PARSE_FIELD_STR(z.version);
    if (lose) return;
    if (p_strcmp_static(z.version, "ZEPH"))
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

    if (lose) {
	printf(" [|zephyr] (%d)", length);
	return;
    }

    printf(" zephyr");
    if (p_strcmp_static(z.version+4, "0.2")) {
	buf = p_strdup(z.version+4);
	printf(" v%s", buf);
	p_strfree(buf);
	return;
    }

    printf(" %s", tok2str(z_types, "type %d", z.kind));
    if (z.kind == Z_PACKET_SERVACK) {
	/* Initialization to silence warnings */
	packetbody_t ackdata = NULL;
	PARSE_FIELD_STR(ackdata);
	if (!lose && p_strcmp_static(ackdata, "SENT"))
	    printf("/%s", str_to_lower(ackdata));
    }
    if (*z.sender) {
	buf = p_strdup(z.sender);
	printf(" %s", buf);
	p_strfree(buf);
    }

    if (!p_strcmp_static(z.class, "USER_LOCATE")) {
	if (!p_strcmp_static(z.opcode, "USER_HIDE"))
	    printf(" hide");
	else if (!p_strcmp_static(z.opcode, "USER_UNHIDE"))
	    printf(" unhide");
	else {
	    buf = p_strdup(z.inst);
	    printf(" locate %s", buf);
	    p_strfree(buf);
	}
	return;
    }

    if (!p_strcmp_static(z.class, "ZEPHYR_ADMIN")) {
	printf(" zephyr-admin %s", str_to_lower(z.opcode));
	return;
    }

    if (!p_strcmp_static(z.class, "ZEPHYR_CTL")) {
	if (!p_strcmp_static(z.inst, "CLIENT")) {
	    if (!p_strcmp_static(z.opcode, "SUBSCRIBE") ||
		!p_strcmp_static(z.opcode, "SUBSCRIBE_NODEFS") ||
		!p_strcmp_static(z.opcode, "UNSUBSCRIBE")) {

		printf(" %ssub%s", p_strcmp_static(z.opcode, "SUBSCRIBE") ? "un" : "",
				   p_strcmp_static(z.opcode, "SUBSCRIBE_NODEFS") ? "" :
								   "-nodefs");
		if (z.kind != Z_PACKET_SERVACK) {
		    /* Initialization to silence warnings */
		    packetbody_t c = NULL, i = NULL, r = NULL;
		    PARSE_FIELD_STR(c);
		    PARSE_FIELD_STR(i);
		    PARSE_FIELD_STR(r);
		    if (!lose) printf(" %s", z_triple(c, i, r));
		}
		return;
	    }

	    if (!p_strcmp_static(z.opcode, "GIMME")) {
		printf(" ret");
		return;
	    }

	    if (!p_strcmp_static(z.opcode, "GIMMEDEFS")) {
		printf(" gimme-defs");
		return;
	    }

	    if (!p_strcmp_static(z.opcode, "CLEARSUB")) {
		printf(" clear-subs");
		return;
	    }

	    printf(" %s", str_to_lower(z.opcode));
	    return;
	}

	if (!p_strcmp_static(z.inst, "HM")) {
	    printf(" %s", str_to_lower(z.opcode));
	    return;
	}

	if (!p_strcmp_static(z.inst, "REALM")) {
	    if (!p_strcmp_static(z.opcode, "ADD_SUBSCRIBE"))
		printf(" realm add-subs");
	    if (!p_strcmp_static(z.opcode, "REQ_SUBSCRIBE"))
		printf(" realm req-subs");
	    if (!p_strcmp_static(z.opcode, "RLM_SUBSCRIBE"))
		printf(" realm rlm-sub");
	    if (!p_strcmp_static(z.opcode, "RLM_UNSUBSCRIBE"))
		printf(" realm rlm-unsub");
	    return;
	}
    }

    if (!p_strcmp_static(z.class, "HM_CTL")) {
	printf(" hm_ctl %s", str_to_lower(z.inst));
	printf(" %s", str_to_lower(z.opcode));
	return;
    }

    if (!p_strcmp_static(z.class, "HM_STAT")) {
	if (!p_strcmp_static(z.inst, "HMST_CLIENT") && !p_strcmp_static(z.opcode, "GIMMESTATS")) {
	    printf(" get-client-stats");
	    return;
	}
    }

    if (!p_strcmp_static(z.class, "WG_CTL")) {
	printf(" wg_ctl %s", str_to_lower(z.inst));
	printf(" %s", str_to_lower(z.opcode));
	return;
    }

    if (!p_strcmp_static(z.class, "LOGIN")) {
	if (!p_strcmp_static(z.opcode, "USER_FLUSH")) {
	    printf(" flush_locs");
	    return;
	}

	if (!p_strcmp_static(z.opcode, "NONE") ||
	    !p_strcmp_static(z.opcode, "OPSTAFF") ||
	    !p_strcmp_static(z.opcode, "REALM-VISIBLE") ||
	    !p_strcmp_static(z.opcode, "REALM-ANNOUNCED") ||
	    !p_strcmp_static(z.opcode, "NET-VISIBLE") ||
	    !p_strcmp_static(z.opcode, "NET-ANNOUNCED")) {
	    printf(" set-exposure %s", str_to_lower(z.opcode));
	    return;
	}
    }

    if (!*z.recipient)
	z.recipient = (__capability char *)"*";

    printf(" to %s", z_triple(z.class, z.inst, z.recipient));
    if (*z.opcode) {
	buf = p_strdup(z.opcode);
	printf(" op %s", buf);
	p_strfree(buf);
    }
    return;
}
