/*
 * Copyright (c) 1992, 2000 Hellmuth Michaelis
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*---------------------------------------------------------------------------
 *
 *	misc.c		font editor misc routines
 *	-----------------------------------------
 *
 * 	last edit-date: [Mon Mar 27 16:38:12 2000]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#include "fed.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

static unsigned char *fonttab;		/* ptr to font in core memory */

static char *bitmask[] = {
		"....",			/*  0 */
		"...*",			/*  1 */
		"..*.",			/*  2 */
		"..**",			/*  3 */
		".*..",			/*  4 */
		".*.*",			/*  5 */
		".**.",			/*  6 */
		".***",			/*  7 */
		"*...",			/*  8 */
		"*..*",			/*  9 */
		"*.*.",			/*  A */
		"*.**",			/*  B */
		"**..",			/*  C */
		"**.*",			/*  D */
		"***.",			/*  E */
		"****",			/*  F */
		NULL };

static char lfilename[1024];	/* current filename */
static unsigned int lfilesize;	/* current filename's size */

/*---------------------------------------------------------------------------*
 *	read fontfile into memory
 *---------------------------------------------------------------------------*/
void readfont(char *filename)
{
	FILE *in;
	struct stat sbuf, *sbp;
	int ret;
	char buffer[1024];

	sbp = &sbuf;

	if((in = fopen(filename, "r")) == NULL)
	{
		sprintf(buffer, "cannot open file %s for reading", filename);
		perror(buffer);
		exit(1);
	}

	if((fstat(fileno(in), sbp)) != 0)
	{
		sprintf(buffer, "cannot fstat file %s", filename);
		perror(buffer);
		exit(1);
	}

	switch(sbp->st_size)
	{
		case FONT8X8:
			ch_height = HEIGHT8X8;
			ch_width = WIDTH8;
			break;

		case FONT8X10:
			ch_height = HEIGHT8X10;
			ch_width = WIDTH8;
			break;

		case FONT8X14:
			ch_height = HEIGHT8X14;
			ch_width = WIDTH8;
			break;

		case FONT8X16:
			ch_height = HEIGHT8X16;
			ch_width = WIDTH8;
			break;

		case FONT16X16:
			ch_height = HEIGHT16X16;
			ch_width = WIDTH16;
			break;

		default:
			fprintf(stderr,"error, file %s is no valid font file, size=%d\n",filename,sbp->st_size);
			exit(1);
	}

	if((fonttab = (unsigned char *)malloc((size_t)sbp->st_size)) == NULL)
	{
		fprintf(stderr,"error, malloc failed\n");
		exit(1);
	}

	strcpy(lfilename, filename);	/* save for write */
	lfilesize = sbp->st_size;	/* save for write */

	if((ret = fread(fonttab, sizeof(*fonttab), sbp->st_size, in)) != sbp->st_size)
	{
		sprintf(buffer,"error reading file %s, size = %d, ret = %d\n",filename,sbp->st_size, ret);
		perror(buffer);
		exit(1);
	}
}

/*---------------------------------------------------------------------------*
 *	write fontfile to disk
 *---------------------------------------------------------------------------*/
void writefont()
{
	FILE *in, *out;
	int ret;
	char buffer[1024];

	if((in = fopen(lfilename, "r")) != NULL)
	{
		int c;
		char wfn[1024];

		strcpy(wfn, lfilename);
		strcat(wfn, ".BAK");
		if((out = fopen(wfn, "w")) == NULL)
		{
			sprintf(buffer, "cannot open file %s for writing", wfn);
			perror(buffer);
			exit(1);
		}

		while(( c = fgetc(in) ) != EOF )
			fputc(c, out);

		fclose(out);
		fclose(in);
	}

	if((out = fopen(lfilename, "w")) == NULL)
	{
		sprintf(buffer, "cannot open file %s for writing", lfilename);
		perror(buffer);
		exit(1);
	}

	if((ret = fwrite(fonttab, sizeof(*fonttab), lfilesize, out)) != lfilesize)
	{
		sprintf(buffer,"error writing file %s, size=%d, ret=%d\n",lfilename,lfilesize, ret);
		perror(buffer);
		exit(1);
	}
}

