/*
 * Copyright (c) 1996 Gary Jennejohn.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *---------------------------------------------------------------------------*
 *
 *	q.921.c - print Q.921 traces
 *	----------------------------
 *
 *	$Id: q921.c,v 1.4 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdntrace/q921.c,v 1.6 1999/12/14 21:07:50 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:56:46 1999]
 *
 *---------------------------------------------------------------------------*/

#include "trace.h"

/*---------------------------------------------------------------------------*
 *	decode LAPD (Q.921) protocol
 *---------------------------------------------------------------------------*/
int
decode_lapd(char *pbuf, int n, unsigned char *buf, int dir, int raw, int printit)
{
	int sap, tei, cmd, p_f;
	int cnt = 0;
	int i;
	char locbuf[32000];
	char *lbufp = &locbuf[0];

	*lbufp = '\0';
	*pbuf = '\0';
	
	sap = (buf[0] >> 2) & 0x3f;
	cnt++;

	tei = buf[1] >> 1;
	cnt++;

	if(dir == FROM_TE)
		cmd = !(buf[0] & 2);
	else
		cmd = buf[0] & 2;

	switch (sap)
	{
		/* SAPI control procedures */
		case 0:
		{
			if(printit)
				sprintf((lbufp+strlen(lbufp)), "Q921: SAP=%d (Call Control), %c, TEI=%d, ", sap, cmd?'C':'R', tei);

			if((buf[2] & 0x01) == 0)
			{
				if(printit)
					sprintf((lbufp+strlen(lbufp)), "I-Frame: ");

				p_f = buf [3] & 1;

				if(printit)
					sprintf((lbufp+strlen(lbufp)), "N(S) %d N(R) %d P %d ", buf [2] >> 1, buf [3] >> 1, p_f);

				cnt += 2;
			}
			else if((buf[2] & 0x03) == 0x01)
			{
				if(printit)
					sprintf((lbufp+strlen(lbufp)), "S-Frame: ");

				p_f = buf [3] & 1;
				cmd = buf [2] & 0x0c;

				if(printit)
				{
					if (cmd == 0)
						sprintf((lbufp+strlen(lbufp)), "RR N(R) %d PF %d ", buf [3] >> 1, p_f);
					if (cmd == 4)
						sprintf((lbufp+strlen(lbufp)), "RNR N(R) %d PF %d ", buf [3] >> 1, p_f);
					if (cmd == 8)
						sprintf((lbufp+strlen(lbufp)), "REJ N(R) %d PF %d ", buf [3] >> 1, p_f);
				}
				cnt += 2;
		  	}
		  	else if((buf[2] & 0x03) == 0x03)
			{
		  		if(printit)
					sprintf((lbufp+strlen(lbufp)), "U-Frame: ");

				p_f = (buf [2] & 0x10) >> 4;
				cmd = buf [2] & 0xec;

				if(printit)
				{
					if (cmd == 0x6c)
						sprintf((lbufp+strlen(lbufp)), "SABME PF %d ", p_f);
			  		if (cmd == 0x0c)
						sprintf((lbufp+strlen(lbufp)), "DM PF %d ", p_f);
			  		if (cmd == 0)
						sprintf((lbufp+strlen(lbufp)), "UI PF %d ", p_f);
			  		if (cmd == 0x40)
						sprintf((lbufp+strlen(lbufp)), "DISC PF %d ", p_f);
			  		if (cmd == 0x60)
						sprintf((lbufp+strlen(lbufp)), "UA PF %d ", p_f);
			  		if (cmd == 0x84)
						sprintf((lbufp+strlen(lbufp)), "FRMR PF %d ", p_f);
			  		if (cmd == 0xac)
						sprintf((lbufp+strlen(lbufp)), "XID PF %d ", p_f);
			  		/* information field ??? */
			  	}
		  		cnt++;
		  	}
			break;
		}

		/* D channel X.25 */
		
		case 16:
			if(printit)
				sprintf((lbufp+strlen(lbufp)), "Q921: SAP=%d (X.25), %c, TEI=%d, ", sap, cmd?'C':'R', tei);
			cnt = n;
			goto dump;				

		/* Loopback test */
		
		case 32:
			if(printit)
				sprintf((lbufp+strlen(lbufp)), "Q921: SAP=%d (Loopbacktest), %c, TEI=%d, ", sap, cmd?'C':'R', tei);
			cnt = n;
			goto dump;				

		/* SAPI layer 2 management functions */

		case 63:
		{
			if(printit)
				sprintf((lbufp+strlen(lbufp)), "Q921: SAP=%d (TEI-Management), %c, TEI=%d, ", sap, cmd?'C':'R', tei);

			if (tei != 127)
			{
				if(printit)
					sprintf((lbufp+strlen(lbufp)), "ILLEGAL TEI\n");
				cnt = n;
				goto dump;				
			}

			if (buf [2] != 3 && buf [3] != 0xf)
			{
				if(printit)
					sprintf((lbufp+strlen(lbufp)), "invalid format!\n");
				cnt = n;
				goto dump;				
			}
			cnt+= 2; /* UI + MEI */

			if(printit)
				sprintf((lbufp+strlen(lbufp)), "Ri=0x%04hx, ", *(short *)&buf[4]);
			cnt += 2; /* Ri */
			
			switch (buf[6])
			{
				case 1: 
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "IdRequest, Ai=%d", (buf [7] >> 1));
					cnt += 2;
					break;
		  		case 2:
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "IdAssign, Ai=%d", (buf [7] >> 1));
					cnt += 2;
					break;
		  		case 3:
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "IdDenied, Ai=%d", (buf [7] >> 1));
					cnt += 2;
					break;
		  		case 4:
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "IdCheckReq, Ai=%d", (buf [7] >> 1));
					cnt += 2;
					break;
		  		case 5:
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "IdCheckResp, Ai=%d", (buf [7] >> 1));
					cnt += 2;
					break;
		  		case 6:
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "IdRemove, Ai=%d", (buf [7] >> 1));
					cnt += 2;
					break;
		  		case 7:
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "IdVerify, Ai=%d", (buf [7] >> 1));
					cnt += 2;
					break;
		  		default:
					if(printit)
						sprintf((lbufp+strlen(lbufp)), "Unknown Msg Type\n");
					cnt = n;
					goto dump;				
	  		}
			break;
		}

		/* Illegal SAPI */
		
		default:
			if(printit)
				sprintf((lbufp+strlen(lbufp)), "Q921: ERROR, SAP=%d (Illegal SAPI), %c, TEI=%d\n", sap, cmd?'C':'R', tei);
			cnt = n;
			goto dump;				
	}

dump:	
	if(printit)
		sprintf((lbufp+strlen(lbufp)), "\n");

	if(raw && printit)
	{
		int j;
		for (i = 0; i < cnt; i += 16)
		{
			sprintf((pbuf+strlen(pbuf)),"Dump:%.3d  ", i);
			for (j = 0; j < 16; j++)
				if (i + j < cnt)
					sprintf((pbuf+strlen(pbuf)),"%02x ", buf[i + j]);
				else
					sprintf((pbuf+strlen(pbuf)),"   ");
			sprintf((pbuf+strlen(pbuf)),"      ");
			for (j = 0; j < 16 && i + j < cnt; j++)
				if (isprint(buf[i + j]))
					sprintf((pbuf+strlen(pbuf)),"%c", buf[i + j]);
				else
					sprintf((pbuf+strlen(pbuf)),".");
			sprintf((pbuf+strlen(pbuf)),"\n");
		}
	}

	sprintf((pbuf+strlen(pbuf)),"%s", &locbuf[0]);
	
	return (cnt);
}

/* EOF */
