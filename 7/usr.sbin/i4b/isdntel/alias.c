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
 *	isdntel - isdn4bsd telephone answering machine support
 *      ======================================================
 *
 *      $Id: alias.c,v 1.9 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:53:37 1999]
 *
 *----------------------------------------------------------------------------*/

#include "defs.h"
#include "alias.h"

static struct alias *firsta = NULL;

#define MAXBUFSZ	256

/*---------------------------------------------------------------------------*
 *	read in and init aliases
 *---------------------------------------------------------------------------*/
void 
init_alias(char *filename)
{
	FILE *fp;
	unsigned char buffer[MAXBUFSZ + 1];
	unsigned char number[MAXBUFSZ + 1];
	unsigned char name[MAXBUFSZ + 1];
	unsigned char *s, *d;
	struct alias *newa = NULL;
	struct alias *lasta = NULL;
	
	if((fp = fopen(filename, "r")) == NULL)
		return;

	while((fgets(buffer, MAXBUFSZ, fp)) != NULL)
	{
		if(buffer[0] == '#'  || buffer[0] == ' ' ||
		   buffer[0] == '\t' || buffer[0] == '\n')
		{
			continue;
		}

		s = buffer;
		d = number;

		while(*s && (isdigit(*s)))
			*d++ = *s++;

		*d = '\0';

		while(*s && (isspace(*s)))
			s++;

		d = name;

		while(*s && (isprint(*s)))
			*d++ = *s++;

		*d = '\0';
		
		if((strlen(number) > 1) && (strlen(name) > 1))
		{
			if((newa = (struct alias *) malloc(sizeof(struct alias))) == NULL)
				fatal("malloc failed for struct alias");
			if((newa->number = (char *) malloc(strlen(number)+1)) == NULL)
				fatal("malloc failed for number alias");
			if((newa->name = (char *) malloc(strlen(name)+1)) == NULL)
				fatal("malloc failed for name alias");
			strcpy(newa->name, name);
			strcpy(newa->number, number);
			newa->next = NULL;
			
			if(firsta == NULL)
			{
				firsta = newa;
			}
			else
			{
				lasta->next = newa;
			}
			lasta = newa;			
		}
	}
	fclose(fp);
}

/*---------------------------------------------------------------------------*
 *	read in and init aliases
 *---------------------------------------------------------------------------*/
char *
get_alias(char *number)
{
	struct alias *ca = NULL;

	if(firsta == NULL)
		return(NULL);

	ca = firsta;

	for(;;)
	{
		if(strlen(number) == strlen(ca->number))
		{
			if(!(strcmp(number, ca->number)))
				return(ca->name);
		}
		if(ca->next == NULL)
			break;
		ca = ca->next;
	}
	return(NULL);
}
			
/* EOF */
