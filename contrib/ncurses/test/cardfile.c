/****************************************************************************
 * Copyright (c) 1999,2000 Free Software Foundation, Inc.                   *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/*
 * Author: Thomas E. Dickey <dickey@clark.net> 1999
 *
 * $Id: cardfile.c,v 1.5 2000/09/09 19:08:32 tom Exp $
 *
 * File format: text beginning in column 1 is a title; other text forms the content.
 */

#include <test.priv.h>

#include <form.h>
#include <panel.h>

#include <string.h>
#include <ctype.h>

#define VISIBLE_CARDS 10
#define OFFSET_CARD 2

#ifndef CTRL
#define CTRL(x)		((x) & 0x1f)
#endif

typedef struct _card {
    struct _card *link;
    PANEL *panel;
    FORM *form;
    char *title;
    char *content;
} CARD;

static CARD *all_cards;
static char default_name[] = "cardfile.dat";

#if !HAVE_STRDUP
#define strdup my_strdup
static char *
strdup(char *s)
{
    char *p = (char *) malloc(strlen(s) + 1);
    if (p)
	strcpy(p, s);
    return (p);
}
#endif /* not HAVE_STRDUP */

static const char *
skip(const char *buffer)
{
    while (isspace(*buffer))
	buffer++;
    return buffer;
}

static void
trim(char *buffer)
{
    unsigned n = strlen(buffer);
    while (n-- && isspace(buffer[n]))
	buffer[n] = 0;
}

/*******************************************************************************/

static CARD *
add_title(const char *title)
{
    CARD *card, *p, *q;

    for (p = all_cards, q = 0; p != 0; q = p, p = p->link) {
	int cmp = strcmp(p->title, title);
	if (cmp == 0)
	    return p;
	if (cmp > 0)
	    break;
    }

    card = (CARD *) calloc(1, sizeof(CARD));
    card->title = strdup(title);
    card->content = strdup("");

    if (q == 0) {
	card->link = all_cards;
	all_cards = card;
    } else {
	card->link = q->link;
	q->link = card;
    }

    return card;
}

static void
add_content(CARD * card, const char *content)
{
    unsigned total, offset;

    content = skip(content);
    if ((total = strlen(content)) != 0) {
	if ((offset = strlen(card->content)) != 0) {
	    total += 1 + offset;
	    card->content = (char *) realloc(card->content, total + 1);
	    strcpy(card->content + offset++, " ");
	} else {
	    card->content = (char *) malloc(total + 1);
	}
	strcpy(card->content + offset, content);
    }
}

static CARD *
new_card(void)
{
    CARD *card = add_title("");
    add_content(card, "");
    return card;
}

static CARD *
find_card(char *title)
{
    CARD *card;

    for (card = all_cards; card != 0; card = card->link)
	if (!strcmp(card->title, title))
	    break;

    return card;
}

static void
read_data(char *fname)
{
    FILE *fp;
    CARD *card = 0;
    char buffer[BUFSIZ];

    if ((fp = fopen(fname, "r")) != 0) {
	while (fgets(buffer, sizeof(buffer), fp)) {
	    trim(buffer);
	    if (isspace(*buffer)) {
		if (card == 0)
		    card = add_title("");
		add_content(card, buffer);
	    } else if ((card = find_card(buffer)) == 0) {
		card = add_title(buffer);
	    }
	}
	fclose(fp);
    }
}

/*******************************************************************************/

static void
write_data(const char *fname)
{
    FILE *fp;
    CARD *p = 0;
    int n;

    if (!strcmp(fname, default_name))
	fname = "cardfile.out";

    if ((fp = fopen(fname, "w")) != 0) {
	for (p = all_cards; p != 0; p = p->link) {
	    FIELD **f = form_fields(p->form);
	    for (n = 0; f[n] != 0; n++) {
		char *s = field_buffer(f[n], 0);
		if (s != 0
		    && (s = strdup(s)) != 0) {
		    trim(s);
		    fprintf(fp, "%s%s\n", n ? "\t" : "", s);
		    free(s);
		}
	    }
	}
	fclose(fp);
    }
}

/*******************************************************************************/

/*
 * Count the cards
 */
static int
count_cards(void)
{
    CARD *p;
    int count = 0;

    for (p = all_cards; p != 0; p = p->link)
	count++;

    return count;
}

/*
 * Shuffle the panels to keep them in a natural hierarchy.
 */
static void
order_cards(CARD * first, int depth)
{
    if (first) {
	if (depth && first->link)
	    order_cards(first->link, depth - 1);
	top_panel(first->panel);
    }
}

/*
 * Return the next card in the list
 */
