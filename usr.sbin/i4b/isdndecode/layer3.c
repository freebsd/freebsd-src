/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	layer3.c - decode and print layer 3 (Q.931) information
 *	-------------------------------------------------------
 *
 *	$Id: layer3.c,v 1.9 2000/02/21 15:17:17 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Feb 21 15:54:58 2000]
 *
 *---------------------------------------------------------------------------*/

#include "decode.h"

char *mttab[] = {

/* 0x00 */			/* call establishment group */

	"ESCAPE",
	"ALERTING",
	"CALL PROCEEDING",
	"PROGRESS",
	"undefined (0x04)",
	"SETUP",
	"undefined (0x06)",
	"CONNECT",
	"undefined (0x08)",
	"undefined (0x09)",
	"undefined (0x0a)",
	"undefined (0x0b)",
	"undefined (0x0c)",
	"SETUP ACKNOWLEDGE",
	"undefined (0x0e)",
	"CONNECT ACKNOWLEDGE",

/* 0x10 */
	"undefined (0x10)",
	"undefined (0x11)",
	"undefined (0x12)",
	"undefined (0x13)",
	"undefined (0x14)",
	"undefined (0x15)",
	"undefined (0x16)",
	"undefined (0x17)",
	"undefined (0x18)",
	"undefined (0x19)",
	"undefined (0x1a)",
	"undefined (0x1b)",
	"undefined (0x1c)",
	"undefined (0x1d)",
	"undefined (0x1e)",
	"undefined (0x1f)",

/* 0x20 */	
		
	"USER INFORMATION", 		/* call information phase */
	"SUSPEND REJECT",
	"RESUME REJECT",
	"undefined (0x23)",
	"HOLD",
	"SUSPEND",
	"RESUME",
	"undefined (0x27)",
	"HOLD ACKNOWLEDGE",
	"undefined (0x29)",
	"undefined (0x2a)",
	"undefined (0x2b)",
	"undefined (0x2c)",
	"SUSPEND ACKNOWLEDGE",
	"RESUME ACKNOWLEDGE",
	"undefined (0x2f)",

/* 0x30 */

	"HOLD REJECT",
	"RETRIEVE",
	"undefined (0x32)",	
	"RETRIEVE ACKNOWLEDGE",
	"undefined (0x34)",
	"undefined (0x35)",
	"undefined (0x36)",
	"RETRIEVE REJECT",
	"undefined (0x38)",
	"undefined (0x39)",
	"undefined (0x3a)",
	"undefined (0x3b)",
	"undefined (0x3c)",
	"undefined (0x3d)",
	"undefined (0x3e)",
	"undefined (0x3f)",

/* 0x40 */	
	
	"DETACH",			/* call clearing */
	"undefined (0x41)",
	"undefined (0x42)",
	"undefined (0x43)",
	"undefined (0x44)",
	"DISCONNECT",
	"RESTART",
	"undefined (0x47)",
	"DETACH ACKNOWLEDGE",
	"undefined (0x49)",
	"undefined (0x4a)",
	"undefined (0x4b)",
	"undefined (0x4c)",
	"RELEASE",
	"RESTART ACKNOWLEDGE",
	"undefined (0x4f)",

/* 0x50 */

	"undefined (0x50)",
	"undefined (0x51)",
	"undefined (0x52)",
	"undefined (0x53)",
	"undefined (0x54)",
	"undefined (0x55)",
	"undefined (0x56)",
	"undefined (0x57)",
	"undefined (0x58)",
	"undefined (0x59)",
	"RELEASE COMPLETE",
	"undefined (0x5b)",
	"undefined (0x5c)",
	"undefined (0x5d)",
	"undefined (0x5e)",
	"undefined (0x5f)",

/* 0x60 */	

	"SEGMENT", 			/* misc messages */
	"undefined (0x61)",
	"FACILITY",
	"undefined (0x63)",
	"REGISTER",
	"undefined (0x65)",
	"undefined (0x66)",
	"undefined (0x67)",
	"CANCEL ACKNOWLEDGE",
	"undefined (0x69)",
	"FACILITY ACKNOWLEDGE",
	"undefined (0x6b)",
	"REGISTER ACKNOWLEDGE",
	"undefined (0x6d)",
 	"NOTIFY",
 	"undefined (0x6f)",

/* 0x70 */

	"CANCEL REJECT",
	"undefined (0x71)",
	"FACILITY REJECT",
	"undefined (0x73)",
	"REGISTER REJECT",
	"STATUS ENQIRY",
	"undefined (0x76)",
	"undefined (0x77)",
	"undefined (0x78)",
	"CONGESTION CONTROL",
	"undefined (0x7a)",
	"INFORMATION",
	"undefined (0x7c)",
	"STATUS",
	"undefined (0x7e)",
	"undefined (0x7f)",
};

