/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	layer3_subr.c - subroutines for IE decoding
 *	-------------------------------------------
 *
 *	$Id: layer3_subr.c,v 1.8 2000/02/21 15:17:17 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Feb 21 15:45:16 2000]
 *
 *---------------------------------------------------------------------------*/

#include "decode.h"

/*---------------------------------------------------------------------------*
 *	dummy function
 *---------------------------------------------------------------------------*/
int
f_null(char *pbuf, unsigned char *buf, int off)
{
	return(0);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
f_cstat(char *pbuf, unsigned char *buf, int off)
{
	int i = 0;
	int len = 0;
	char buffer[256];
	
	i++;
	len = buf[i];
	i++;
	sprintf((pbuf+strlen(pbuf)), "Std=");
	switch((buf[i] & 0x60) >> 5)
	{
		case 0:
			strcpy(buffer, "CCITT");
			break;
		case 1:
			strcpy(buffer, "ISO/IEC");
			break;
		case 2:
			strcpy(buffer, "National");
			break;
		case 3:
			strcpy(buffer, "Special");
			break;
	}
	sprintf((pbuf+strlen(pbuf)), ", State=");

	switch((buf[i] & 0x3f))
	{
		case 0:
			strcpy(buffer, "Null");
			break;
		case 1:
			strcpy(buffer, "Call initiated");
			break;
		case 2:
			strcpy(buffer, "Overlap sending");
			break;
		case 3:
			strcpy(buffer, "Outgoing call proceeding");
			break;
		case 4:
			strcpy(buffer, "Call delivered");
			break;
		case 6:
			strcpy(buffer, "Call present");
			break;
		case 7:
			strcpy(buffer, "Call received");
			break;
		case 8:
			strcpy(buffer, "Connect request");
			break;
		case 9:
			strcpy(buffer, "Incoming call proceeding");
			break;
		case 10:
			strcpy(buffer, "Active");
			break;
		case 11:
			strcpy(buffer, "Disconnect request");
			break;
		case 12:
			strcpy(buffer, "Disconnect indication");
			break;
		case 15:
			strcpy(buffer, "Suspend request");
			break;
		case 17:
			strcpy(buffer, "Resume request");
			break;
		case 19:
			strcpy(buffer, "Release request");
			break;
		case 22:
			strcpy(buffer, "Call abort");
			break;
		case 25:
			strcpy(buffer, "Overlap receiving");
			break;
		case 0x3d:
			strcpy(buffer, "Restart request");
			break;
		case 0x3e:
			strcpy(buffer, "Restart");
			break;
		default:
			strcpy(buffer, "ERROR: undefined/reserved");
			break;
	}
	sprintf((pbuf+strlen(pbuf)), "]");
	i++;
	return(i);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
f_chid(char *pbuf, unsigned char *buf, int off)
{
	int i = 0;
	int len = 0;
	char buffer[256];
	
	i++;
	len = buf[i];
	i++;

	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x40, "Interface Id present = %s", buf[i] & 0x40 ? "Yes" : "No");

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x20, "Interface Type = %s", buf[i] & 0x20 ? "Other (PRI)" : "BRI");

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x10, "Spare");

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x08, "Channel = %s", buf[i] & 0x08 ? "exclusive" : "preferred");

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x04, "Channel is%s the D-Channel", buf[i] & 0x04 ? "" : " not");	

	switch(buf[i] & 0x03)
	{
		case 0:
			strcpy(buffer, "no channel");
			break;
		case 1:
			strcpy(buffer, "B-1");
			break;
		case 2:
			strcpy(buffer, "B-2");
			break;
		case 3:
			strcpy(buffer, "any channel");
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x03, "Channel = %s", buffer);

	i++;
	return(i);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
