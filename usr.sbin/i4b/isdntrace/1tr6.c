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
 *	1tr6.c - print 1TR6 protocol traces
 *	-----------------------------------
 *
 *	$Id: 1tr6.c,v 1.6 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:55:31 1999]
 *
 *---------------------------------------------------------------------------*/

#include "trace.h"

static int p_1tr6address(char *pbuf, unsigned char buf[]);
static int p_1tr6cause(char *pbuf, unsigned char buf[]);

/*---------------------------------------------------------------------------*
 *	decode the (german) national specific 1TR6 protocol
 *---------------------------------------------------------------------------*/
void
decode_1tr6(char *pbuf, int n, int off, unsigned char *buf, int raw)
{
	int codeset = 0;
	int oldcodeset = 0;	
	int codelock = 0;

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

	sprintf((pbuf+strlen(pbuf)), "1TR6: ");

	/* protocol discriminator */

	i = 0;

	pd = buf[i];

	switch(pd)
	{
		case 0x40:
			sprintf((pbuf+strlen(pbuf)), "pd=N0, ");
			break;
		case 0x41:
			sprintf((pbuf+strlen(pbuf)), "pd=N1, ");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "pd=UNDEF (0x%02x), ",pd);
			break;
	}

	/* call reference */

	i++;

	len = buf[i] & 0x0f;

	switch(len)
	{
		case 1:
			sprintf((pbuf+strlen(pbuf)), "cr=0x%02x %s, ", (buf[i+1] & 0x7f), (buf[i+1] & 0x80) ? "(from destination)" : "(from origination)");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "cr: LEN=%d %s 0x%02x 0x%02x, ", len, (buf[i+1] & 0x80) ? "org" : "dst", (buf[i+1] & 0x7f), (buf[i+2] & 0x7f));
			break;
	}

	i += (len+1);

	/* message type */

	sprintf((pbuf+strlen(pbuf)), "message=");

	if(pd == 0x40)	/* protocol discriminator N0 */
	{
		switch(buf[i])
		{
			case 0x61:
				sprintf((pbuf+strlen(pbuf)), "REGISTER INDICATION: ");
				break;
			case 0x62:
				sprintf((pbuf+strlen(pbuf)), "CANCEL INDICATION: ");
				break;
			case 0x63:
				sprintf((pbuf+strlen(pbuf)), "FACILITY STATUS: ");
				break;
			case 0x64:
				sprintf((pbuf+strlen(pbuf)), "STATUS ACKNOWLEDGE: ");
				break;
			case 0x65:
				sprintf((pbuf+strlen(pbuf)), "STATUS REJECT: ");
				break;
			case 0x66:
				sprintf((pbuf+strlen(pbuf)), "FACILITY INFORMATION: ");
				break;
			case 0x67:
				sprintf((pbuf+strlen(pbuf)), "INFORMATION ACKNOWLEDGE: ");
				break;
			case 0x68:
				sprintf((pbuf+strlen(pbuf)), "INFORMATION REJECT: ");
				break;
			case 0x75:
				sprintf((pbuf+strlen(pbuf)), "CLOSE: ");
				break;
			case 0x77:
				sprintf((pbuf+strlen(pbuf)), "CLOSE ACKNOWLEDGE: ");
				break;
			default:
				sprintf((pbuf+strlen(pbuf)), "ERROR: PD=0x40 MSG=0x%02x, ", buf[i]);
				break;
		}
	}
	else if(pd == 0x41)
	{
		switch(buf[i])
		{
			case 0x00:
				sprintf((pbuf+strlen(pbuf)), "ESCAPE: ");
				break;
			case 0x01:
				sprintf((pbuf+strlen(pbuf)), "ALERT: ");
				break;
			case 0x02:
				sprintf((pbuf+strlen(pbuf)), "CALL SENT: ");
				break;
			case 0x07:
				sprintf((pbuf+strlen(pbuf)), "CONNECT: ");
				break;
			case 0x0f:
				sprintf((pbuf+strlen(pbuf)), "CONNECT ACKNOWLEDGE: ");
				break;
			case 0x05:
				sprintf((pbuf+strlen(pbuf)), "SETUP: ");
				break;
			case 0x0d:
				sprintf((pbuf+strlen(pbuf)), "SETUP ACKNOWLEDGE: ");
				break;

			case 0x26:
				sprintf((pbuf+strlen(pbuf)), "RESUME: ");
				break;
			case 0x2e:
				sprintf((pbuf+strlen(pbuf)), "RESUME ACKNOWLEDGE: ");
				break;
			case 0x22:
				sprintf((pbuf+strlen(pbuf)), "RESUME REJECT: ");
				break;
			case 0x25:
				sprintf((pbuf+strlen(pbuf)), "SUSPEND: ");
				break;
			case 0x2d:
				sprintf((pbuf+strlen(pbuf)), "SUSPEND ACKNOWLEDGE: ");
				break;
			case 0x21:
				sprintf((pbuf+strlen(pbuf)), "SUSPEND REJECT: ");
				break;
			case 0x20:
				sprintf((pbuf+strlen(pbuf)), "USER INFORMATION: ");
				break;
				
			case 0x40:
				sprintf((pbuf+strlen(pbuf)), "DETACH");
				break;
			case 0x45:
				sprintf((pbuf+strlen(pbuf)), "DISCONNECT: ");
				break;
			case 0x4d:
				sprintf((pbuf+strlen(pbuf)), "RELEASE: ");
				break;
			case 0x5a:
				sprintf((pbuf+strlen(pbuf)), "RELEASE ACKNOWLEDGE");
				break;
				
			case 0x6e:
				sprintf((pbuf+strlen(pbuf)), "CANCEL ACKNOWLEDGE: ");
				break;
			case 0x67:
				sprintf((pbuf+strlen(pbuf)), "CANCEL REJECT: ");
				break;
			case 0x69:
				sprintf((pbuf+strlen(pbuf)), "CONGESTION CONTROL: ");
				break;
			case 0x60:
				sprintf((pbuf+strlen(pbuf)), "FACILITY: ");
				break;
			case 0x68:
				sprintf((pbuf+strlen(pbuf)), "FACILITY ACKNOWLEDGE: ");
				break;
			case 0x66:
				sprintf((pbuf+strlen(pbuf)), "FACILITY CANCEL: ");
				break;
			case 0x64:
				sprintf((pbuf+strlen(pbuf)), "FACILITY REGISTER: ");
				break;
			case 0x65:
				sprintf((pbuf+strlen(pbuf)), "FACILITY REJECT: ");
				break;
			case 0x6d:
				sprintf((pbuf+strlen(pbuf)), "INFORMATION: ");
				break;
			case 0x6c:
				sprintf((pbuf+strlen(pbuf)), "REGISTER ACKNOWLEDGE: ");
				break;
			case 0x6f:
				sprintf((pbuf+strlen(pbuf)), "REGISTER REJECT: ");
				break;
			case 0x63:
				sprintf((pbuf+strlen(pbuf)), "STATUS: ");
				break;

			default:
				sprintf((pbuf+strlen(pbuf)), "ERROR: PD=0x41 MSG=0x%02x, ", buf[i]);
				break;
		}
	}
	else
	{
		sprintf((pbuf+strlen(pbuf)), "ERROR: PD=0x%02x MSG=0x%02x, ", pd, buf[i]);
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
					sprintf((pbuf+strlen(pbuf)), "[more data]");
					break;

				case 0x30:	/* congestion level */
					sprintf((pbuf+strlen(pbuf)), "[congestion level = %d]", buf[i] & 0x0f);
					break;

				default:
					sprintf((pbuf+strlen(pbuf)), "[UNDEF SINGLE OCTET ELEMENT 0x%02x]", buf[i]);
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
					case 0x08:
						sprintf((pbuf+strlen(pbuf)), "[cause: ");
						i += p_1tr6cause(pbuf, &buf[i]);
						goto next;
						break;
						
					case 0x0c:
						sprintf((pbuf+strlen(pbuf)), "[connected address: ");
						i += p_1tr6address(pbuf, &buf[i]);
						goto next;
						break;

					case 0x10:
						sprintf((pbuf+strlen(pbuf)), "[call identity: ");
						break;
					case 0x18:
						sprintf((pbuf+strlen(pbuf)), "[channel id: channel=");
						i += 2;
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
					case 0x20:
						sprintf((pbuf+strlen(pbuf)), "[network specific facilities: ");
						i++;
						len = buf[i];
						i+=2;
						switch(buf[i])
						{
							case 1:
								sprintf((pbuf+strlen(pbuf)), "Sperre");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "AWS 1");
								break;
							case 3:
								sprintf((pbuf+strlen(pbuf)), "AWS 2");
								break;
							case 0xe:
								sprintf((pbuf+strlen(pbuf)), "Konferenz");
								break;
							case 0xf:
								sprintf((pbuf+strlen(pbuf)), "B-Kan uebern.");
								break;
							case 0x10:
								sprintf((pbuf+strlen(pbuf)), "aktvrg. ghlt. Vbdg.");
								break;
							case 0x11:
								sprintf((pbuf+strlen(pbuf)), "3er Konf");
								break;
							case 0x12:
								sprintf((pbuf+strlen(pbuf)), "1seitg D/G Wechsel");
								break;
							case 0x13:
								sprintf((pbuf+strlen(pbuf)), "2seitig D/G Wechsel");
								break;
							case 0x14:
								sprintf((pbuf+strlen(pbuf)), "Rufnr. identifiz.");
								break;
							case 0x15:
								sprintf((pbuf+strlen(pbuf)), "GBG");
								break;
							case 0x17:
								sprintf((pbuf+strlen(pbuf)), "ueberg. Ruf");
								break;
							case 0x1a:
								sprintf((pbuf+strlen(pbuf)), "um/weitergel. Ruf");
								break;
							case 0x1b:
								sprintf((pbuf+strlen(pbuf)), "unterdr. A-Rufnr.");
								break;
							case 0x1e:
								sprintf((pbuf+strlen(pbuf)), "Verbdg. deaktivieren");
								break;
							case 0x1d:
								sprintf((pbuf+strlen(pbuf)), "Verbdg. aktivieren");
								break;
							case 0x1f:
								sprintf((pbuf+strlen(pbuf)), "SPV");
								break;
							case 0x23:
								sprintf((pbuf+strlen(pbuf)), "Rueckw. 2seitg. DW");
								break;
							case 0x24:
								sprintf((pbuf+strlen(pbuf)), "Anrufumltg. priv. Netz");
								break;
							default:
								sprintf((pbuf+strlen(pbuf)), "undefined");
								break;
						}
						i++;
						sprintf((pbuf+strlen(pbuf)), ", serv=%d", buf[i]);
						i++;
						sprintf((pbuf+strlen(pbuf)), ", ainfo=%d", buf[i]);
						i++;
						len-=4;
						for(j = 0; j < len; j++)
						{
							sprintf((pbuf+strlen(pbuf))," 0x%02x", buf[j+i]);
						}
						sprintf((pbuf+strlen(pbuf)),"]");
						i += j;
						goto next;
						break;
					case 0x28:
						sprintf((pbuf+strlen(pbuf)), "[display: ");
						break;
					case 0x2c:
						sprintf((pbuf+strlen(pbuf)), "[keypad: ");
						break;
					case 0x6c:
						sprintf((pbuf+strlen(pbuf)), "[origination address: ");
						i += p_1tr6address(pbuf, &buf[i]);
						goto next;
						break;
					case 0x70:
						sprintf((pbuf+strlen(pbuf)), "[destination address: ");
						i += p_1tr6address(pbuf, &buf[i]);
						goto next;
						break;
					case 0x7e:
						sprintf((pbuf+strlen(pbuf)), "[user-user information: ");
						break;
					case 0x7f:
						sprintf((pbuf+strlen(pbuf)), "[reserved: ");
						break;
					default:
						sprintf((pbuf+strlen(pbuf)), "[UNKNOWN INFO-ELEMENT-ID");
						break;
				}
			}
			else if(codeset == 6)
			{
				switch(buf[i])
				{
					case 0x01:
						sprintf((pbuf+strlen(pbuf)), "[service ind: serv=");
						i+= 2;
						switch(buf[i])
						{
							case 0x01:
								sprintf((pbuf+strlen(pbuf)), "phone");
								break;
							case 0x02:
								sprintf((pbuf+strlen(pbuf)), "a/b");
								break;
							case 0x03:
								sprintf((pbuf+strlen(pbuf)), "X.21");
								break;
							case 0x04:
								sprintf((pbuf+strlen(pbuf)), "fax g4");
								break;
							case 0x05:
								sprintf((pbuf+strlen(pbuf)), "btx");
								break;
							case 0x07:
								sprintf((pbuf+strlen(pbuf)), "64k data");
								break;
							case 0x08:
								sprintf((pbuf+strlen(pbuf)), "X.25");
								break;
							case 0x09:
								sprintf((pbuf+strlen(pbuf)), "teletex");
								break;
							case 0x0a:
								sprintf((pbuf+strlen(pbuf)), "mixed");
								break;
							case 0x0d:
								sprintf((pbuf+strlen(pbuf)), "temex");
								break;
							case 0x0e:
								sprintf((pbuf+strlen(pbuf)), "picturephone");
								break;
							case 0x0f:
								sprintf((pbuf+strlen(pbuf)), "btx (new)");
								break;
							case 0x10:
								sprintf((pbuf+strlen(pbuf)), "videophone");
								break;
							default:
								sprintf((pbuf+strlen(pbuf)), "undefined");
								break;
						}
						i++;
						sprintf((pbuf+strlen(pbuf)), ", ainfo=0x%02x]", buf[i]);
						i++;
						goto next;
						break;
					case 0x02:
						sprintf((pbuf+strlen(pbuf)), "[charging information: ");
						break;
					case 0x03:
						sprintf((pbuf+strlen(pbuf)), "[date: ");
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
					case 0x05:
						sprintf((pbuf+strlen(pbuf)), "[facility select: ");
						break;
					case 0x06:
						sprintf((pbuf+strlen(pbuf)), "[status of facilities: ");
						break;
					case 0x07:
						sprintf((pbuf+strlen(pbuf)), "[status of called party: ");
						i+=2;
						switch(buf[i])
						{
							case 1:
								sprintf((pbuf+strlen(pbuf)), "no information]");
								break;
							case 2:
								sprintf((pbuf+strlen(pbuf)), "is being called]");
								break;
							default:
								sprintf((pbuf+strlen(pbuf)), "undefined (0x%02x)]", buf[i]);
								break;
						}
						i++;
						goto next;
						break;
					case 0x08:
						sprintf((pbuf+strlen(pbuf)), "[additional tx attributes: ");
						i++;
						len = buf[i];
						i++;
						for(j = 0; j < len; j++)
						{
							switch(buf[j+i] &0x70)
							{
								case 0:
									sprintf((pbuf+strlen(pbuf)), "no satellite link");
									break;
								case 1:
									sprintf((pbuf+strlen(pbuf)), "one satellite link");
									break;
								case 2:
									sprintf((pbuf+strlen(pbuf)), "two satellite links");
									break;
								case 3:
									sprintf((pbuf+strlen(pbuf)), "three satellite links");
									break;
								default:
									sprintf((pbuf+strlen(pbuf)), "undefined value");
									break;
							}
							if(buf[j+i] & 0x80)
								sprintf((pbuf+strlen(pbuf)),"(flag=req)]");
							else
								sprintf((pbuf+strlen(pbuf)),"(flag=ind)]");
						}
						i += j;
						goto next;
						break;
					default:
						sprintf((pbuf+strlen(pbuf)), "[UNKNOWN INFO-ELEMENT-ID");
						break;
				}
			}
			else
			{
				sprintf((pbuf+strlen(pbuf)), "[ILLEGAL CODESET = 0x%02x", codeset);
			}

			i++;	/* index -> length */

			len = buf[i];

			i++;	/* index -> 1st param */

			for(j = 0; j < len; j++)
			{
				sprintf((pbuf+strlen(pbuf))," 0x%02x", buf[j+i]);
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

/*---------------------------------------------------------------------------*
 *	decode and print the cause
 *---------------------------------------------------------------------------*/
static int
p_1tr6cause(char *pbuf, unsigned char buf[])
{
	int j;
	int len;
	int i = 0;
	
	i++;	/* index -> length */

	len = buf[i];

	switch(len)
	{
		case 0:
			sprintf((pbuf+strlen(pbuf)), "%s", print_cause_1tr6(0));
			break;
		case 1:
			i++;
			sprintf((pbuf+strlen(pbuf)), "%s", print_cause_1tr6(buf[i] & 0x7f));
			break;
		case 2:
			i++;
			sprintf((pbuf+strlen(pbuf)), "%s, location: ", print_cause_1tr6(buf[i] & 0x7f));
			i++;
			switch(buf[i] & 0x0f)
			{
				case 0x04:
					sprintf((pbuf+strlen(pbuf)), "public network");
					break;
				case 0x05:
					sprintf((pbuf+strlen(pbuf)), "private network");
					break;
				case 0x0f:
					sprintf((pbuf+strlen(pbuf)), "no information");
					break;
				default:
					sprintf((pbuf+strlen(pbuf)), "reserved (0x%02x)", buf[i] & 0x0f);
					break;
			}
			break;
		default:
			i++;	/* index -> length */
			len = buf[i];
			i++;	/* index -> 1st param */
			for(j = 0; j < len; j++)
			{
				sprintf((pbuf+strlen(pbuf))," 0x%02x", buf[j+i]);
			}
			break;
	}
	i++;
	sprintf((pbuf+strlen(pbuf)),"]");
	return(i);
}

/*---------------------------------------------------------------------------*
 *	decode and print the ISDN (telephone) number
 *---------------------------------------------------------------------------*/
static int
p_1tr6address(char *pbuf, unsigned char buf[])
{
	int j;
	int len;
	int i = 0;
	int tp;
	
	i++;	/* index -> length */
	len = buf[i];
	i++;	/* index -> 1st param */
	tp = buf[i];
	
	i++;
	len--;
	
	for(j = 0; j < len; j++)
	{
		sprintf((pbuf+strlen(pbuf)),"%c", buf[j+i]);
	}

	switch((tp & 0x70) >> 4)
	{
		case 0:
			sprintf((pbuf+strlen(pbuf)), " (type=unknown, ");
			break;
		case 1:
			sprintf((pbuf+strlen(pbuf)), " (type=international, ");
			break;
		case 2:
			sprintf((pbuf+strlen(pbuf)), " (type=national, ");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), " (type=%d, ", ((tp & 0x70) >> 4));
			break;
	}

	switch(tp & 0x0f)
	{
		case 0:
			sprintf((pbuf+strlen(pbuf)), "plan=unknown)");
			break;
		case 1:
			sprintf((pbuf+strlen(pbuf)), "plan=ISDN)");
			break;
		default:
			sprintf((pbuf+strlen(pbuf)), "plan=%d)", (tp & 0x0f));
			break;
	}
	
	sprintf((pbuf+strlen(pbuf)),"]");

	i += j;

	return(i);
}

/* EOF */