static CARD *
next_card(CARD * now)
{
    if (now->link)
	now = now->link;
    return now;
}

/*
 * Return the previous card in the list
 */
static CARD *
prev_card(CARD * now)
{
    CARD *p;
    for (p = all_cards; p != 0; p = p->link)
	if (p->link == now)
	    return p;
    return now;
}

/*******************************************************************************/

static int
form_virtualize(WINDOW *w)
{
    int c = wgetch(w);

    switch (c) {
    case CTRL('W'):
	return (MAX_FORM_COMMAND + 4);
    case CTRL('N'):
	return (MAX_FORM_COMMAND + 3);
    case CTRL('P'):
	return (MAX_FORM_COMMAND + 2);
    case CTRL('Q'):
    case 033:
	return (MAX_FORM_COMMAND + 1);

    case KEY_BACKSPACE:
	return (REQ_DEL_PREV);
    case KEY_DC:
	return (REQ_DEL_CHAR);
    case KEY_LEFT:
	return (REQ_LEFT_CHAR);
    case KEY_RIGHT:
	return (REQ_RIGHT_CHAR);

    case KEY_DOWN:
    case KEY_NEXT:
	return (REQ_NEXT_FIELD);
    case KEY_UP:
    case KEY_PREVIOUS:
	return (REQ_PREV_FIELD);

    default:
	return (c);
    }
}

/*******************************************************************************/

static void
cardfile(char *fname)
{
    WINDOW *win;
    CARD *p;
    CARD *top_card;
    int visible_cards = count_cards();
    int panel_wide = COLS - (visible_cards * OFFSET_CARD);
    int panel_high = LINES - (visible_cards * OFFSET_CARD) - 5;
    int form_wide = panel_wide - 2;
    int form_high = panel_high - 2;
    int x = (visible_cards - 1) * OFFSET_CARD;
    int y = 0;
    int ch;
    int finished = FALSE;

    move(LINES - 3, 0);
    addstr("^Q/ESC -- exit form            ^W   -- writes data to file\n");
    addstr("^N   -- go to next card        ^P   -- go to previous card\n");
    addstr("Arrow keys move left/right within a field, up/down between fields");

    /* make a panel for each CARD */
    for (p = all_cards; p != 0; p = p->link) {
	FIELD **f = (FIELD **) calloc(3, sizeof(FIELD *));

	win = newwin(panel_high, panel_wide, x, y);
	keypad(win, TRUE);
	p->panel = new_panel(win);
	box(win, 0, 0);

	/* ...and a form in each panel */
	f[0] = new_field(1, form_wide, 0, 0, 0, 0);
	set_field_back(f[0], A_REVERSE);
	set_field_buffer(f[0], 0, p->title);

	f[1] = new_field(form_high - 1, form_wide, 1, 0, 0, 0);
	set_field_buffer(f[1], 0, p->content);
	set_field_just(f[1], JUSTIFY_LEFT);

	f[2] = 0;

	p->form = new_form(f);
	set_form_win(p->form, win);
	set_form_sub(p->form, derwin(win, form_high, form_wide, 1, 1));
	post_form(p->form);

	x -= OFFSET_CARD;
	y += OFFSET_CARD;
    }

    order_cards(top_card = all_cards, visible_cards);

    while (!finished) {
	update_panels();
	doupdate();

	switch (form_driver(top_card->form, ch =
			    form_virtualize(panel_window(top_card->panel)))) {
	case E_OK:
	    break;
	case E_UNKNOWN_COMMAND:
	    switch (ch) {
	    case MAX_FORM_COMMAND + 1:
		finished = TRUE;
		break;
	    case MAX_FORM_COMMAND + 2:
		top_card = prev_card(top_card);
		order_cards(top_card, visible_cards);
		break;
	    case MAX_FORM_COMMAND + 3:
		top_card = next_card(top_card);
		order_cards(top_card, visible_cards);
		break;
	    case MAX_FORM_COMMAND + 4:
		write_data(fname);
		break;
	    default:
		beep();
		break;
	    }
	    break;
	default:
	    flash();
	    break;
	}
    }
}

/*******************************************************************************/

int
main(int argc, char *argv[])
{
    int n;

    initscr();
    cbreak();
    noecho();

    if (argc > 1) {
	for (n = 1; n < argc; n++)
	    read_data(argv[n]);
	if (count_cards() == 0)
	    new_card();
	cardfile(argv[1]);
    } else {
	read_data(default_name);
	if (count_cards() == 0)
	    new_card();
	cardfile(default_name);
    }

    endwin();

    return EXIT_SUCCESS;
}
