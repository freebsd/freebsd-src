
/* This file contains code for X-CHESS.
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of X-CHESS.

X-CHESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the X-CHESS General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
X-CHESS, but only under the conditions described in the
X-CHESS General Public License.   A copy of this license is
supposed to have been given to you along with X-CHESS so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* RCS Info: $Revision: 1.4 $ on $Date: 86/11/23 17:18:20 $
 *           $Source: /users/faustus/xchess/RCS/record.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Deal with recording moves.
 */

#include "xchess.h"

#undef smartass

bool record_english = true;
char *record_file = DEF_RECORD_FILE;
int movenum = 0;
bool saveflag = false;

static char *colnames[] = { "qr", "qn", "qb", "q", "k", "kb", "kn", "kr" } ;
static char *pcnames[] = { "P", "R", "N", "B", "Q", "K" } ;

static char *movestring();
static char *tstring();
static FILE *backup;

#define RECORD_HEADER	"\n1    XChess Game Record0\n"

void
record_init(win)
	windata *win;
{
	int i;

	i = XTextWidth(win->medium, RECORD_HEADER,
		       sizeof(RECORD_HEADER) - 1);
	i = (40 * win->small->max_bounds.width - i *
	     win->medium->max_bounds.width) / 
			win->medium->max_bounds.width / 2;
	TxtGrab(win->display, win->recwin, "xchess", win->small, win->textback.pixel,
			win->textcolor.pixel, win->cursorcolor.pixel);
	TxtAddFont(win->display, win->recwin, 1, win->medium, win->textcolor.pixel);
	for (; i > 0; i++)
		TxtWriteStr(win->display, win->recwin, " ");
	TxtWriteStr(win->display, win->recwin, RECORD_HEADER);
	
	if (saveflag) {
		if (!(backup = fopen(record_file, "w"))) {
			perror(record_file);
			saveflag = false;
		} else {
			fprintf(backup, "X Chess -- %s\n", datestring());
			if (dispname2)
				fprintf(backup, "\tWhite on %s, black on %s\n", 
						dispname1, dispname2);
			else
				fprintf(backup, "\tGame played on %s\n",
						dispname1);
			fprintf(backup, "\t%s\n", record_english ? "english" :
					"algebraic");
			fflush(backup);
		}
	}

	movenum = 0;
	return;
}

void
record_reset()
{
	TxtWriteStr(win1->display, win1->recwin, "\n\n1    New Game0\n\n");
	if (!oneboard) {
		TxtWriteStr(win2->display, win2->recwin, "\n\n1    New Game0\n\n");
	}
	movenum = 0;
	if (saveflag) {
		fprintf(backup, "\n\nNew Game\n\n");
		fflush(backup);
	}
	return;
}

void
record_end(s)
	char *s;
{
	char buf[BSIZE];

	sprintf(buf, "\n%s\n", s);
	TxtWriteStr(win1->display, win1->recwin, s);
	if (!oneboard) {
		TxtWriteStr(win2->display, win2->recwin, s);
	}
	if (saveflag) {
		fprintf(backup, "\n%s\n", s);
		fprintf(backup, "Time: white: %s, ", tstring(whiteseconds));
		fprintf(backup, "black: %s\n", tstring(blackseconds));
		fclose(backup);
	}
	return;
}

void
record_save()
{
	move *m;
	FILE *fp;
	int i;
	char *s;

	if (!(fp = fopen(record_file, "w"))) {
		perror(record_file);
		return;
	}
	fprintf(fp, "X Chess -- %s\n", datestring());
	if (dispname2)
		fprintf(fp, "\tWhite on %s, black on %s\n", 
				dispname1, dispname2);
	else
		fprintf(fp, "\tGame played on %s\n", dispname1);
	fprintf(fp, "\t%s\n", record_english ? "english" : "algebraic");

	for (m = moves, i = 1; m; i++) {
		s = movestring(m);
		fprintf(fp, "%2d. %-16s ", i, s);
		m = m->next;
		if (m)
			s = movestring(m);
		else
			s = "";
		fprintf(fp, "%s\n", s);
		if (m)
			m = m->next;
	}
	fclose(fp);
	return;
}

