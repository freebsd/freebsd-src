/*
 * Copyright (c) 1988 Mark Nudleman
 * Portions copyright (c) 1999 T. Michael Vanderhoek
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)command.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Functions for interacting with the user directly printing hello
 * messages or reading from the terminal.  All of these functions deal
 * specifically with the prompt line, and only the prompt line.
 */

#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "less.h"
#include "pathnames.h"

extern int erase_char, kill_char, werase_char;
extern int sigs;
extern int quit_at_eof;
extern int hit_eof;
extern int horiz_off;
extern int sc_width;
extern int bo_width;
extern int be_width;
extern int so_width;
extern int se_width;
extern int curr_ac;
extern int ac;
extern char **av;
extern int screen_trashed;	/* The screen has been overwritten */

static int cmd_col;		/* Current screen column when accepting input */

static cmd_char(), cmd_erase(), getcc();


/*****************************************************************************
 *
 * Functions for reading-in user input.
 *
 */

static int biggetinputhack_f;

/* biggetinputhack()
 *
 * Performs as advertised.
 */
biggetinputhack()
{
	biggetinputhack_f = 1;
}

/*
 * Read a line of input from the terminal.  Reads at most bufsiz - 1 characters
 * and places them in buffer buf.  They are NUL-terminated.  Prints the
 * temporary prompt prompt.  Returns true if the user aborted the input and
 * returns false otherwise.
 */
int
getinput(prompt, buf, bufsiz)
	const char *prompt;
	char *buf;
	int bufsiz;
{
	extern bo_width, be_width;
	char *bufcur;
	int c;

	prmpt(prompt);

	bufcur = buf;
	for (;;) {
		c = getcc();
		if (c == '\n') {
			*bufcur = '\0';
			return 0;
		}
		if (c == READ_INTR ||
		    cmd_char(c, buf, &bufcur, buf + bufsiz - 1)) {
			/* input cancelled */
			if (bufsiz) *buf = '\0';
			return 1;
		}
		if (biggetinputhack_f) {
			biggetinputhack_f = 0;
			*bufcur = '\0';
			return 0;
		}
	}
}

/*
 * Process a single character of a multi-character input, such as
 * a number, or the pattern of a search command.  Returns true if the user
 * has cancelled the multi-character input, false otherwise and attempts
 * to add it to buf (not exceeding bufsize).  Prints the character on the
 * terminal output.  The bufcur should initially equal bufbeg.  After that
 * it does not need to be touched or modified by the user, but may be expected
 * to point at the future position of the next character.
 */
static int
cmd_char(c, bufbeg, bufcur, bufend)
	int c;          /* The character to process */
	char *bufbeg;   /* The buffer to add the character to */
	char **bufcur;  /* The position at which to add the character */
	char *bufend;   /* One after the last address available in the buffer.
	                 * No character will be placed into *bufend. */
{
	if (c == erase_char)
		return(cmd_erase(bufbeg, bufcur));
	/* in this order, in case werase == erase_char */
	if (c == werase_char) {
		if (*bufcur > bufbeg) {
			while (isspace((*bufcur)[-1]) &&
			    !cmd_erase(bufbeg, bufcur)) ;
			while (!isspace((*bufcur)[-1]) &&
			    !cmd_erase(bufbeg, bufcur)) ;
			while (isspace((*bufcur)[-1]) &&
			    !cmd_erase(bufbeg, bufcur)) ;
		}
		return *bufcur == bufbeg;
	}
	if (c == kill_char) {
		while (!cmd_erase(bufbeg, bufcur));
		return 1;
	}

	/*
	 * No room in the command buffer, or no room on the screen;
	 * XXX If there is no room on the screen, we should just let the
	 * screen scroll down and set screen_trashed=1 appropriately, or
	 * alternatively, scroll the prompt line horizontally.
	 */
	assert (*bufcur <= bufend);
	if (*bufcur == bufend || cmd_col >= sc_width - 3)
		bell();
	else {
		*(*bufcur)++ = c;
		if (CONTROL_CHAR(c)) {
			putchr('^');
			cmd_col++;
			c &= ~0200;
			c = CARAT_CHAR(c);
		}
		putchr(c);
		cmd_col++;
	}
	return 0;
}

/*
 * Helper function to cmd_char().  Backs-up one character from bufcur in the
 * buffer passed, and prints a backspace on the screen.  Returns true if the
 * we backspaced past bufbegin (ie. the input is being aborted), and false
 * otherwise.  The bufcur is expected to point to the future location of the
 * next character in the buffer, and is modified appropriately.
 */
