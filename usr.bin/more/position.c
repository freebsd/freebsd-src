/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)position.c	5.7 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * Routines dealing with the "position" table.
 * This is a table which tells the position (in the input file) of the
 * first char on each currently displayed line.
 *
 * {{ The position table is scrolled by moving all the entries.
 *    Would be better to have a circular table 
 *    and just change a couple of pointers. }}
 */

#include <sys/types.h>
#include <less.h>

static off_t *table;		/* The position table */
static int tablesize;

extern int sc_height;

/*
 * Return the starting file position of a line displayed on the screen.
 * The line may be specified as a line number relative to the top
 * of the screen, but is usually one of these special cases:
 *	the top (first) line on the screen
 *	the second line on the screen
 *	the bottom line on the screen
 *	the line after the bottom line on the screen
 */
off_t
position(where)
	int where;
{
	switch (where)
	{
	case BOTTOM:
		where = sc_height - 2;
		break;
	case BOTTOM_PLUS_ONE:
		where = sc_height - 1;
		break;
	case MIDDLE:
		where = sc_height / 2;
	}
	return (table[where]);
}

/*
 * Add a new file position to the bottom of the position table.
 */
add_forw_pos(pos)
	off_t pos;
{
	register int i;

	/*
	 * Scroll the position table up.
	 */
	for (i = 1;  i < sc_height;  i++)
		table[i-1] = table[i];
	table[sc_height - 1] = pos;
}

/*
 * Add a new file position to the top of the position table.
 */
add_back_pos(pos)
	off_t pos;
{
	register int i;

	/*
	 * Scroll the position table down.
	 */
	for (i = sc_height - 1;  i > 0;  i--)
		table[i] = table[i-1];
	table[0] = pos;
}

copytable()
{
	register int a, b;

	for (a = 0; a < sc_height && table[a] == NULL_POSITION; a++);
	for (b = 0; a < sc_height; a++, b++) {
		table[b] = table[a];
		table[a] = NULL_POSITION;
	}
}

/*
 * Initialize the position table, done whenever we clear the screen.
 */
pos_clear()
{
	register int i;
	extern char *malloc(), *realloc();

	if (table == 0) {
		tablesize = sc_height > 25 ? sc_height : 25;
		table = (off_t *)malloc(tablesize * sizeof *table);
	} else if (sc_height >= tablesize) {
		tablesize = sc_height;
		table = (off_t *)realloc(table, tablesize * sizeof *table);
	}

	for (i = 0;  i < sc_height;  i++)
		table[i] = NULL_POSITION;
}

/*
 * See if the byte at a specified position is currently on the screen.
 * Check the position table to see if the position falls within its range.
 * Return the position table entry if found, -1 if not.
 */
onscreen(pos)
	off_t pos;
{
	register int i;

	if (pos < table[0])
		return (-1);
	for (i = 1;  i < sc_height;  i++)
		if (pos < table[i])
			return (i-1);
	return (-1);
}