f_fac(char *pbuf, unsigned char *buf, int off)
{
	return(q932_facility(pbuf, buf));
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
f_progi(char *pbuf, unsigned char *buf, int off)
{
	int i = 0;
	int len = 0;
	char buffer[256];
	
	i++;
	len = buf[i];
	i++;
	sprintf((pbuf+strlen(pbuf)), "Std=");
	switch((buf[i] & 0x60) >> 5)
	{
		case 0:
			strcpy(buffer, "CCITT");
			break;
		case 1:
			strcpy(buffer, "ISO/IEC");
			break;
		case 2:
			strcpy(buffer, "National");
			break;
		case 3:
			strcpy(buffer, "Local");
			break;
	}
	sprintf((pbuf+strlen(pbuf)), ", Loc=");

	switch((buf[i] & 0x0f))
	{
		case 0:
			strcpy(buffer, "User");
			break;
		case 1:
			strcpy(buffer, "Private network serving local user");
			break;
		case 2:
			strcpy(buffer, "Public network serving local user");
			break;
		case 3:
			strcpy(buffer, "Transit network");
			break;
		case 4:
			strcpy(buffer, "Public network serving remote user");
			break;
		case 5:
			strcpy(buffer, "Private network serving remote user");
			break;
		case 6:
			strcpy(buffer, "Network beyond interworking point");
			break;
		default:
			strcpy(buffer, "ERROR: undefined/reserved");
			break;
	}

	i++;

	sprintf((pbuf+strlen(pbuf)), "\n          Description");
	
	switch((buf[i] & 0x7f))
	{
		case 1:
			strcpy(buffer, "Call is not end-to-end ISDN");
			break;
		case 2:
			strcpy(buffer, "Destination address is non-ISDN");
			break;
		case 3:
			strcpy(buffer, "Origination address is non-ISDN");
			break;
		case 4:
			strcpy(buffer, "Call has returned to the ISDN");
			break;
		case 5:
			strcpy(buffer, "Interworking occured, Service change");
			break;
		case 8:
			strcpy(buffer, "In-band info or appropriate pattern now available");
			break;
		default:
			strcpy(buffer, "ERROR: undefined/reserved");
			break;
	}
	sprintf((pbuf+strlen(pbuf)), "]");
	i++;

	return(i);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
f_displ(char *pbuf, unsigned char *buf, int off)
{
	int i = 0;
	int j = 0;
	int len = 0;
	
	i++;
	len = buf[i];
	i++;
	for(j = 0; j < len; j++)
	{
		sprintf((pbuf+strlen(pbuf)),"%c", buf[j+i]);
	}
	sprintf((pbuf+strlen(pbuf)),"]");
	i += j;

	return(i);
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
int
f_date(char *pbuf, unsigned char *buf, int off)
{
	int i = 0;
	int j = 0;
	int len = 0;

	i++;
	len = buf[i];
	i++;

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "Year = %02d", buf[i]);
	i++;
	
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "Month = %02d", buf[i]);
	i++;
	
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "Day = %02d", buf[i]);
	i++;
	
	j=3;
	if(j < len)
	{
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "Hour = %02d", buf[i]);
		i++;
		j++;
	}
	if(j < len)
	{
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "Minute = %02d", buf[i]);
		i++;
		j++;
	}
	if(j < len)
	{
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "Second = %02d", buf[i]);
		i++;
		j++;
	}
	i += len;
	return(i);
}

/*---------------------------------------------------------------------------*
 *	decode and print the cause
 *---------------------------------------------------------------------------*/
int
f_cause(char *pbuf, unsigned char *buf, int off)
{
	int j;
	int len;
	int i = 0;
	int ls;
	char buffer[256];
	
	i++;	/* index -> length */

	len = buf[i];

	i++;	/* coding/location */
	len--;

	ls = buf[i];

	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);	

	switch((ls & 0x60) >> 5)
	{
		case 0:
			strcpy(buffer, "CCITT");
			break;
		case 1:
			strcpy(buffer, "ISO/IEC");
			break;
		case 2:
			strcpy(buffer, "National");
			break;
		case 3:
			strcpy(buffer, "Local");
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x60, "Coding Standard = %s", buffer);

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x10, "Spare");

	switch(ls & 0x0f)
	{
		case 0x00:
			strcpy(buffer, "user");
			break;
		case 0x01:
			strcpy(buffer, "private network serving local user");
			break;
		case 0x02:
			strcpy(buffer, "public network serving local user");
			break;
		case 0x03:
			strcpy(buffer, "transit network");
			break;
		case 0x04:
			strcpy(buffer, "public network serving remote user");
			break;
		case 0x05:
			strcpy(buffer, "private network serving remote user");
			break;
		case 0x07:
			strcpy(buffer, "international network");
			break;
		case 0x0a:
			strcpy(buffer, "network beyond interworking point");
			break;
		default:
			sprintf(buffer, "reserved (0x%02x)", ls & 0x0f);
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Location = %s", buffer);
	
	i++;
	len--;
	
	if(!(ls & 0x80))
	{
		extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);

		switch(buf[i] & 0x7f)
		{
			case 0:
				strcpy(buffer, "Q.931");
				break;
			case 3:
				strcpy(buffer, "X.21");
				break;
			case 4:
				strcpy(buffer, "X.25");
				break;
			case 5:
				strcpy(buffer, "Q.1031/Q.1051");
				break;
			default:
				strcpy(buffer, "Reserved");
				break;
		}
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Recommendation = %s", buffer);
		i++;
		len--;
	}

	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);
	
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Cause = %s", print_cause_q850(buf[i] & 0x7f));	
	
	i++;
	len--;
	
	for(j = 0; j < len; j++)
		sprintline(3, (pbuf+strlen(pbuf)), off+i+j, buf[i+j], 0xff, "Diagnostics = %02d %s", buf[i+j]);

	i += (len+1);
	
	return(i);
}

