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
 *	isdnd - common aliasfile handling
 *      =================================
 *
 *	NOTE:	this has to stay in sync with isdntel/alias.c to be able
 *		to share a common aliasfile!
 *		
 *      $Id: alias.c,v 1.8 1999/12/13 21:25:24 hm Exp $
 *
 * $FreeBSD: src/usr.sbin/i4b/isdnd/alias.c,v 1.6 1999/12/14 21:07:25 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:45:19 1999]
 *
 *----------------------------------------------------------------------------*/

#include "isdnd.h"

static struct alias *firsta = NULL;

#define MAXBUFSZ	256

static void free_alias(struct alias *ptr);

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

	firsta = NULL;
	
	if((fp = fopen(filename, "r")) == NULL)
	{
		log(LL_ERR, "init_alias: error opening aliasfile %s: %s!", filename, strerror(errno));
		exit(1);
	}

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
			{
				log(LL_ERR, "init_alias: malloc failed for struct alias!\n");
				exit(1);
			}

			if((newa->number = (char *) malloc(strlen(number)+1)) == NULL)
			{
				log(LL_ERR, "init_alias: malloc failed for number alias!\n");
				exit(1);
			}

			if((newa->name = (char *) malloc(strlen(name)+1)) == NULL)
			{
				log(LL_ERR, "init_alias: malloc failed for name alias!\n");
				exit(1);
			}

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
 *	free all aliases
 *---------------------------------------------------------------------------*/
void
free_aliases(void)
{
	free_alias(firsta);
}

/*---------------------------------------------------------------------------*
 *	free aliases
 *---------------------------------------------------------------------------*/
static void
free_alias(struct alias *ptr)
{

	if(ptr == NULL)
		return;

	if(ptr->next != NULL)
		free_alias(ptr->next);

	if(ptr->number != NULL)
		free(ptr->number);
		
	if(ptr->name != NULL)
		free(ptr->name);

	free(ptr);
}
	
/*---------------------------------------------------------------------------*
 *	try to find alias for number. if no alias found, return number.
 *---------------------------------------------------------------------------*/
char *
get_alias(char *number)
{
	struct alias *ca = NULL;

	if(firsta == NULL)
		return(number);

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
	return(number);
}
			
/* EOF */
