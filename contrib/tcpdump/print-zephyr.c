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
    if (!PACKET_VALID(*pptr))
	return NULL;

    s = *pptr;
    /*
     * XXX-BD: OVERFLOW: Previous code incremented two past the end and
     * dereferenced one past.
     */
    while (PACKET_REMAINING(*pptr) && *len >= 0 && **pptr) {
	(*pptr)++;
	(*len)--;
    }
    if (*len == 0 || !PACKET_REMAINING(*pptr))
	return NULL;
    (*pptr)++;
    (*len)--;
    return s;
}

static const char *
z_triple(packetbody_t class, packetbody_t inst, packetbody_t recipient)
{
    if (!*recipient)
	recipient = (__capability const char *)"*";
    snprintf(z_buf, sizeof(z_buf), "<%s,%s,%s>", class, inst, recipient);
    z_buf[sizeof(z_buf)-1] = '\0';
    return z_buf;
}

static const char *
str_to_lower(packetbody_t string)
{
    char *s;

    cstrncpy(cheri_ptr(z_buf, sizeof(z_buf)), string, sizeof(z_buf));
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
	if (!lose) field = cstrtol(s, 0, 16);

#define PARSE_FIELD_STR(field)			\
	PARSE_STRING				\
	if (!lose) field = s;

    PARSE_FIELD_STR(z.version);
    if (lose) return;
    if (cstrncmp(z.version, (__capability const char *)"ZEPH", 4))
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
    if (cstrncmp(z.version+4, (__capability const char *)"0.2", 3)) {
	printf(" v%s", z.version+4);
	return;
    }

    printf(" %s", tok2str(z_types, "type %d", z.kind));
    if (z.kind == Z_PACKET_SERVACK) {
	/* Initialization to silence warnings */
	packetbody_t ackdata = NULL;
	PARSE_FIELD_STR(ackdata);
	if (!lose && cstrcmp(ackdata, (__capability const char *)"SENT"))
	    printf("/%s", str_to_lower(ackdata));
    }
    if (*z.sender) printf(" %s", z.sender);

    if (!cstrcmp(z.class, (__capability const char *)"USER_LOCATE")) {
	if (!cstrcmp(z.opcode, (__capability const char *)"USER_HIDE"))
	    printf(" hide");
	else if (!cstrcmp(z.opcode, (__capability const char *)"USER_UNHIDE"))
	    printf(" unhide");
	else
	    printf(" locate %s", z.inst);
	return;
    }

    if (!cstrcmp(z.class, (__capability const char *)"ZEPHYR_ADMIN")) {
	printf(" zephyr-admin %s", str_to_lower(z.opcode));
	return;
    }

    if (!cstrcmp(z.class, (__capability const char *)"ZEPHYR_CTL")) {
	if (!cstrcmp(z.inst, (__capability const char *)"CLIENT")) {
	    if (!cstrcmp(z.opcode, (__capability const char *)"SUBSCRIBE") ||
		!cstrcmp(z.opcode, (__capability const char *)"SUBSCRIBE_NODEFS") ||
		!cstrcmp(z.opcode, (__capability const char *)"UNSUBSCRIBE")) {

		printf(" %ssub%s", cstrcmp(z.opcode, (__capability const char *)"SUBSCRIBE") ? "un" : "",
				   cstrcmp(z.opcode, (__capability const char *)"SUBSCRIBE_NODEFS") ? "" :
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

	    if (!cstrcmp(z.opcode, (__capability const char *)"GIMME")) {
		printf(" ret");
		return;
	    }

	    if (!cstrcmp(z.opcode, (__capability const char *)"GIMMEDEFS")) {
		printf(" gimme-defs");
		return;
	    }

	    if (!cstrcmp(z.opcode, (__capability const char *)"CLEARSUB")) {
		printf(" clear-subs");
		return;
	    }

	    printf(" %s", str_to_lower(z.opcode));
	    return;
	}

	if (!cstrcmp(z.inst, (__capability const char *)"HM")) {
	    printf(" %s", str_to_lower(z.opcode));
	    return;
	}

	if (!cstrcmp(z.inst, (__capability const char *)"REALM")) {
	    if (!cstrcmp(z.opcode, (__capability const char *)"ADD_SUBSCRIBE"))
		printf(" realm add-subs");
	    if (!cstrcmp(z.opcode, (__capability const char *)"REQ_SUBSCRIBE"))
		printf(" realm req-subs");
	    if (!cstrcmp(z.opcode, (__capability const char *)"RLM_SUBSCRIBE"))
		printf(" realm rlm-sub");
	    if (!cstrcmp(z.opcode, (__capability const char *)"RLM_UNSUBSCRIBE"))
		printf(" realm rlm-unsub");
	    return;
	}
    }

    if (!cstrcmp(z.class, (__capability const char *)"HM_CTL")) {
	printf(" hm_ctl %s", str_to_lower(z.inst));
	printf(" %s", str_to_lower(z.opcode));
	return;
    }

    if (!cstrcmp(z.class, (__capability const char *)"HM_STAT")) {
	if (!cstrcmp(z.inst, (__capability const char *)"HMST_CLIENT") && !cstrcmp(z.opcode, (__capability const char *)"GIMMESTATS")) {
	    printf(" get-client-stats");
	    return;
	}
    }

    if (!cstrcmp(z.class, (__capability const char *)"WG_CTL")) {
	printf(" wg_ctl %s", str_to_lower(z.inst));
	printf(" %s", str_to_lower(z.opcode));
	return;
    }

    if (!cstrcmp(z.class, (__capability const char *)"LOGIN")) {
	if (!cstrcmp(z.opcode, (__capability const char *)"USER_FLUSH")) {
	    printf(" flush_locs");
	    return;
	}

	if (!cstrcmp(z.opcode, (__capability const char *)"NONE") ||
	    !cstrcmp(z.opcode, (__capability const char *)"OPSTAFF") ||
	    !cstrcmp(z.opcode, (__capability const char *)"REALM-VISIBLE") ||
	    !cstrcmp(z.opcode, (__capability const char *)"REALM-ANNOUNCED") ||
	    !cstrcmp(z.opcode, (__capability const char *)"NET-VISIBLE") ||
	    !cstrcmp(z.opcode, (__capability const char *)"NET-ANNOUNCED")) {
	    printf(" set-exposure %s", str_to_lower(z.opcode));
	    return;
	}
    }

    if (!*z.recipient)
	z.recipient = (__capability const char *)"*";

    printf(" to %s", z_triple(z.class, z.inst, z.recipient));
    if (*z.opcode)
	printf(" op %s", z.opcode);
    return;
}
