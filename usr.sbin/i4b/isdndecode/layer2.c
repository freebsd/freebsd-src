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
 *	layer2.c - decode and print layer 2 (Q.921) information
 *	-------------------------------------------------------
 *
 *	$Id: layer2.c,v 1.5 1999/12/13 21:25:25 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:50:41 1999]
 *
 *---------------------------------------------------------------------------*/

#include "decode.h"
                
/*---------------------------------------------------------------------------*
 *	decode poll bit
 *---------------------------------------------------------------------------*/
static void
poll(int layer, char *buffer, int cnt, unsigned char value, unsigned char mask)
{
	sprintline(layer, buffer, cnt, value, mask, "P/F, Poll = %s", (value & mask) ? "Immediate Response Required" : "No Immediate Response Required");
}	

/*---------------------------------------------------------------------------*
 *	decode final bit
 *---------------------------------------------------------------------------*/
static void
final(int layer, char *buffer, int cnt, unsigned char value, unsigned char mask)
{
	sprintline(layer, buffer, cnt, value, mask, "P/F, Final = %s", (value & mask) ? "Result of Poll" : "No Result of Poll");
}	

/*---------------------------------------------------------------------------*
 *	decode protocol specified in Q.921
 *---------------------------------------------------------------------------*/
int
layer2(char *pbuf, unsigned char *buf, int dir, int printit)
{
	int sap, tei, cmd;
	int cnt = 0;
	char locbuf[32000];
	char *lbufp = &locbuf[0];
	char buffer[80];
	
	*lbufp = '\0';
	*pbuf = '\0';
	
	/* address high */

	sap = (buf[0] >> 2) & 0x3f;

	if(sap == 0)
		strcpy(buffer, "Call Control");
	else if((sap >= 1) && (sap <= 15))
		strcpy(buffer, "Reserved");
	else if(sap == 16)
		strcpy(buffer, "X.25");
	else if((sap >= 17) && (sap <= 31))
		strcpy(buffer, "Reserved");
	else if(sap == 63)
		strcpy(buffer, "Layer 2 Management");
	else
		strcpy(buffer, "Not available for Q.921");
	sprintline(2, lbufp+strlen(lbufp), cnt, buf[0], 0xfc, "SAPI = %d (%s)", sap, buffer);

	if(dir == FROM_TE)
		cmd = !(buf[0] & 0x02);
	else
		cmd = buf[0] & 0x02;

	sprintline(2, lbufp+strlen(lbufp), cnt, buf[0], 0x02, "C/R = %s", cmd ? "Command" : "Response");
	extension(2, lbufp+strlen(lbufp), cnt, buf[0], 0x01);
	cnt++;

	/* address low */

	tei = buf[1] >> 1;

	if((tei >= 0) && (tei <= 63))
		strcpy(buffer, "Non-automatic TEI");
	else if((tei >= 64) && (tei <= 126))
		strcpy(buffer, "Automatic TEI");
	if(tei == 127)
		strcpy(buffer, "Group TEI");
		
	sprintline(2, lbufp+strlen(lbufp), cnt, buf[1], 0xfe, "TEI = %d (%s)", tei, buffer);
	extension(2, lbufp+strlen(lbufp), cnt, buf[1], 0x01);
	cnt++;

	/* control 1 */
	
	if((buf[2] & 0x03) == 0x03)
	{
		/* U-frame */

		if((buf[2] & 0xef) == 0x6f)
		{
			/* SABME */
			
			sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xef, "U-Frame: SABME (Set Asynchonous Balanced Mode)");
			poll(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			cnt++;
		}
		else if((buf[2] & 0xef) == 0x0f)
		{
			/* DM */

			sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xef, "U-Frame: DM (Disconnected Mode)");
			final(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			cnt++;
		}
		else if((buf[2] & 0xef) == 0x03)
		{
			/* UI */

			sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xef, "U-Frame: UI (Unnumbered Information)");
			poll(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			cnt++;
	
			if(sap == 63 && (buf[3] == 0x0f))	/* TEI management */
			{
				sprintline(2, lbufp+strlen(lbufp), cnt, buf[3], 0xff, "MEI (Management Entity Identifier)");
				cnt++;
				sprintline(2, lbufp+strlen(lbufp), cnt, buf[4], 0xff, "Ri = 0x%04x (Reference number high)", (buf[4] << 8) | buf[5]);
				cnt++;
				sprintline(2, lbufp+strlen(lbufp), cnt, buf[5], 0xff, "Ri (Reference Number low)");
				cnt++;
	
				switch(buf[6])
				{
					case 0x01:
						strcpy(buffer, "Identity Request");
						break;
					case 0x02:
						strcpy(buffer, "Identity Assigned");
						break;
					case 0x03:
						strcpy(buffer, "Identity denied");
						break;
					case 0x04:
						strcpy(buffer, "Identity Check Request");
						break;
					case 0x05:
						strcpy(buffer, "Identity Check Response");
						break;
					case 0x06:
						strcpy(buffer, "Identity Remove");
						break;
					case 0x07:
						strcpy(buffer, "Identity Verify");
						break;
					default:
						strcpy(buffer, "undefined");
						break;
				}
				
				sprintline(2, lbufp+strlen(lbufp), cnt, buf[6], 0xff, "TEI %s (Message Type %d)", buffer, buf[6]);
				cnt++;
	
				switch(buf[6])
				{
					case 0x01:
						strcpy(buffer, "Any TEI value acceptable");
						break;
					case 0x02:
						strcpy(buffer, "");
						break;
					case 0x03:
						strcpy(buffer, "No TEI Value available");
						break;
					case 0x04:
						strcpy(buffer, "Check all TEI values");
						break;
					case 0x05:
						strcpy(buffer, "");
						break;
					case 0x06:
						strcpy(buffer, "Request for removal of all TEI values");
						break;
					case 0x07:
						strcpy(buffer, "");
						break;
					default:
						strcpy(buffer, "");
						break;
				}
				if(((buf[7] >> 1) & 0x7f) == 127)
					sprintline(2, lbufp+strlen(lbufp), cnt, buf[7], 0xfe, "Ai = %d (Action Indicator = %s)", (buf[7] >> 1) & 0x7f, buffer);
				else
					sprintline(2, lbufp+strlen(lbufp), cnt, buf[7], 0xfe, "Ai = %d (Action Indicator)", (buf[7] >> 1) & 0x7f);
				extension(2, lbufp+strlen(lbufp), cnt, buf[7], 0x01);
				cnt++;
			}
		}
		else if((buf[2] & 0xef) == 0x43)
		{
			/* DISC */

			sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xef, "U-Frame: DISC (Disconnect)");
			poll(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			cnt++;
		}
		else if((buf[2] & 0xef) == 0x63)
		{
			/* UA */

			sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xef, "U-Frame: UA (Unnumbered Acknowledge)");
			final(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			cnt++;
		}
		else if((buf[2] & 0xef) == 0x87)
		{
			/* FRMR */

			sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xef, "U-Frame: FRMR (Frame Reject)");
			final(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			cnt++;
		}
		else if((buf[2] & 0xef) == 0x9f)
		{
			/* XID */

			sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xef, "U-Frame: XID (Exchange Identification)");
			if(cmd)			
				poll(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			else
				final(2, lbufp+strlen(lbufp), cnt, buf[2], 0x10);
			cnt++;
		}
		
	}
	else if((buf[2] & 0x03) == 0x01)
	{
		/* S-frame */

		if(buf[2] == 0x01)
			strcpy(buffer, "RR (Receiver Ready)");
		else if(buf[2] == 0x05)
			strcpy(buffer, "RNR (Receiver Not Ready)");
		else if(buf[2] == 0x09)
			strcpy(buffer, "REJ (Reject)");
		else
			strcpy(buffer, "Unknown");
			
		sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xff, "S-Frame: %s", buffer);
		cnt++;
		
		sprintline(2, lbufp+strlen(lbufp), cnt, buf[3], 0xfe, "N(R) = %d (receive sequence number)", (buf[3] >> 1) & 0x7f);
		if(cmd)		
			poll(2, lbufp+strlen(lbufp), cnt, buf[3], 0x01);
		else
			final(2, lbufp+strlen(lbufp), cnt, buf[3], 0x01);
		cnt++;
		
	}
	else if((buf[2] & 0x01) == 0x00)
	{
		/* I-frame */

		sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0xfe, "N(S) = %d (send sequence number)", (buf[2] >> 1) & 0x7f);
		sprintline(2, lbufp+strlen(lbufp), cnt, buf[2], 0x01, "I-Frame: Information transfer");
		cnt++;
		
		sprintf(buffer, "N(R) = %d", (buf[3] >> 1) & 0x7f);
		sprintline(2, lbufp+strlen(lbufp), cnt, buf[3], 0xfe, "N(R) = %d (receive sequence number)", (buf[3] >> 1) & 0x7f);
		poll(2, lbufp+strlen(lbufp), cnt, buf[3], 0x01);
		cnt++;

	}

	sprintf((pbuf+strlen(pbuf)),"%s", &locbuf[0]);
	return (cnt);
}

/* EOF */