/*---------------------------------------------------------------------------*
 *	decode and print the bearer capability
 *---------------------------------------------------------------------------*/
int
f_bc(char *pbuf, unsigned char *buf, int off)
{
	int len;
	int i = 0;
	int mr = 0;
	char buffer[256];
	
	i++;	/* index -> length */

	len = buf[i];
	i++;

	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);
	
	switch((buf[i] & 0x60) >> 5)
	{
		case 0:
			strcpy(buffer, "CCITT");
			break;
		case 1:
			strcpy(buffer, "ISO/IEC");
			break;
		case 2:
			strcpy(buffer, "National");
			break;
		case 3:
			strcpy(buffer, "NSI Std");
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x60, "Coding standard = %s", buffer);	

	switch(buf[i] & 0x1f)
	{
		case 0x00:
			strcpy(buffer, "speech");
			break;
		case 0x08:
			strcpy(buffer, "unrestricted digital information");
			break;
		case 0x09:
			strcpy(buffer, "restricted digital information");
			break;
		case 0x10:
			strcpy(buffer, "3.1 kHz audio");
			break;
		case 0x11:
			strcpy(buffer, "unrestricted digital information with tones");
			break;
		case 0x18:
			strcpy(buffer, "video");
			break;
		default:
			sprintf(buffer, "reserved (0x%02x)", buf[i] & 0x0f);
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x1f, "Capability = %s", buffer);	

	i++;
	len--;

	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);
	
	switch((buf[i] & 0x60) >> 5)
	{
		case 0:
			strcpy(buffer, "circuit");
			break;
		case 2:
			strcpy(buffer, "packet");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "reserved (0x%02x)", ((buf[i] & 0x60) >> 5));
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x60, "Mode = %s", buffer);	
	
	switch(buf[i] & 0x1f)
	{
		case 0x00:
			strcpy(buffer, "packet mode");
			break;
		case 0x10:
			strcpy(buffer, "64 kbit/s");
			break;
		case 0x11:
			strcpy(buffer, "2 x 64 kbit/s");
			break;
		case 0x13:
			strcpy(buffer, "384 kbit/s");
			break;
		case 0x15:
			strcpy(buffer, "1536 kbit/s");
			break;
		case 0x17:
			strcpy(buffer, "1920 kbit/s");
			break;
		case 0x18:
			strcpy(buffer, "Multirate");
			mr = 1;
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "reserved (0x%02x)", buf[i] & 0x0f);
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x1f, "Rate = %s", buffer);

	i++;
	len--;

	if(!len)
		goto exit;
	
	if(mr)
	{
		extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x60, "Rate multiplier = %d", buf[i] & 0x7f);
		i++;
		len--;
	}

	if(!len)
		goto exit;
	
	switch(buf[i] & 0x1f)
	{
		case 0x01:
			strcpy(buffer, "V.110/X.30");
			break;
		case 0x02:
			strcpy(buffer, "G.711 u-Law");
			break;
		case 0x03:
			strcpy(buffer, "G.711 a-Law");
			break;
		case 0x04:
			strcpy(buffer, "G.721 ADPCM/I.460");
			break;
		case 0x05:
			strcpy(buffer, "H.221/H.242");
			break;
		case 0x07:
			strcpy(buffer, "non-CCITT rate adaption");
			break;
		case 0x08:
			strcpy(buffer, "V.120");
			break;
		case 0x09:
			strcpy(buffer, "X.31");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "reserved (0x%02x)", buf[i] & 0x1f);
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x1f, "Layer 1 Protocol = %s", buffer);
	
	i++;
	len--;
	
	if(!len)
		goto exit;

