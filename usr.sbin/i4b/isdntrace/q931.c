/*
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	q931.c - print Q.931 traces
 *	---------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 10 10:32:33 2001]
 *
 *---------------------------------------------------------------------------*/

#include "trace.h"

/*---------------------------------------------------------------------------*
 *	decode Q.931 protocol
 *---------------------------------------------------------------------------*/
void
decode_q931(char *pbuf, int n, int off, unsigned char *buf, int raw)
{
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
	
	if(raw)
	{
		for (i = 0; i < n; i += 16)
		{
			sprintf((pbuf+strlen(pbuf)),"Dump:%.3d  ", i+off);
			for (j = 0; j < 16; j++)
				if (i + j < n)
					sprintf((pbuf+strlen(pbuf)),"%02x ", buf[i + j]);
				else
					sprintf((pbuf+strlen(pbuf)),"   ");
			sprintf((pbuf+strlen(pbuf)),"      ");
			for (j = 0; j < 16 && i + j < n; j++)
				if (isprint(buf[i + j]))
					sprintf((pbuf+strlen(pbuf)),"%c", buf[i + j]);
				else
					sprintf((pbuf+strlen(pbuf)),".");
			sprintf((pbuf+strlen(pbuf)),"\n");
		}
	}

	i = 0;
		
	sprintf((pbuf+strlen(pbuf)), "Q931: ");

	/* protocol discriminator */

	pd = buf[i];

	if(pd >= 0x00 && pd <= 0x07)
		sprintf((pbuf+strlen(pbuf)), "pd=User-User (0x%02x)\n",pd);
	else if(pd == 0x08)
		sprintf((pbuf+strlen(pbuf)), "pd=Q.931/I.451, ");
	else if(pd >= 0x10 && pd <= 0x3f)
		sprintf((pbuf+strlen(pbuf)), "pd=Other Layer 3 or X.25 (0x%02x)\n",pd);
	else if(pd >= 0x40 && pd <= 0x4f)
		sprintf((pbuf+strlen(pbuf)), "pd=National Use (0x%02x)\n",pd);
	else if(pd >= 0x50 && pd <= 0xfe)
		sprintf((pbuf+strlen(pbuf)), "pd=Other Layer 3 or X.25 (0x%02x)\n",pd);
	else
		sprintf((pbuf+strlen(pbuf)), "pd=Reserved (0x%02x)\n",pd);

	/* call reference */

	i++;

	len = buf[i] & 0x0f;

	switch(len)
	{
		case 0:
			sprintf((pbuf+strlen(pbuf)), "cr=Dummy, ");
			break;
		case 1:
			sprintf((pbuf+strlen(pbuf)), "cr=0x%02x %s, ", (buf[i+1] & 0x7f), (buf[i+1] & 0x80) ? "(from destination)" : "(from origination)");
			break;
		case 2:
			sprintf((pbuf+strlen(pbuf)), "cr=0x%02x 0x%02x %s, ", (buf[i+1] & 0x7f), (buf[i+2] & 0x7f), (buf[i+1] & 0x80) ? "(org)" : "(dst)");
			break;
	}

	i += (len+1);
	
	/* message type */	

	sprintf((pbuf+strlen(pbuf)), "message=");

	switch(buf[i])
	{
		/* escape to nationally specific message type */

		case 0x00:
			sprintf((pbuf+strlen(pbuf)), "ESCAPE: ");
			break;

		/* call establishment */

		case 0x01:
			sprintf((pbuf+strlen(pbuf)), "ALERTING: ");
			break;
		case 0x02:
			sprintf((pbuf+strlen(pbuf)), "CALL PROCEEDING: ");
			break;
		case 0x03:
			sprintf((pbuf+strlen(pbuf)), "PROGRESS: ");
			break;
		case 0x05:
			sprintf((pbuf+strlen(pbuf)), "SETUP: ");
			break;
		case 0x07:
			sprintf((pbuf+strlen(pbuf)), "CONNECT: ");
			break;
		case 0x0d:
			sprintf((pbuf+strlen(pbuf)), "SETUP ACKNOWLEDGE: ");
			break;
		case 0x0f:
			sprintf((pbuf+strlen(pbuf)), "CONNECT ACKNOWLEDGE: ");
			break;

		/* call information phase */

		case 0x20:
			sprintf((pbuf+strlen(pbuf)), "USER INFORMATION: ");
			break;
		case 0x21:
			sprintf((pbuf+strlen(pbuf)), "SUSPEND REJECT: ");
			break;
		case 0x22:
			sprintf((pbuf+strlen(pbuf)), "RESUME REJECT: ");
			break;
		case 0x24:
			sprintf((pbuf+strlen(pbuf)), "HOLD: ");
			break;
		case 0x25:
			sprintf((pbuf+strlen(pbuf)), "SUSPEND: ");
			break;
		case 0x26:
			sprintf((pbuf+strlen(pbuf)), "RESUME: ");
			break;
		case 0x28:
			sprintf((pbuf+strlen(pbuf)), "HOLD ACKNOWLEDGE: ");
			break;
		case 0x2d:
			sprintf((pbuf+strlen(pbuf)), "SUSPEND ACKNOWLEDGE: ");
			break;
		case 0x2e:
			sprintf((pbuf+strlen(pbuf)), "RESUME ACKNOWLEDGE: ");
			break;
		case 0x30:
			sprintf((pbuf+strlen(pbuf)), "HOLD REJECT (Q.932): ");
			break;
		case 0x31:
			sprintf((pbuf+strlen(pbuf)), "RETRIEVE (Q.932): ");
			break;
		case 0x32:
			sprintf((pbuf+strlen(pbuf)), "RETRIEVE ACKNOWLEDGE (Q.932): ");
			break;
		case 0x37:
			sprintf((pbuf+strlen(pbuf)), "RETRIEVE REJECT (Q.932): ");
			break;

		/* call clearing */
			
		case 0x40:
			sprintf((pbuf+strlen(pbuf)), "DETACH: ");
			break;
		case 0x45:
			sprintf((pbuf+strlen(pbuf)), "DISCONNECT: ");
			break;
		case 0x46:
			sprintf((pbuf+strlen(pbuf)), "RESTART: ");
			break;
		case 0x48:
			sprintf((pbuf+strlen(pbuf)), "DETACH ACKNOWLEDGE: ");
			break;
		case 0x4d:
			sprintf((pbuf+strlen(pbuf)), "RELEASE: ");
			break;
		case 0x4e:
			sprintf((pbuf+strlen(pbuf)), "RESTART ACKNOWLEDGE: ");
			break;
		case 0x5a:
			sprintf((pbuf+strlen(pbuf)), "RELEASE COMPLETE: ");
			break;
			
		/* misc messages */

		case 0x60:
			sprintf((pbuf+strlen(pbuf)), "SEGMENT: ");
			break;
		case 0x62:
			sprintf((pbuf+strlen(pbuf)), "FACILITY (Q.932): ");
			break;
		case 0x64:
			sprintf((pbuf+strlen(pbuf)), "REGISTER (Q.932): ");
			break;
		case 0x68:
			sprintf((pbuf+strlen(pbuf)), "CANCEL ACKNOWLEDGE: ");
			break;
		case 0x6a:
			sprintf((pbuf+strlen(pbuf)), "FACILITY ACKNOWLEDGE: ");
			break;
		case 0x6c:
			sprintf((pbuf+strlen(pbuf)), "REGISTER ACKNOWLEDGE: ");
			break;
		case 0x6e:
			sprintf((pbuf+strlen(pbuf)), "NOTIFY: ");
			break;
		case 0x70:
			sprintf((pbuf+strlen(pbuf)), "CANCEL REJECT: ");
			break;
		case 0x72:
			sprintf((pbuf+strlen(pbuf)), "FACILITY REJECT: ");
			break;
		case 0x74:
			sprintf((pbuf+strlen(pbuf)), "REGISTER REJECT: ");
			break;
		case 0x75:
			sprintf((pbuf+strlen(pbuf)), "STATUS ENQIRY: ");
			break;
		case 0x79:
			sprintf((pbuf+strlen(pbuf)), "CONGESTION CONTROL: ");
			break;
		case 0x7b:
			sprintf((pbuf+strlen(pbuf)), "INFORMATION: ");
			break;
		case 0x7d:
			sprintf((pbuf+strlen(pbuf)), "STATUS: ");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "UNDEFINED, TYPE=0x%02x, ", buf[i]);
			break;
	}

	/* other information elements */

	i++;
	
	for (; i < n;)
	{
		sprintf((pbuf+strlen(pbuf)), "\n     ");
		
		if(buf[i] & 0x80)
		{
			/* single octett info element */

			switch(buf[i] & 0x70)
			{
				case 0x00:	/* reserved */
					sprintf((pbuf+strlen(pbuf)), "[reserved single octett info]");
					break;

				case 0x10:	/* shift */
					oldcodeset = codeset;
					codeset = buf[i] & 0x07;
					if(buf[i] & 0x08)
						codelock = 0;
					else
						codelock = 1;
					sprintf((pbuf+strlen(pbuf)), "[shift: codeset=%d lock=%d]", codeset, codelock);
					break;

				case 0x20:	/* more data */
					if(buf[i] & 0x01)
						sprintf((pbuf+strlen(pbuf)), "[sending complete]");
					else
						sprintf((pbuf+strlen(pbuf)), "[more data]");
					break;

				case 0x30:	/* congestion level */
					sprintf((pbuf+strlen(pbuf)), "[congestion level=");
					switch(buf[i] & 0x0f)
					{
						case 0x00:
							sprintf((pbuf+strlen(pbuf)), "rx-ready]");
							break;
						case 0x0f:
							sprintf((pbuf+strlen(pbuf)), "rx-not-ready]");
							break;
						default:
							sprintf((pbuf+strlen(pbuf)), "reserved (0x%02x)]", buf[i] & 0x0f);
							break;
					}
					break;
					
				case 0x50:	/* repeat ind */
					sprintf((pbuf+strlen(pbuf)), "[repeat indicator]");
					break;

				default:
					sprintf((pbuf+strlen(pbuf)), "[UNKNOWN SINGLE OCTET ELEMENT 0x%02x]", buf[i]);
					break;
			}

			i++;	/* next */

		}
		else
		{
			/* variable length info element */

			if(codeset == 0)
			{
				switch(buf[i])
				{
					case 0x00:
						sprintf((pbuf+strlen(pbuf)), "[segmented message: ");
						break;
					case 0x04:
						sprintf((pbuf+strlen(pbuf)), "[bearer capability: ");
						i += p_q931bc(pbuf, &buf[i]);
						goto next;
						break;
					case 0x08:
						sprintf((pbuf+strlen(pbuf)), "[cause: ");
						i += p_q931cause(pbuf, &buf[i]);
						goto next;
						break;
					case 0x0c:
						sprintf((pbuf+strlen(pbuf)), "[connected address (old): ");
						break;
					case 0x0d:
						sprintf((pbuf+strlen(pbuf)), "[extended facility (Q.932: )");
						break;
					case 0x10:
						sprintf((pbuf+strlen(pbuf)), "[call identity: ");
						break;
					case 0x14:
						sprintf((pbuf+strlen(pbuf)), "[call state: ");
						i++;
						len = buf[i];
						i++;
						sprintf((pbuf+strlen(pbuf)), "Std=");
						switch((buf[i] & 0x60) >> 5)
						{
							case 0:
								sprintf((pbuf+strlen(pbuf)), "CCITT");
								break;
							case 1:
								sprintf((pbuf+strlen(pbuf)), "ISO/IEC");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "National");
								break;
							case 3:
								sprintf((pbuf+strlen(pbuf)), "Special");
								break;
						}
						sprintf((pbuf+strlen(pbuf)), ", State=");

						switch((buf[i] & 0x3f))
						{
							case 0:
								sprintf((pbuf+strlen(pbuf)), "Null");
								break;
							case 1:
								sprintf((pbuf+strlen(pbuf)), "Call initiated");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "Overlap sending");
								break;
							case 3:
								sprintf((pbuf+strlen(pbuf)), "Outgoing call proceeding");
								break;
							case 4:
								sprintf((pbuf+strlen(pbuf)), "Call delivered");
								break;
							case 6:
								sprintf((pbuf+strlen(pbuf)), "Call present");
								break;
							case 7:
								sprintf((pbuf+strlen(pbuf)), "Call received");
								break;
							case 8:
								sprintf((pbuf+strlen(pbuf)), "Connect request");
								break;
							case 9:
								sprintf((pbuf+strlen(pbuf)), "Incoming call proceeding");
								break;
							case 10:
								sprintf((pbuf+strlen(pbuf)), "Active");
								break;
							case 11:
								sprintf((pbuf+strlen(pbuf)), "Disconnect request");
								break;
							case 12:
								sprintf((pbuf+strlen(pbuf)), "Disconnect indication");
								break;
							case 15:
								sprintf((pbuf+strlen(pbuf)), "Suspend request");
								break;
							case 17:
								sprintf((pbuf+strlen(pbuf)), "Resume request");
								break;
							case 19:
								sprintf((pbuf+strlen(pbuf)), "Release request");
								break;
							case 22:
								sprintf((pbuf+strlen(pbuf)), "Call abort");
								break;
							case 25:
								sprintf((pbuf+strlen(pbuf)), "Overlap receiving");
								break;
							case 0x3d:
								sprintf((pbuf+strlen(pbuf)), "Restart request");
								break;
							case 0x3e:
								sprintf((pbuf+strlen(pbuf)), "Restart");
								break;
							default:
								sprintf((pbuf+strlen(pbuf)), "ERROR: undefined/reserved");
								break;
						}
						sprintf((pbuf+strlen(pbuf)), "]");
						i++;
						goto next;
						break;
					case 0x18:
						sprintf((pbuf+strlen(pbuf)), "[channel id: channel=");
						i++;
						len = buf[i];
						i++;
						switch(buf[i] & 0x03)
						{
							case 0:
								sprintf((pbuf+strlen(pbuf)), "no channel");
								break;
							case 1:
								sprintf((pbuf+strlen(pbuf)), "B-1");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "B-2");
								break;
							case 3:
								sprintf((pbuf+strlen(pbuf)), "any channel");
								break;
						}
						if(buf[i] & 0x08)
							sprintf((pbuf+strlen(pbuf)), " (exclusive)]");
						else
							sprintf((pbuf+strlen(pbuf)), " (preferred)]");
						i++;
						goto next;
						break;
					case 0x19:
						sprintf((pbuf+strlen(pbuf)), "[data link connection id (Q.933): ");
						break;
					case 0x1c:
						i += q932_facility(pbuf, &buf[i]);
						goto next;
						break;
					case 0x1e:
						sprintf((pbuf+strlen(pbuf)), "[progress ind: ");
						i++;
						len = buf[i];
						i++;
						sprintf((pbuf+strlen(pbuf)), "Std=");
						switch((buf[i] & 0x60) >> 5)
						{
							case 0:
								sprintf((pbuf+strlen(pbuf)), "CCITT");
								break;
							case 1:
								sprintf((pbuf+strlen(pbuf)), "ISO/IEC");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "National");
								break;
							case 3:
								sprintf((pbuf+strlen(pbuf)), "Local");
								break;
						}
						sprintf((pbuf+strlen(pbuf)), ", Loc=");

						switch((buf[i] & 0x0f))
						{
							case 0:
								sprintf((pbuf+strlen(pbuf)), "User");
								break;
							case 1:
								sprintf((pbuf+strlen(pbuf)), "Private network serving local user");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "Public network serving local user");
								break;
							case 3:
								sprintf((pbuf+strlen(pbuf)), "Transit network");
								break;
							case 4:
								sprintf((pbuf+strlen(pbuf)), "Public network serving remote user");
								break;
							case 5:
								sprintf((pbuf+strlen(pbuf)), "Private network serving remote user");
								break;
							case 6:
								sprintf((pbuf+strlen(pbuf)), "Network beyond interworking point");
								break;
							default:
								sprintf((pbuf+strlen(pbuf)), "ERROR: undefined/reserved");
								break;
						}

						i++;

						sprintf((pbuf+strlen(pbuf)), "\n          Description: ");
						
						switch((buf[i] & 0x7f))
						{
							case 1:
								sprintf((pbuf+strlen(pbuf)), "Call is not end-to-end ISDN");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "Destination address is non-ISDN");
								break;
							case 3:
								sprintf((pbuf+strlen(pbuf)), "Origination address is non-ISDN");
								break;
							case 4:
								sprintf((pbuf+strlen(pbuf)), "Call has returned to the ISDN");
								break;
							case 5:
								sprintf((pbuf+strlen(pbuf)), "Interworking occured, Service change");
								break;
							case 8:
								sprintf((pbuf+strlen(pbuf)), "In-band info or appropriate pattern now available");
								break;
							default:
								sprintf((pbuf+strlen(pbuf)), "ERROR: undefined/reserved");
								break;
						}
						sprintf((pbuf+strlen(pbuf)), "]");
						i++;
						goto next;
						break;
					case 0x20:
						sprintf((pbuf+strlen(pbuf)), "[network specific facilities: ");
						break;
					case 0x24:
						sprintf((pbuf+strlen(pbuf)), "[terminal capabilities: ");
						break;
					case 0x27:
						sprintf((pbuf+strlen(pbuf)), "[notification indicator: ");
						i += p_q931notification(pbuf, &buf[i]);
						goto next;
						break;
					case 0x28:
						sprintf((pbuf+strlen(pbuf)), "[display: ");
						i++;
						len = buf[i];
						i++;
						for(j = 0; j < len; j++)
						{
							sprintf((pbuf+strlen(pbuf)),"%c", buf[j+i]);
						}
						sprintf((pbuf+strlen(pbuf)),"]");
						i += j;
						goto next;
						break;
					case 0x29:
						sprintf((pbuf+strlen(pbuf)), "[date/time: ");
						i++;
						len = buf[i];
						i++;
						j = 0;
						sprintf((pbuf+strlen(pbuf)),"%.2d.%.2d.%.2d",
							buf[i+2], buf[i+1], buf[i]);
						j+=3;
						if(j < len)
						{
							sprintf((pbuf+strlen(pbuf))," %.2d", buf[i+3]);
							j++;
						}
						if(j < len)
						{
							sprintf((pbuf+strlen(pbuf)),":%.2d", buf[i+4]);
							j++;
						}
						if(j < len)
						{
							sprintf((pbuf+strlen(pbuf)),":%.2d", buf[i+5]);
							j++;
						}
						sprintf((pbuf+strlen(pbuf)),"]");	
						i += len;
						goto next;
						break;
					case 0x2c:
						sprintf((pbuf+strlen(pbuf)), "[keypad: ");
						i++;
						len = buf[i];
						i++;
						for(j = 0; j < len; j++)
						{
							sprintf((pbuf+strlen(pbuf)),"%c", buf[j+i]);
						}
						sprintf((pbuf+strlen(pbuf)),"]");
						i += j;
						goto next;
						break;
					case 0x30:
						sprintf((pbuf+strlen(pbuf)), "[keypad echo: ");
						break;
					case 0x32:
						sprintf((pbuf+strlen(pbuf)), "[information req (Q.932): ");
						break;
					case 0x34:
						sprintf((pbuf+strlen(pbuf)), "[signal: ");
						break;
					case 0x36:
						sprintf((pbuf+strlen(pbuf)), "[switchhook: ");
						break;
					case 0x38:
						sprintf((pbuf+strlen(pbuf)), "[feature activation (Q.932): ");
						break;
					case 0x39:
						sprintf((pbuf+strlen(pbuf)), "[feature ind (Q.932): ");
						break;
					case 0x3a:
						sprintf((pbuf+strlen(pbuf)), "[service profile id (Q.932): ");
						break;
					case 0x3b:
						sprintf((pbuf+strlen(pbuf)), "[endpoint id (Q.932): ");
						break;
					case 0x40:
						sprintf((pbuf+strlen(pbuf)), "[information rate: ");
						break;
					case 0x41:
						sprintf((pbuf+strlen(pbuf)), "[precedence level (Q.955): ");
						break;
					case 0x42:
						sprintf((pbuf+strlen(pbuf)), "[end-to-end transit delay: ");
						break;
					case 0x43:
						sprintf((pbuf+strlen(pbuf)), "[transit delay detection and indication: ");
						break;
					case 0x44:
						sprintf((pbuf+strlen(pbuf)), "[packet layer binary parameters: ");
						break;
					case 0x45:
						sprintf((pbuf+strlen(pbuf)), "[packet layer window size: ");
						break;
					case 0x46:
						sprintf((pbuf+strlen(pbuf)), "[packet size: ");
						break;
					case 0x47:
						sprintf((pbuf+strlen(pbuf)), "[closed user group: ");
						break;
					case 0x48:
						sprintf((pbuf+strlen(pbuf)), "[link layer core parameters (Q.933): ");
						break;
					case 0x49:
						sprintf((pbuf+strlen(pbuf)), "[link layer protocol parameters (Q.933): ");
						break;
					case 0x4a:
						sprintf((pbuf+strlen(pbuf)), "[reverse charging information: ");
						break;
					case 0x4c:
						sprintf((pbuf+strlen(pbuf)), "[connected number (Q.951): ");
						i += p_q931address(pbuf, &buf[i]);
						goto next;
						break;

						break;
					case 0x4d:
						sprintf((pbuf+strlen(pbuf)), "[connected subaddress (Q.951): ");
						break;
					case 0x50:
						sprintf((pbuf+strlen(pbuf)), "[X.213 priority (Q.933): ");
						break;
					case 0x51:
						sprintf((pbuf+strlen(pbuf)), "[report type (Q.933): ");
						break;
					case 0x53:
						sprintf((pbuf+strlen(pbuf)), "[link integrity verification (Q.933): ");
						break;
					case 0x57:
						sprintf((pbuf+strlen(pbuf)), "[PVC status (Q.933): ");
						break;
					case 0x6c:
						sprintf((pbuf+strlen(pbuf)), "[calling party number: ");
						i += p_q931address(pbuf, &buf[i]);
						goto next;
						break;
					case 0x6d:
						sprintf((pbuf+strlen(pbuf)), "[calling party subaddress: ");
						break;
					case 0x70:
						sprintf((pbuf+strlen(pbuf)), "[called party number: ");
						i += p_q931address(pbuf, &buf[i]);
						goto next;
						break;
					case 0x71:
						sprintf((pbuf+strlen(pbuf)), "[called party subaddress: ");
						break;
					case 0x74:
						sprintf((pbuf+strlen(pbuf)), "[redirecting number: ");
						i += p_q931redir(pbuf, &buf[i]);
						goto next;
						break;
					case 0x76:
						sprintf((pbuf+strlen(pbuf)), "[redirection number: ");
						i += p_q931redir(pbuf, &buf[i]);
						goto next;
						break;
					case 0x78:
						sprintf((pbuf+strlen(pbuf)), "[transit network selection: ");
						break;
					case 0x79:
						sprintf((pbuf+strlen(pbuf)), "[restart indicator: ");
						break;
					case 0x7c:
						sprintf((pbuf+strlen(pbuf)), "[low layer compatibility: ");
						break;
					case 0x7d:
						sprintf((pbuf+strlen(pbuf)), "[high layer compatibility:");
						i += p_q931high_compat(pbuf, &buf[i]);
						goto next;
						break;
					case 0x7e:
						sprintf((pbuf+strlen(pbuf)), "[user-user: ");
						i += p_q931user_user(pbuf, &buf[i]);
						goto next;
						break;
					case 0x7f:
						sprintf((pbuf+strlen(pbuf)), "[escape for extension: ");
						break;
					default:
						sprintf((pbuf+strlen(pbuf)), "[UNKNOWN INFO-ELEMENT-ID=0x%02x: ", buf[i]);
						break;
				}
			}
			else
			{
				sprintf((pbuf+strlen(pbuf)), "[UNKNOWN CODESET=%d, IE=0x%02x: ", codeset, buf[i]);
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
	sprintf((pbuf+strlen(pbuf)),"\n");
}

/* EOF */