static
cmd_erase(bufbegin, bufcur)
	char *bufbegin;
	char **bufcur;
{
	int c;

	/*
	 * XXX Could add code to detect a backspace that is backing us over
	 * the beginning of a line and onto the previous line.  The backspace
	 * would not be printed for some terminals (eg. hardcopy) in that
	 * case.
	 */

	/*
	 * backspace past beginning of the string: this usually means
	 * abort the input.
	 */
	if (*bufcur == bufbegin)
		return 1;

	(*bufcur)--;

	/* If erasing a control-char, erase an extra character for the carat. */
	c = **bufcur;
	if (CONTROL_CHAR(c)) {
		backspace();
		cmd_col--;
	}

	backspace();
	cmd_col--;

	return 0;
}

static int ungotcc;

/*
 * Get command character from the terminal.
 */
static
getcc()
{
	int ch;
	off_t position();

	/* left over from error() routine. */
	if (ungotcc) {
		ch = ungotcc;
		ungotcc = 0;
		return(ch);
	}

	return(getchr());
}

/*
 * Same as ungetc(), but works for people who don't like to use streams.
 */
ungetcc(c)
	int c;
{
	ungotcc = c;
}


/*****************************************************************************
 *
 * prompts
 *
 */

static int longprompt;

/*
 * Prints prmpt where the prompt would normally appear.  This is different
 * from changing the current prompt --- this is more like printing a
 * unimportant notice or error.  The prmpt line will be printed in bold (if
 * possible).  Will in the future print only the last sc_width - 1 - bo_width
 * characters (to prevent newline).  
 */
prmpt(prmpt)
	const char *prmpt;
{
	lower_left();
	clear_eol();
	bo_enter();
	putxstr(prmpt);
	bo_exit();
	flush();
	cmd_col = strlen(prmpt) + bo_width + be_width;
}

/*
 * Print the main prompt that signals we are ready for user commands.  This
 * also magically positions the current file where it should be (either by
 * calling repaint() if screen_trashed or by searching for a search
 * string that was specified through option.c on the more(1) command line).
 * Additional magic will randomly call the quit() function.
 *
 * This is really intended to do a lot of the work of commands().  It has
 * little purpose outside of commands().
 */
prompt()
{
	extern int linenums, short_file, ispipe;
	extern char *current_name, *firstsearch, *next_name;
	off_t len, pos, ch_length(), position(), forw_line();
	char pbuf[40];

	/*
	 * if nothing is displayed yet, display starting from line 1;
	 * if search string provided, go there instead.
	 */
	if (position(TOP) == NULL_POSITION) {
#if 0
/* This code causes "more zero-byte-file /etc/termcap" to skip straight
 * to the /etc/termcap file ... that is undesireable.  There are only a few
 * instances where these two lines perform something useful. */
		if (forw_line((off_t)0) == NULL_POSITION)
			return 0 ;
#endif
		if (!firstsearch || !search(1, firstsearch, 1, 1))
			jump_back(1);
	}
	else if (screen_trashed)
		repaint();

	/* if no -e flag and we've hit EOF on the last file, quit. */
	if (!quit_at_eof && hit_eof && curr_ac + 1 >= ac)
		quit();

	/* select the proper prompt and display it. */
	lower_left();
	clear_eol();
	pbuf[sizeof(pbuf) - 1] = '\0';
	if (longprompt) {
		/*
		 * Get the current line/pos from the BOTTOM of the screen
		 * even though that's potentially confusing for the user
		 * when switching between wraplines=true and a valid horiz_off
		 * (with wraplines=false).  In exchange, it is sometimes
		 * easier for the user to tell when a file is relatively
		 * short vs. long.
		 */
		so_enter();
		putstr(current_name);
		putstr(":");
		if (!ispipe) {
			(void)snprintf(pbuf, sizeof(pbuf) - 1,
			    " file %d/%d", curr_ac + 1, ac);
			putstr(pbuf);
		}
		if (linenums) {
			(void)snprintf(pbuf, sizeof(pbuf) - 1,
			    " line %d", currline(BOTTOM));
			putstr(pbuf);
		}
		(void)snprintf(pbuf, sizeof(pbuf) - 1, " col %d", horiz_off);
		putstr(pbuf);
		if ((pos = position(BOTTOM)) != NULL_POSITION) {
			(void)snprintf(pbuf, sizeof(pbuf) - 1,
			    " byte %qd", pos);
			putstr(pbuf);
			if (!ispipe && (len = ch_length())) {
				(void)snprintf(pbuf, sizeof(pbuf) - 1,
				    "/%qd pct %qd%%", len, ((100 * pos) / len));
				putstr(pbuf);
			}
		}
		so_exit();
	}
	else {
		so_enter();
		putstr(current_name);
		if (hit_eof)
			if (next_name) {
				putstr(": END (next file: ");
				putstr(next_name);
				putstr(")");
			}
			else
				putstr(": END");
		else if (!ispipe &&
		    (pos = position(BOTTOM)) != NULL_POSITION &&
		    (len = ch_length())) {
			(void)snprintf(pbuf, sizeof(pbuf) - 1,
			    " (%qd%%)", ((100 * pos) / len));
			putstr(pbuf);
		}
		so_exit();
	}

	/*
	 * XXX This isn't correct, but until we get around to reworking
	 * the whole prompt stuff the way we want it to be, this hack
	 * is necessary to prevent input from being blocked if getinput()
	 * is called and the user enters an input that fills the cmd
	 * buffer (or reaches the far rightside end of the screen).
	 */
	cmd_col = 0;

	return 1;
}

