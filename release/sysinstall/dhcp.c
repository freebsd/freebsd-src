/*
 * $FreeBSD$
 *
 * Copyright (c) 1999
 *	C. Stone.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY C. STONE ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL C STONE OR HIS BODILY PARASITES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE BY THE VOICES IN YOUR HEAD BEFOREHAND.
 *
 */

#include "sysinstall.h"

#include <ctype.h>

int
dhcpParseLeases(char *file, char *hostname, char *domain, char *nameserver,
		char *ipaddr, char *gateway, char *netmask)
{
    char tempbuf[1024];
    char optbuf[1024], *optname = NULL;
    char *tptr;
    int endedflag = 0;
    int leaseflag = 0;
    FILE *fp;
    enum { P_NOSTMT, P_NOSTMT1, P_STMT, P_STMTLINE } state;

    if ((fp = fopen(file, "r")) == NULL) {
	msgDebug("error opening file %s: %s\n", file, strerror(errno));
	return -1;
    }

    state = P_NOSTMT;
    while (fscanf(fp, "%1023s", tempbuf) > 0) {
    	switch (state) {
	case P_NOSTMT:
	    state = P_NOSTMT1;
	    if (!strncasecmp(tempbuf, "lease", 5)) {
		if (!leaseflag)
		    leaseflag = 1;
		else {
		    fclose(fp);
		    return 0;
		}
	    }
	    break;

	case P_NOSTMT1: 
	    if (tempbuf[0] != '{') {
		msgWarn("dhcpParseLeases: '{' expected");
		fclose(fp);
		return -1;
	    }
	    state = P_STMT;
	    break;

	case P_STMT:
	    if (!strncasecmp("option", tempbuf, 6))
		continue;
	    if (tempbuf[0] == '}') {
		state = P_NOSTMT;
		leaseflag = 0;
		continue;
	    }
	    if (!leaseflag) 
		break;
	    if (tempbuf[0] == ';') { 	/* play it safe */
		state = P_STMT;
		continue;
	    }
	    if ((tptr = (char *)strchr(tempbuf, ';')) && (*(tptr + 1) == 0)) {
		*tptr = NULL;
		endedflag = 1;
	    }
	    if (!isalnum(tempbuf[0])) {
		msgWarn("dhcpParseLeases: bad option");
		fclose(fp);
		return -1;
	    }
	    if (optname)
		free(optname);
	    optname = strdup(tempbuf);
	    if (endedflag) {
		state = P_STMT;
		endedflag = 0;
		continue;
	    }
	    state = P_STMTLINE;
	    break;

	case P_STMTLINE:
	    if (tempbuf[0] == ';') {
		state = P_STMT;
		continue;
	    }
	    if ((tptr = (char *)strchr(tempbuf, ';')) && (*(tptr + 1) == 0)) {
		*tptr = NULL;
		endedflag = 1;
	    }
	    if (tempbuf[0] == '"') {
		if (sscanf(tempbuf, "\"%[^\" ]\"", optbuf) < 1) {
		    msgWarn("dhcpParseLeases: bad option value");
		    fclose(fp);
		    return -1;
		}
	    }
	    else
		strcpy(optbuf, tempbuf);

	    if (!strcasecmp("host-name", optname)) {
		strcpy(hostname, optbuf);
	    } else if (!strcasecmp("domain-name", optname)) {
		strcpy(domain, optbuf);
	    } else if (!strcasecmp("fixed-address", optname)) {
		strcpy(ipaddr, optbuf);
	    } else if (!strcasecmp("routers", optname)) {
		if((tptr = (char *)strchr(optbuf, ',')))
		    *tptr = NULL;
		strcpy(gateway, optbuf);
	    } else if (!strcasecmp("subnet-mask", optname)) {
		strcpy(netmask, optbuf);
	    } else if (!strcasecmp("domain-name-servers", optname)) {
		/* <jkh> ...one value per property */
		if((tptr = (char *)strchr(optbuf, ',')))
		    *tptr = NULL;
		strcpy(nameserver, optbuf);
	    }
	    if (endedflag) {
		state = P_STMT;
		endedflag = 0;
		continue;
	    }
	    break;
	}
    }
    fclose(fp);
    return 0;
}
