/*
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
 */

/* \summary: Real Time Streaming Protocol (RTSP) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"

static const char *rtspcmds[] = {
	"DESCRIBE",
	"ANNOUNCE",
	"GET_PARAMETER",
	"OPTIONS",
	"PAUSE",
	"PLAY",
	"RECORD",
	"REDIRECT",
	"SETUP",
	"SET_PARAMETER",
	"TEARDOWN",
	NULL
};

void
rtsp_print(netdissect_options *ndo, const u_char *pptr, u_int len)
{
	ndo->ndo_protocol = "rtsp";
	txtproto_print(ndo, pptr, len, rtspcmds, RESP_CODE_SECOND_TOKEN);
}