/* work to do ahead !!! */

	if(!(buf[i-1] & 0x80))
	{
		sprintf((pbuf+strlen(pbuf)), "\n          user rate=0x%02x ", buf[i] & 0x1f);

		if(buf[i] & 0x40)
			sprintf((pbuf+strlen(pbuf)), "(async,");
		else
			sprintf((pbuf+strlen(pbuf)), "(sync,");		

		if(buf[i] & 0x20)
			sprintf((pbuf+strlen(pbuf)), "in-band neg. possible)");
		else
			sprintf((pbuf+strlen(pbuf)), "in-band neg not possible)");
		
		i++;
		len--;
	}

	if(!len)
		goto exit;

	if(!(buf[i-1] & 0x80))
	{
		sprintf((pbuf+strlen(pbuf)), "\n          clk/flow=0x%02x", buf[i] & 0x1f);

		sprintf((pbuf+strlen(pbuf)), "\n          intermediate rate=");
		
		switch((buf[i] & 0x60) >> 5)
		{
			case 0:
				sprintf((pbuf+strlen(pbuf)), "not used");
				break;
			case 1:
				sprintf((pbuf+strlen(pbuf)), "8 kbit/s");
				break;
			case 2:
				sprintf((pbuf+strlen(pbuf)), "16 kbit/s");
				break;
			case 3:
				sprintf((pbuf+strlen(pbuf)), "32 kbit/s");
				break;
		}
		i++;
		len--;
	}

	if(!len)
		goto exit;

	if(!(buf[i-1] & 0x80))
	{
		sprintf((pbuf+strlen(pbuf)), "\n          hdr/mfrm/etc.=0x%02x", buf[i]);
		i++;
		len--;
	}

	if(!len)
		goto exit;

	if(!(buf[i-1] & 0x80))
	{
		sprintf((pbuf+strlen(pbuf)), "\n          stop/data/parity=0x%02x", buf[i]);
		i++;
		len--;
	}

	if(!len)
		goto exit;

	if(!(buf[i-1] & 0x80))
	{
		sprintf((pbuf+strlen(pbuf)), "\n          modemtype=0x%02x", buf[i]);
		i++;
		len--;
	}

	if(!len)
		goto exit;

	switch(buf[i] & 0x7f)
	{
		case 0x42:
			sprintf((pbuf+strlen(pbuf)), "\n          layer2=Q.921/I.441");
			break;
		case 0x46:
			sprintf((pbuf+strlen(pbuf)), "\n          layer2=X.25 link");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "\n          layer2=0x%02x",(buf[i] & 0x7f));
			break;
	}
	i++;
	len--;

	if(!len)
		goto exit;
	
	switch(buf[i] & 0x7f)
	{
		case 0x62:
			sprintf((pbuf+strlen(pbuf)), "\n          layer3=Q.921/I.441");
			break;
		case 0x66:
			sprintf((pbuf+strlen(pbuf)), "\n          layer3=X.25 packet");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "\n          layer3=0x%02x",(buf[i] & 0x7f));
			break;
	}
	i++;
	len--;

exit:	

	return(i);
}

/*---------------------------------------------------------------------------*
 *	decode and print the ISDN (telephone) number
 *---------------------------------------------------------------------------*/