/*
 * Sets the current prompt.  Currently it sets the current prompt to the
 * long prompt.
 */
statprompt(nostatprompt)
	int nostatprompt;  /* Turn off the stat prompt?  (off by default...) */
{
	if (nostatprompt)
		longprompt = 0;
	else
		longprompt = 1;
}


/*****************************************************************************
 *
 * Errors, next-of-kin to prompts.
 *
 */

/*
 * Shortcut function that may be used when setting the current erreur
 * and erreur string at the same time.  The function name is chosen to be
 * symetric with the SETERR() macro in less.h.  This could be written as
 * macro, too, but we'd need to use a GNU C extension.
 */
SETERRSTR(enum error e, const char *s, ...)
{
	va_list args;

	erreur = e;
	if (errstr) free(errstr);
	errstr = NULL;
	va_start(args, s);
	vasprintf(&errstr, s, args);
	va_end(args);
}

/*
 * Prints an error message and clears the current error.
 */
handle_error()
{
	if (erreur == E_OK)
		return;

	bell();
	if (errstr)
		error(errstr);
	else
		error(deferr[erreur]);
	erreur = E_OK;
	errstr = NULL;
}

/*
 * Clears any error messages and pretends they never occurred.
 */
clear_error()
{
	erreur = E_OK;
	if (errstr) free(errstr);
	errstr = NULL;
}

int errmsgs;
static char return_to_continue[] = "(press RETURN)";

/*
 * Output a message in the lower left corner of the screen
 * and wait for carriage return.
 */
/* static */
error(s)
	char *s;
{
	extern int any_display;
	int ch;

	errmsgs++;
	if (!any_display) {
		/*
		 * Nothing has been displayed yet.  Output this message on
		 * error output (file descriptor 2) and don't wait for a
		 * keystroke to continue.
		 *
		 * This has the desirable effect of producing all error
		 * messages on error output if standard output is directed
		 * to a file.  It also does the same if we never produce
		 * any real output; for example, if the input file(s) cannot
		 * be opened.  If we do eventually produce output, code in
		 * edit() makes sure these messages can be seen before they
		 * are overwritten or scrolled away.
		 */
		(void)write(2, s, strlen(s));
		(void)write(2, "\n", 1);
		return;
	}

	lower_left();
	clear_eol();
	so_enter();
	if (s) {
		putstr(s);
		putstr("  ");
	}
	putstr(return_to_continue);
	so_exit();

	if ((ch = getchr()) != '\n') {
		/* XXX hardcoded */
		if (ch == 'q')
			quit();
		ungotcc = ch;
	}
	lower_left();

	if ((s==NULL)?0:(strlen(s)) + sizeof(return_to_continue) +
	    so_width + se_width + 1 > sc_width) {
		/*
		 * Printing the message has probably scrolled the screen.
		 * {{ Unless the terminal doesn't have auto margins,
		 *    in which case we just hammered on the right margin. }}
		 */
		/* XXX Should probably just set screen_trashed=1, but I'm
		 * not going to touch that until all the places that call
		 * error() have been checked, or until error() is staticized. */
		repaint();
	}
	flush();
}


/****************************************************************************
 *
 * The main command processor.
 *
 * (Well, it deals with things on the prompt line, doesn't it?)
 *
 */

/*
 * Main command processor.
 *
 * Accept and execute commands until a quit command, then return.
 */