#define MTTAB_MAX 0x7f

extern int f_null(char *pbuf, unsigned char *buf, int off);
extern int f_bc(char *pbuf, unsigned char *buf, int off);
extern int f_cause(char *pbuf, unsigned char *buf, int off);
extern int f_cstat(char *pbuf, unsigned char *buf, int off);
extern int f_chid(char *pbuf, unsigned char *buf, int off);
extern int f_fac(char *pbuf, unsigned char *buf, int off);
extern int f_progi(char *pbuf, unsigned char *buf, int off);
extern int f_displ(char *pbuf, unsigned char *buf, int off);
extern int f_date(char *pbuf, unsigned char *buf, int off);
extern int f_cnu(char *pbuf, unsigned char *buf, int off);
extern int f_cgpn(char *pbuf, unsigned char *buf, int off);
extern int f_cdpn(char *pbuf, unsigned char *buf, int off);
extern int f_hlc(char *pbuf, unsigned char *buf, int off);
extern int f_uu(char *pbuf, unsigned char *buf, int off);

struct ie {
	unsigned char code;	/* information element identifier code */
	char *name;		/* ie name */
	int (*func) (char *pbuf, unsigned char *buf, int off); /* decode function */
} ietab[] = {
	{ 0x00, "segmented message",		f_null },
	{ 0x04, "bearer capability",		f_bc },
	{ 0x08, "cause", 			f_cause },
	{ 0x0c, "connected address",		f_null },
	{ 0x0d, "extended facility",		f_null },
	{ 0x10, "call identity",		f_null },
	{ 0x14, "call state",			f_cstat },
	{ 0x18,	"channel id",			f_chid },
	{ 0x19, "data link connection id",	f_null },
	{ 0x1c, "facility",			f_fac },
	{ 0x1e, "progress indicator",		f_progi },
	{ 0x20, "network specific facilities",	f_null },
	{ 0x24, "terminal capabilities",	f_null },
	{ 0x27, "notification indicator",	f_null },
	{ 0x28, "display",			f_displ },
	{ 0x29, "date/time",			f_date },
	{ 0x2c, "keypad",			f_null },
	{ 0x30, "keypad echo", 			f_null },
	{ 0x32, "information request",		f_null },
	{ 0x34, "signal",			f_null },
	{ 0x36, "switchhook",			f_null },
	{ 0x38, "feature activation",		f_null },
	{ 0x39, "feature indication",		f_null },
	{ 0x3a, "service profile id",		f_null },
	{ 0x3b, "endpoint identifier",		f_null },
	{ 0x40, "information rate",		f_null },
	{ 0x41, "precedence level",		f_null },
	{ 0x42, "end-to-end transit delay",	f_null },
	{ 0x43, "transit delay detection",	f_null },
	{ 0x44, "packet layer binary parms",	f_null },
	{ 0x45, "packet layer window size",	f_null },
	{ 0x46, "packet size",			f_null },
	{ 0x47, "closed user group",		f_null },
	{ 0x48, "link layer core parameters",	f_null },
	{ 0x49, "link layer protocol parms",	f_null },
	{ 0x4a, "reverse charging information",	f_null },
	{ 0x4c, "connected number",		f_cnu },
	{ 0x4d, "connected subaddress",		f_null },
	{ 0x50, "X.213 priority",		f_null },
	{ 0x51, "report type",			f_null },
	{ 0x53, "link integrity verification",	f_null },
	{ 0x57, "PVC status",			f_null },
	{ 0x6c, "calling party number",		f_cnu },
	{ 0x6d, "calling party subaddress",	f_null },
	{ 0x70, "called party number",		f_cnu },
	{ 0x71, "called party subaddress",	f_null },
	{ 0x74, "redirecting number",		f_null },
	{ 0x78, "transit network selection",	f_null },
	{ 0x79, "restart indicator",		f_null },
	{ 0x7c, "low layer compatibility",	f_null },
	{ 0x7d, "high layer compatibility",	f_hlc },
	{ 0x7e, "user-user",			f_uu },
	{ 0x7f, "escape for extension",		f_null },
	{ 0xff, "unknown information element",	f_null }	
};

