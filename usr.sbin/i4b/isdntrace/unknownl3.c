/*
 * Copyright (c) 2000 Hellmuth Michaelis. All rights reserved.
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
 *	unknownl3.c - print L3 packets with unknown PD
 *	----------------------------------------------
 *
 *	$Id: unknownl3.c,v 1.2 2000/02/13 15:26:52 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun Feb 13 14:16:44 2000]
 *
 *---------------------------------------------------------------------------*/

#include "trace.h"

/*---------------------------------------------------------------------------*
 *	decode unknown protocol
 *---------------------------------------------------------------------------*/
void
decode_unknownl3(char *pbuf, int n, int off, unsigned char *buf, int raw)
{
	int pd;
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
		
	/* protocol discriminator */

	pd = buf[i];

	sprintf((pbuf+strlen(pbuf)), "PD%02X: ", pd);	

	if(pd >= 0x00 && pd <= 0x07)
		sprintf((pbuf+strlen(pbuf)), "pd=User-User (0x%02x)",pd);
	else if(pd == 0x08)
		sprintf((pbuf+strlen(pbuf)), "pd=Q.931/I.451");
	else if(pd >= 0x10 && pd <= 0x3f)
		sprintf((pbuf+strlen(pbuf)), "pd=Other Layer 3 or X.25 (0x%02x)",pd);
	else if(pd >= 0x40 && pd <= 0x4f)
		sprintf((pbuf+strlen(pbuf)), "pd=National Use (0x%02x)",pd);
	else if(pd >= 0x50 && pd <= 0xfe)
		sprintf((pbuf+strlen(pbuf)), "pd=Other Layer 3 or X.25 (0x%02x)",pd);
	else
		sprintf((pbuf+strlen(pbuf)), "pd=Reserved (0x%02x)",pd);

	sprintf((pbuf+strlen(pbuf)), "\n     [");	
	for(j = 0; j < (n-i); j++)
	{
		sprintf((pbuf+strlen(pbuf)),"0x%02x ", buf[j+i]);
	}
	
	sprintf((pbuf+strlen(pbuf)),"]\n");
}

/* EOF */

