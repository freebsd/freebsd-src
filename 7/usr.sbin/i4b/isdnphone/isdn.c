/*
 * Copyright (c) 1999 Hellmuth Michaelis. All rights reserved.
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
 *	isdnphone - isdn (i4b) handling
 *	===============================
 *
 *	$Id: isdn.c,v 1.4 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:53:05 1999]
 *
 *---------------------------------------------------------------------------*/

#include "defs.h"

/*---------------------------------------------------------------------------*
 *	dialer init
 *---------------------------------------------------------------------------*/
int
init_dial(char *device)
{
	int ret;
	
	if((ret = open(device, O_RDWR)) < 0)
	{
		fprintf(stderr, "unable to open %s: %s\n", device, strerror(errno));
		return(-1);
	}
	return(ret);
}

/*---------------------------------------------------------------------------*
 *	i4bteld data available handler
 *---------------------------------------------------------------------------*/
void
dial_hdlr(void)
{
	char result;

	if((read (dialerfd, &result, 1) < 0))
	{
		fatal("read failed: %s", strerror(errno));
	}

	switch(result)
	{
		case RSP_CONN:
			newstate(ST_ACTIVE);
			message("connected to remote!");
			break;
			
		case RSP_BUSY:
			message("remote is busy!");
			break;

		case RSP_HUP:
			newstate(ST_IDLE);		
			message("disconnected from remote!");
			break;

		case RSP_NOA:
			message("no answer from remote!");
			break;

		default:
			message("unknown response = 0x%2x!", result);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	telephone init
 *---------------------------------------------------------------------------*/
int
init_tel(char *device)
{
	int ret;
	int format;

	if(play_fmt == AFMT_MU_LAW)
		format = CVT_ALAW2ULAW;
	else
		format = CVT_NONE;		
	
	if((ret = open(device, O_RDWR)) < 0)
		fatal("unable to open %s: %s\n", device, strerror(errno));

	if((ioctl(ret, I4B_TEL_SETAUDIOFMT, &format)) < 0)
		fatal("ioctl I4B_TEL_SETAUDIOFMT failed: %s", strerror(errno));

	return(ret);
}
		
/*---------------------------------------------------------------------------*
 *	dial number
 *---------------------------------------------------------------------------*/
void
do_dial(char *number)
{
	char commandbuffer[80];	
	sprintf(commandbuffer, "D%s", number);

	if((write(dialerfd, commandbuffer, strlen(commandbuffer))) < 0)
	{
		fatal("write commandbuffer failed: %s", strerror(errno));
	}
}

/*---------------------------------------------------------------------------*
 *	hangup
 *---------------------------------------------------------------------------*/
void
do_hangup(void)
{
	char commandbuffer[80];	

	if(state == ST_IDLE)
	{
		message("tried hangup while ST_IDLE");
		return;
	}
	
	sprintf(commandbuffer, "H");

	if((write(dialerfd, commandbuffer, strlen(commandbuffer))) < 0)
	{
		fatal("write commandbuffer failed: %s", strerror(errno));
	}
}

/*---------------------------------------------------------------------------*
 *	i4btel speech data available handler
 *---------------------------------------------------------------------------*/
void
tel_hdlr(void)
{
	unsigned char buffer[BCH_MAX_DATALEN];
	int ret;

	ret = read(telfd, buffer, BCH_MAX_DATALEN);

	if(ret < 0)
	{
		fatal("read telfd failed: %s", strerror(errno));
	}

	debug("tel_hdlr: read %d bytes\n", ret);

	if(ret > 0)
	{
		audiowrite(ret, buffer);
	}
}

/*---------------------------------------------------------------------------*
 *	write audio data to ISDN
 *---------------------------------------------------------------------------*/
void
telwrite(int len, unsigned char *buf)
{
	if((write(telfd, buf, len)) < 0)
	{
		fatal("write tel failed: %s", strerror(errno));
	}
}

/* EOF */