commands()
{
	enum runmacro runmacro();
	enum runmacro rmret;
	long numberN;
	enum { NOTGOTTEN=0, GOTTEN=1, GETTING } Nstate;  /* ie. numberNstate */
	int c;
	char inbuf[20], *incur = inbuf;
	*inbuf = '\0';

	Nstate = GETTING;
	for (;;) {
		/*
		 * See if any signals need processing.
		 */
		if (sigs)
			psignals();

		/*
		 * Display prompt and generally get setup.  Don't display the
		 * prompt if we are already in the middle of accepting a
		 * set of characters.
		 */
		if (!*inbuf && !prompt()) {
			next_file(1);
			continue;
		}

		c = getcc();

		/* Check sigs here --- getcc() may have given us READ_INTR */
		if (sigs) {
			/* terminate any current macro */
			*inbuf = '\0';
			incur = inbuf;

			continue;  /* process the sigs */
		}

		if (Nstate == GETTING && !isdigit(c)
		    && c != erase_char && c != werase_char && c != kill_char) {
			/*
			 * Mark the end of an input number N, if any.
			 */

			if (!*inbuf) {
				/* We never actually got an input number */
				Nstate = NOTGOTTEN;
			} else {
				numberN = atol(inbuf);
				Nstate = GOTTEN;
			}
			*inbuf = '\0';
			incur = inbuf;
		}
		(void) cmd_char(c, inbuf, &incur, inbuf + sizeof(inbuf) - 1);
		*incur = '\0';
		if (*inbuf)
			prmpt(inbuf);
		else
			Nstate = GETTING;  /* abort command */

		if (Nstate == GETTING) {
			/* Still reading in the number N ... don't want to
			 * try running the macro expander. */
			continue;
		} else {
			/* Try expanding the macro */
			switch (runmacro(inbuf, numberN, Nstate)) {
			case TOOMACRO:
				break;
			case BADMACRO: case NOMACRO: case BADCOMMAND:
				handle_error();
				/* fallthrough */
			case OK:
				/* recock */
				*inbuf = '\0';
				incur = inbuf;
				Nstate = GETTING;
				break;
			}
		}
	}  /* for (;;) */
}


/*****************************************************************************
 *
 * Misc functions that belong in ncommand.c but are here for historical
 * and for copyright reasons.
 * 
 */

editfile()
{
	off_t position();
	extern char *current_file;
	static int dolinenumber;
	static char *editor;
	char *base;
	int linenumber;
	char buf[MAXPATHLEN * 2 + 20], *getenv();

	if (editor == NULL) {
		editor = getenv("EDITOR");

		/* default editor is vi */
		if (editor == NULL || *editor == '\0')
			editor = _PATH_VI;

		/* check last component in case of full path */
		base = strrchr(editor, '/');
		if (!base)
			base = editor;
		else
			base++;

		/* emacs also accepts vi-style +nnnn */
		if (strncmp(base, "vi", 2) == 0 || strcmp(base, "emacs") == 0)
			dolinenumber = 1;
		else
			dolinenumber = 0;
	}
	/*
	 * XXX Can't just use currline(MIDDLE) since that might be NULL_POSITION
	 * if we are editting a short file or some kind of search positioned
	 * us near the last line.  It's not clear what currline() should do
	 * in those circumstances, but as of this writing, it doesn't do
	 * anything reasonable from our perspective.  The currline(MIDDLE)
	 * never had the desired results for an editfile() after a search()
	 * anyways.  Note, though, that when vi(1) starts its editting, it
	 * positions the focus line in the middle of the screen, not the top.
	 *
	 * I think what is needed is some kind of setfocus() and getfocus()
	 * function.  This could put the focussed line in the middle, top,
	 * or wherever as per the user's wishes, and allow things like us
	 * to getfocus() the correct file-position/line-number.  A search would
	 * then search forward (or backward) from the current focus position,
	 * etc.
	 *
	 * currline() doesn't belong.
	 */
	if (position(MIDDLE) == NULL_POSITION)
		linenumber = currline(TOP);
	else
		linenumber = currline(MIDDLE);
	if (dolinenumber && linenumber)
		(void)snprintf(buf, sizeof(buf),
		    "%s +%d %s", editor, linenumber, current_file);
	else
		(void)snprintf(buf, sizeof(buf), "%s %s", editor, current_file);
	lsystem(buf);
}

showlist()
{
	extern int sc_width;
	register int indx, width;
	int len;
	char *p;

	if (ac <= 0) {
		error("No files provided as arguments.");
		return;
	}
	for (width = indx = 0; indx < ac;) {
		p = strcmp(av[indx], "-") ? av[indx] : "stdin";
		len = strlen(p) + 1;
		if (curr_ac == indx)
			len += 2;
		if (width + len + 1 >= sc_width) {
			if (!width) {
				if (curr_ac == indx)
					putchr('[');
				putstr(p);
				if (curr_ac == indx)
					putchr(']');
				++indx;
			}
			width = 0;
			putchr('\n');
			continue;
		}
		if (width)
			putchr(' ');
		if (curr_ac == indx)
			putchr('[');
		putstr(p);
		if (curr_ac == indx)
			putchr(']');
		width += len;
		++indx;
	}
	putchr('\n');
	error((char *)NULL);
}