int
f_cnu(char *pbuf, unsigned char *buf, int off)
{
	int j;
	int len;
	int i = 0;
	int tp;
	int ind = 0;
	char buffer[256];
	
	i++;	/* index -> length */
	len = buf[i];

	i++;	/* index -> type/plan */
	tp = buf[i];

	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);

	switch((tp & 0x70) >> 4)
	{
		case 0:
			strcpy(buffer, "Unknown");
			break;
		case 1:
			strcpy(buffer, "International number");
			break;
		case 2:
			strcpy(buffer, "National number");
			break;
		case 3:
			strcpy(buffer, "Network specific number");
			break;
		case 4:
			strcpy(buffer, "Subscriber number");
			break;
		case 6:
			strcpy(buffer, "Abbreviated number");
			break;
		default:
			sprintf(buffer, "Reserved (%d), ", ((tp & 0x70) >> 4));
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x60, "Type = %s", buffer);

	switch(tp & 0x0f)
	{
		case 0:
			strcpy(buffer, "Unknown");
			break;
		case 1:
			strcpy(buffer, "ISDN (E.164)");
			break;
		case 3:
			strcpy(buffer, "Data (X.121)");
			break;
		case 4:
			strcpy(buffer, "Telex (F.69)");
			break;
		case 8:
			strcpy(buffer, "National");
			break;
		case 9:
			strcpy(buffer, "Private");
			break;
		default:
			sprintf(buffer, "Reserved (%d)", (tp & 0x0f));
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x0f, "Plan = %s", buffer);

	i++;
	len--;
	
	if(!(tp & 0x80))
	{
		extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);

		switch((buf[i] & 0x60) >> 5)
		{
			case 0:
				strcpy(buffer, "allowed");
				break;
			case 1:
				strcpy(buffer, "restricted");
				break;
			case 2:
				strcpy(buffer, "number not available");
				break;
			case 3:
				strcpy(buffer, "reserved");
				break;
		}
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x60, "Presentation = %s", buffer);

		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x1c, "Spare");

		switch(ind & 0x03)
		{
			case 0:
				strcpy(buffer, "user provided, not screened");
				break;
			case 1:
				strcpy(buffer, "user provided, verified & passed");
				break;
			case 2:
				strcpy(buffer, "user provided, verified & failed");
				break;
			case 3:
				strcpy(buffer, "network provided");
				break;
		}
		sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x03, "Screening = %s", buffer);
		i++;
		len--;
	}

	for(j = 0; j < len; j++)
	{
		sprintline(3, (pbuf+strlen(pbuf)), off+i+j, buf[i+j], 0xff, "Number digit = %c", buf[i+j]);
	}		

	i += j;

	return(i);
}

/*---------------------------------------------------------------------------*
 *	decode and print HL comatibility
 *---------------------------------------------------------------------------*/
