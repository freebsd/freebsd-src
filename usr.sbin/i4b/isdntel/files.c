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
 *      $Id: files.c,v 1.8 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdntel/files.c,v 1.6 1999/12/14 21:07:44 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:54:06 1999]
 *
 *----------------------------------------------------------------------------*/

#include "defs.h"

/*---------------------------------------------------------------------------*
 *	create a doubly linked list in sorted order, return pointer to new
 *	first element of list
 *---------------------------------------------------------------------------*/
struct onefile *store
  (register struct onefile *new,		/* new entry to store into list */
   register struct onefile *top)		/* current first entry in list */
{
	register struct onefile *old, *p;

	if (last == NULL)			/* enter very first element ? */
	{
		new->next = NULL;
		new->prev = NULL;
		last = new;			/* init last */
		return (new);			/* return new first */
	}
	p = top;				/* p = old first element */
	old = NULL;
	while (p)
	{
		if ((strcmp(p->fname, new->fname)) < 0)	/* current less new ? */
		{
			old = p;
			p = p->next;
		}
		else
		{				/* current >= new */

			if (p->prev)
			{
				p->prev->next = new;
				new->next = p;
				new->prev = p->prev;
				p->prev = new;
				return (top);
			}
			new->next = p;
			new->prev = NULL;
			p->prev = new;
			return (new);
		}
	}
	old->next = new;
	new->next = NULL;
	new->prev = old;
	last = new;
	return (first);
}

/*---------------------------------------------------------------------------*
 *	read current directory and build up a doubly linked sorted list
 *---------------------------------------------------------------------------*/
int
fill_list(void)
{
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	register struct dirent *dp;
#else
	register struct direct *dp;
#endif
	register struct onefile *new_entry;
	register DIR *dirp;
	int flcnt = 0;
	char tmp[80];
	char *s, *d;
	
	if ((dirp = opendir(spooldir)) == NULL)
		fatal("cannot open spooldirectory %s!\n", spooldir);

	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp))
	{
		if(!isdigit(*(dp->d_name)))
			continue;

		if ((new_entry = (struct onefile *) malloc(sizeof(struct onefile))) == NULL)
		{
			fatal("files.c, fill_list(): structure onefile malloc failed");
		}

		/* alloc filename memory and copy name into it */

		if ((new_entry->fname = (char *) malloc(strlen(dp->d_name) + 1)) == NULL)
		{
			fatal("files.c, fill_list(): malloc filename string memory failed");
		}

		strcpy(new_entry->fname, dp->d_name);

		/* fill in remaining fields from filename */

		tmp[0] = dp->d_name[4]; /* day msb */
		tmp[1] = dp->d_name[5]; /* day lsb */
		tmp[2] = '.';
		tmp[3] = dp->d_name[2]; /* month msb */
		tmp[4] = dp->d_name[3]; /* month lsb */
		tmp[5] = '.';
		tmp[6] = dp->d_name[0]; /* year msb */
		tmp[7] = dp->d_name[1]; /* year lsb */
		tmp[8] = '\0';

		if((new_entry->date = (char *) malloc(strlen(tmp) + 1)) == NULL)
		{
			fatal("files.c, fill_list(): malloc date string memory failed");
		}

		strcpy(new_entry->date, tmp);
		
		tmp[0]  = dp->d_name[6]; /* hour msb */
		tmp[1] = dp->d_name[7]; /* hour lsb */
		tmp[2] = ':';
		tmp[3] = dp->d_name[8]; /* minute msb */
		tmp[4] = dp->d_name[9]; /* minute lsb */
		tmp[5] = ':';
		tmp[6] = dp->d_name[10]; /* second msb */
		tmp[7] = dp->d_name[11]; /* second lsb */
		tmp[8] = '\0';
		
		if((new_entry->time = (char *) malloc(strlen(tmp) + 1)) == NULL)
		{
			fatal("files.c, fill_list(): malloc time string memory failed");
		}

		strcpy(new_entry->time, tmp);

		/* destination number */
		
		s = &dp->d_name[13];
		d = &tmp[0];

		while(*s && (*s != '-'))
			*d++ = *s++;

		*d = '\0';
		
		if((new_entry->dstnumber = (char *) malloc(strlen(tmp) + 1)) == NULL)
		{
			fatal("files.c, fill_list(): malloc dstnumber string memory failed");
		}

		strcpy(new_entry->dstnumber, tmp);

		/* source number */
		
		s++;
		d = &tmp[0];

		while(*s && (*s != '-'))
			*d++ = *s++;

		*d = '\0';
		
		if((new_entry->srcnumber = (char *) malloc(strlen(tmp) + 1)) == NULL)
		{
			fatal("files.c, fill_list(): malloc srcnumber string memory failed");
		}

		strcpy(new_entry->srcnumber, tmp);

		/* length in seconds */
		
		s++;
		d = &tmp[0];

		while(*s && (*s != '-'))
			*d++ = *s++;

		*d = '\0';
		
		if((new_entry->seconds = (char *) malloc(strlen(tmp) + 1)) == NULL)
		{
			fatal("files.c, fill_list(): malloc seconds string memory failed");
		}

		strcpy(new_entry->seconds, tmp);

		/* search for alias and add if found */
		
		new_entry->alias = get_alias(new_entry->srcnumber);
		
		/* sort entry into linked list */

		first = store(new_entry, first);

		flcnt++;			/* increment file count */
	}
	closedir(dirp);				/* close current dir */
	return(flcnt);				/* ok return */
}

/*---------------------------------------------------------------------------*
 *	free the current malloc'ed list
 *---------------------------------------------------------------------------*/
void
free_list(void)
{
	register struct onefile *dir;
	register struct onefile *tmp;

	dir = first;				/* start of linked list */

	while (dir)				/* free all */
	{
		tmp = dir->next;		/* save ptr to next entry */
		free(dir->fname);		/* free filename space */
		free(dir->date);
		free(dir->time);
		free(dir->srcnumber);
		free(dir->dstnumber);
		free(dir->seconds);		
		free(dir);			/* free struct space */
		dir = tmp;			/* ptr = ptr to next entry */
	}
	first = NULL;				/* first ptr = NULL */
	last = NULL;				/* last ptr = NULL */
}

/*---------------------------------------------------------------------------*
 *	delete a file
 *---------------------------------------------------------------------------*/
void
delete(struct onefile *this)
{
	char buffer[MAXPATHLEN+1];

	if(this == NULL)
		return;
		
	sprintf(buffer, "%s", this->fname);
	
	unlink(buffer);

	free_list();

	wclear(main_w);

	init_files(cur_pos);
}

/*---------------------------------------------------------------------------*
 *	reread the spool directory
 *---------------------------------------------------------------------------*/
void
reread(void)
{
	free_list();

	wclear(main_w);

	init_files(cur_pos);
}

/*---------------------------------------------------------------------------*
 *	play a file
 *---------------------------------------------------------------------------*/
void
play(struct onefile *this)
{
	char buffer[MAXPATHLEN+1];

	if(this == NULL)
		return;
		
	sprintf(buffer, playstring, this->fname);
	
	system(buffer);
}

/*---------------------------------- EOF -------------------------------------*/