void
record_move(m)
	move *m;
{
	char *s, buf[BSIZE];

	s = movestring(m);

	if (m->piece.color == WHITE) {
		movenum++;
		sprintf(buf, "%2d. %-16s ", movenum, s);
	} else {
		sprintf(buf, "%s\n", s);
	}
	TxtWriteStr(win1->display, win1->recwin, buf);
	if (!oneboard) {
		TxtWriteStr(win2->display, win2->recwin, buf);
	}
	if (saveflag) {
		fprintf(backup, "%s", buf);
		fflush(backup);
	}

	return;
}

void
record_back()
{
    extern move *lastmove;
    move *m = lastmove;
    char *s = movestring(m);
    char buf[BSIZE];
    long i;

    if (m->piece.color == WHITE) {
	sprintf(buf, "%2d. %-16s ", movenum, s);
    } else {
	sprintf(buf, "%s\n", s);
    }
    s = buf;
    for (i = 0; *s != '\0'; i++)
	*s++ = '';		/* control H, backspace */
   
    TxtWriteStr(win1->display, win1->recwin, buf);
    if (!oneboard) {
	TxtWriteStr(win2->display, win2->recwin, buf);
    }

    if (nexttomove == BLACK)
	movenum--;
    if (saveflag) {
	fseek(backup, -i, 1);
	fflush(backup);
    }

    return;
}

static char *
movestring(m)
	move *m;
{
	int fy, ty;
	static char buf[BSIZE];

	if (!record_english || (m->piece.color == WHITE)) {
		fy = SIZE - m->fromy;
		ty = SIZE - m->toy;
	} else {
		fy = m->fromy + 1;
		ty = m->toy + 1;
	}

	switch (m->type) {
	    case MOVE:
		if (record_english)
			sprintf(buf, "%s/%s%d-%s%d%s", pcnames[(int) m->piece.
					type], colnames[m->fromx], fy,
					colnames[m->tox], ty, m->check ? "+" :
					"");
		else
			sprintf(buf, "%c%d%c%d", 'a' + m->fromx, fy, 'a' +
					m->tox, ty);
		break;
	    case CAPTURE:
		if (record_english)
			sprintf(buf, "%s/%s%dx%s/%s%d%s%s",
					pcnames[(int) m->piece.type],
					colnames[m->fromx], fy,
					pcnames[(int) m->taken.type],
					colnames[m->tox], ty,
					m->enpassant ? "e.p." : "",
					m->check ? "+" : "");
		else
			sprintf(buf, "%c%d%c%d", 'a' + m->fromx, fy, 'a' +
					m->tox, ty);
		break;

	    case KCASTLE:
		if (record_english)
			sprintf(buf, "O-O%s", m->check ? "ch" : "");
		else if (m->piece.color == WHITE)
			strcpy(buf, "e1g1");
		else
			strcpy(buf, "e8g8");
		break;

	    case QCASTLE:
		if (record_english)
			sprintf(buf, "O-O-O%s", m->check ? "ch" : "");
		else if (m->piece.color == WHITE)
			strcpy(buf, "e1c1");
		else
			strcpy(buf, "e8c8");
		break;

	    default:
		sprintf(buf, "something strange");
		break;
	}
	if ((m->piece.type == PAWN) && (((m->piece.color == BLACK) && 
			(m->toy == 7)) || ((m->piece.color == WHITE) &&
			(m->toy == 0)))) 
		strcat(buf, "(Q)");

#ifdef smartass
	if (!(random() % 50))
		strcat(buf, "?");
	else if (!(random() % 50))
		strcat(buf, "!");
	else if (!(random() % 500))
		strcat(buf, "???");
	else if (!(random() % 500))
		strcat(buf, "!!!");
#endif smartass

	return (buf);
}

static char *
tstring(s)
	int s;
{
	static char buf[64];

	if (s > 3600)
		sprintf(buf, "%dh %dm %ds", s / 3600, (s % 3600) / 60, s % 60);
	else if (s > 60)
		sprintf(buf, "%dm %ds", (s % 3600) / 60, s % 60);
	else
		sprintf(buf, "%ds", s);
	return (buf);
}