/*---------------------------------------------------------------------------*
 *	display a string
 *---------------------------------------------------------------------------*/
void dis_cmd(char *strg)
{
	move(22,0);
	clrtoeol();
	mvaddstr(22,0,strg);
	refresh();
}

/*---------------------------------------------------------------------------*
 *	clear a command string
 *---------------------------------------------------------------------------*/
void clr_cmd(void)
{
	move(22,0);
	clrtoeol();
	refresh();
}

/*---------------------------------------------------------------------------*
 *	move char from src to dest
 *---------------------------------------------------------------------------*/
void move_ch(int src, int dst)
{
	unsigned char *s, *d;
	int offset = 0;

	if(ch_width == WIDTH16)
		offset = 2;
	else
		offset = 1;

	s = &(fonttab[ch_height * offset * src]);
	d = &(fonttab[ch_height * offset * dst]);

	bcopy(s, d, (ch_height*offset));	/* src -> dst */
}

/*---------------------------------------------------------------------------*
 *	exchange char's src and dest
 *---------------------------------------------------------------------------*/
void xchg_ch(int src, int dst)
{
	unsigned char *s, *d;
	unsigned char buf[32];
	int offset = 0;

	if(ch_width == WIDTH16)
		offset = 2;
	else
		offset = 1;

	s = &(fonttab[ch_height * offset * src]);
	d = &(fonttab[ch_height * offset * dst]);

	bcopy(s, buf, (ch_height*offset));	/* src -> tmp */
	bcopy(d, s, (ch_height*offset));	/* dst -> src */
	bcopy(buf, d, (ch_height*offset));	/* tmp -> dst */
}

/*---------------------------------------------------------------------------*
 *	display the current selected character
 *---------------------------------------------------------------------------*/
void display(int no)
{
	unsigned char *fontchar;
	char line[32];
	int ln_no;
	unsigned char hibyte;
	unsigned char lobyte;
	int offset;
	int r;

	offset = 0;
	r = 1;
	lobyte = 0;

	if(ch_width == WIDTH16)
		fontchar = &(fonttab[ch_height * 2 * no]);
	else
		fontchar = &(fonttab[ch_height * no]);

	for (ln_no = 0; ln_no < ch_height; ln_no++)
	{
		hibyte = *(fontchar + (offset++));

		if(ch_width == WIDTH16)
		{
			lobyte = *(fontchar + offset++);
		}

		strcpy(line,bitmask[(int)((hibyte >> 4) & 0x0f)]);
		strcat(line,bitmask[(int)(hibyte & 0x0f)]);

		if(ch_width == WIDTH16)
		{
			strcat(line,bitmask[(int)((lobyte >> 4) & 0x0f)]);
			strcat(line,bitmask[(int)(lobyte & 0x0f)]);
			mvwprintw(ch_win, r, 1, "%16.16s", line);
		}
		else
		{
			mvwprintw(ch_win, r, 1, "%8.8s", line);
		}
		r++;
	}
	wmove(ch_win, 1, 1);
	wrefresh(ch_win);
}

/*---------------------------------------------------------------------------*
 *	save character
 *---------------------------------------------------------------------------*/
void save_ch(void)
{
	unsigned char *s;
	int offset = 0;
	int r, c;
	unsigned short byte;
	unsigned short shift;

	if(ch_width == WIDTH16)
		offset = 2;
	else
		offset = 1;

	s = &(fonttab[ch_height * offset * curchar]);

	r = 1;

	while(r <= ch_height)
	{
		c = 1;
		byte = 0;
		if(offset == 2)
			shift = 0x8000;
		else
			shift = 0x80;

		while(c <= ch_width)
		{
			if(mvwinch(ch_win, r, c) == BLACK)
				byte |= shift;
			shift = (shift >> 1);
			c++;
		}
		*s++ = byte;
		r++;
	}
}

/*---------------------------------- E O F ----------------------------------*/