/*---------------------------------------------------------------------------*
 *	decode Q.931 protocol
 *---------------------------------------------------------------------------*/
void
layer3(char *pbuf, int n, int off, unsigned char *buf)
{
	char buffer[256];
	int codeset = 0;
	int codelock = 0;
	int oldcodeset = 0;
	
	int pd;
	int len;
	int j;
	int i;

	if(n <= 0)
		return;

	*pbuf = '\0';
	
	i = 0;
		
	/* protocol discriminator */

	pd = buf[i];

	if(pd >= 0x00 && pd <= 0x07)
		sprintf(buffer, "User-User IE (0x%02x)",pd);
	else if(pd == 0x08)
		sprintf(buffer, "Q.931/I.451");
	else if(pd >= 0x10 && pd <= 0x3f)
		sprintf(buffer, "Other Layer 3 or X.25 (0x%02x)",pd);
	else if(pd >= 0x40 && pd <= 0x4f)
		sprintf(buffer, "National Use (0x%02x)",pd);
	else if(pd >= 0x50 && pd <= 0xfe)
		sprintf(buffer, "Other Layer 3 or X.25 (0x%02x)",pd);
	else
		sprintf(buffer, "Reserved (0x%02x)",pd);

	sprintline(3, (pbuf+strlen(pbuf)), off+i, pd, 0xff, "Protocol discriminator = %s", buffer);
	i++;

	if(pd != 0x08)
	{
		for (; i < n;i++)
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "-");
		return;
	}
	
	/* call reference */

	len = buf[i] & 0x0f;
	
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xf0, "Call Reference");

	switch(len)
	{
		case 0:
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Length of Call Reference = 0 (Dummy CR)");
			break;
		case 1:
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Length of Call Reference = 1");
			i++;
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x80, "Call Reference sent %s origination side", (buf[i] & 0x80) ? "to" : "from");
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Call Reference = %d = 0x%02x", (buf[i] & 0x7f), (buf[i] & 0x7f));
			break;
		case 2:
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Length of Call Reference = 2");
			i++;
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x80, "Call Reference sent %s origination side", (buf[i] & 0x80) ? "to" : "from");
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Call reference = %d = %02x", (buf[i] & 0x7f), (buf[i] & 0x7f));
			i++;
			sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "Call reference = %d = %02x", (buf[i]), (buf[i]));
			break;
	}
	i++;	

	/* message type */	

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x80, "Message type extension = %d", buf[i] & 0x80 ? 1 : 0);

	if(buf[i] <= MTTAB_MAX)
		strcpy(buffer, mttab[buf[i]]);
	else
		sprintf(buffer, "unknown (0x%02x)", buf[i]);

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Message type = %s", buffer);
	i++;
	
	/* information elements */
	
	for (; i < n;)
	{
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x80, "%s Information element", buf[i] & 0x80 ? "Single octet" : "Variable length");

		if(buf[i] & 0x80)
		{
			/* single octett info element type 1 */

			if((buf[i] & 0x70) == 0x00)
			{
				strcpy(buffer, "Reserved");
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x70, "Reserved");
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Reserved, content of IE");
			}
			else if((buf[i] & 0x70) == 0x10)
			{
				strcpy(buffer, "Shift");
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x70, "Shift");
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x08, "%s shift", buf[i] & 0x08 ? "Non-locking" : "Locking");

					switch(buf[i] & 0x07)
					{
						case 0:
							strcpy(buffer, "Not applicable");
							break;
						case 1:
						case 2:
						case 3:
							sprintf(buffer, "Reserved (%d)", buf[i] & 0x07);
							break;
						case 4:
							strcpy(buffer, "Codeset 4 (ISO/IEC)");
							break;
						case 5:
							strcpy(buffer, "Codeset 5 (National use)");
							break;
						case 6:
							strcpy(buffer, "Codeset 6 (Local network specific)");
							break;
						case 7:
							strcpy(buffer, "Codeset 7 (User specific)");
							break;
					}
					sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x07, "%s", buffer);
					break;
			}
			else if((buf[i] & 0x70) == 0x30)
			{
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x70, "Congestion Level");
				switch(buf[i] & 0x0f)
				{
					case 0x00:
						strcpy(buffer, "receiver ready");
						break;
					case 0x0f:
						strcpy(buffer, "receiver not ready");
						break;
					default:
						sprintf(buffer, "reserved (0x%02x)", buf[i] & 0x0f);
						break;
				}
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Congestion Level = ", buffer);
				break;
			}
			else if((buf[i] & 0x70) == 0x50)
			{
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x70, "Repeat Indicator");
				switch(buf[i] & 0x0f)
				{
					case 0x02:
						strcpy(buffer, "Prioritized list for selecting one possibility");
						break;
					default:
						sprintf(buffer, "reserved (0x%02x)", buf[i] & 0x0f);
						break;
				}
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Repeat indication = ", buffer);
				break;
			}
			
			/* single octett info element type 2 */

			else if((buf[i] & 0x7f) == 0x20)
			{
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "More data");
			}
			else if((buf[i] & 0x7f) == 0x21)
			{
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Sending complete");
			}
			else
			{
				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "UNKNOWN single octet IE = 0x%02x", buf[i]);
			}
			i++;	/* next */
		}
		else
		{
			if(codeset == 0)
			{
				struct ie *iep = &ietab[0];

				for(;;)
				{
					if((iep->code == buf[i]) ||
					   (iep->code == 0xff))
						break;
					iep++;
				}

				sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "IE = %s", iep->name);
				sprintline(3, pbuf+strlen(pbuf), off+i+1, buf[i+1], 0xff, "IE Length = %d", buf[i+1]);
				
				if(iep->func == f_null)
				{
				}
				else
				{
					i += (iep->func)(pbuf, &buf[i], off+i);
					goto next;
				}
			}
			else
			{
				sprintf((pbuf+strlen(pbuf)), "UNKNOWN CODESET=%d, IE=0x%02x", codeset, buf[i]);
			}

			i++;	/* index -> length */

			len = buf[i];

			sprintf((pbuf+strlen(pbuf)), "LEN=0x%02x, DATA=", len);			

			i++;	/* index -> 1st param */

			for(j = 0; j < len; j++)
			{
				sprintf((pbuf+strlen(pbuf)),"0x%02x ", buf[j+i]);
			}
	
			sprintf((pbuf+strlen(pbuf)),"]");

			i += len;

next:

			if(!codelock && (codeset != oldcodeset))
				codeset = oldcodeset;
		}
	}
/* 	sprintf((pbuf+strlen(pbuf)),"\n"); */
}

/* EOF */