int
f_hlc(char *pbuf, unsigned char *buf, int off)
{
	int i = 0;
	int len = 0;
	char buffer[256];

	i++;
	len = buf[i];

	i++;
	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);
	
	switch((buf[i] >> 5) & 0x03)
	{
		case 0: strcpy(buffer, "CCITT");
			break;
		case 1: strcpy(buffer, "ISO/IEC");
			break;
		case 2: strcpy(buffer, "National");
			break;
		case 3: strcpy(buffer, "Network");
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x60, "Coding standard = %s", buffer);

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x1c, "Interpretation = %s", ((buf[i] >> 2) & 0x07) == 0x04 ? "first" : "reserved");

	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x03, "Presentation = %s", ((buf[i]) & 0x03) == 0x01 ? "High layer protocol profile" : "reserved");

	i++;
	len--;
	
	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);

	switch(buf[i] & 0x7f)
	{
		case 0x01:
			strcpy(buffer, "Telephony");
			break;
		case 0x04:
			strcpy(buffer, "Fax Group 2/3 (F.182)");
			break;
		case 0x21:
			strcpy(buffer, "Fax Group 4 I (F.184)");
			break;
		case 0x24:
			strcpy(buffer, "Teletex (F.230) or Fax Group 4 II/III (F.184)");
			break;
		case 0x28:
			strcpy(buffer, "Teletex (F.220)");
			break;
		case 0x31:
			strcpy(buffer, "Teletex (F.200)");
			break;
		case 0x32:
			strcpy(buffer, "Videotex (F.300/T.102)");
			break;
		case 0x33:
			strcpy(buffer, "Videotex (F.300/T.101)");
			break;
		case 0x35:
			strcpy(buffer, "Telex (F.60)");
			break;
		case 0x38:
			strcpy(buffer, "MHS (X.400)");
			break;
		case 0x41:
			strcpy(buffer, "OSI (X.200)");
			break;
		case 0x5e:
			strcpy(buffer, "Maintenance");
			break;
		case 0x5f:
			strcpy(buffer, "Management");
			break;
		case 0x60:
			strcpy(buffer, "Audio visual (F.721)");
			break;
		default:
			sprintf(buffer, "Reserved (0x%02x)", buf[i] & 0x7f);
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Characteristics = %s", buffer);
	i++;
	len--;

	if(buf[i-1] & 0x80)
	{
		return(i);
	}
	
	extension(3, pbuf+strlen(pbuf), off+i, buf[i], 0x80);

	switch(buf[i] & 0x7f)
	{
		case 0x01:
			strcpy(buffer, "Telephony");
			break;
		case 0x04:
			strcpy(buffer, "Fax Group 2/3 (F.182)");
			break;
		case 0x21:
			strcpy(buffer, "Fax Group 4 I (F.184)");
			break;
		case 0x24:
			strcpy(buffer, "Teletex (F.230) or Fax Group 4 II/III (F.184)");
			break;
		case 0x28:
			strcpy(buffer, "Teletex (F.220)");
			break;
		case 0x31:
			strcpy(buffer, "Teletex (F.200)");
			break;
		case 0x32:
			strcpy(buffer, "Videotex (F.300/T.102)");
			break;
		case 0x33:
			strcpy(buffer, "Videotex (F.300/T.101)");
			break;
		case 0x35:
			strcpy(buffer, "Telex (F.60)");
			break;
		case 0x38:
			strcpy(buffer, "MHS (X.400)");
			break;
		case 0x41:
			strcpy(buffer, "OSI (X.200)");
			break;
		case 0x5e:
			strcpy(buffer, "Maintenance");
			break;
		case 0x5f:
			strcpy(buffer, "Management");
			break;
		case 0x60:
			strcpy(buffer, "Audio visual (F.721)");
			break;
		default:
			sprintf(buffer, "Reserved (0x%02x)", buf[i] & 0x7f);
			break;

	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0x7f, "Ext. characteristics = %s", buffer);
	i++;
	return(i);
}

/*---------------------------------------------------------------------------*
 *	user-user
 *---------------------------------------------------------------------------*/
int
f_uu(char *pbuf, unsigned char *buf, int off)
{
	int j;
	int len;
	int i = 0;
	int pd;
	char buffer[256];
	
	i++;	/* index -> length */
	len = buf[i];

	i++;	/* index -> PD */
	pd = buf[i];

	switch(pd)
	{
		case 0:
			strcpy(buffer, "user-specific");
			break;
		case 1:
			strcpy(buffer, "OSI high layer");
			break;
		case 2:
			strcpy(buffer, "X.244");
			break;
		case 3:
			strcpy(buffer, "reserved for sys mgmt");
			break;
		case 4:
			strcpy(buffer, "IA5 characters");
			break;
		case 5:
			strcpy(buffer, "X.208/X.209");
			break;
		case 7:
			strcpy(buffer, "V.120");
			break;
		case 8:
			strcpy(buffer, "Q.931/I.451");
			break;
		default:
			if(pd >= 0x10 && pd <= 0x3f)
				sprintf(buffer, "reserved incl X.31 (0x%2x)", pd);
			else if (pd >= 0x40 && pd <= 0x4f)
				sprintf(buffer, "national use (0x%2x)", pd);
			else if (pd >= 0x50 && pd <= 0xfe)
				sprintf(buffer, "reserved incl X.31 (0x%2x)", pd);
			else
				sprintf(buffer, "reserved (0x%2x)", pd);
			break;
	}
	sprintline(3, (pbuf+strlen(pbuf)), off+i, buf[i], 0xff, "protocol = %s", buffer);

	i++;
	len--;
	
	for(j = 0; j < len; j++)
	{
		if(isprint(buf[i+j]))
			sprintline(3, (pbuf+strlen(pbuf)), off+i+j, buf[i+j], 0xff, "user information = %c", buf[i+j]);
		else
			sprintline(3, (pbuf+strlen(pbuf)), off+i+j, buf[i+j], 0xff, "user information = 0x%2x", buf[i+j]);
	}		

	i += j;

	return(i);
}

/* EOF */

