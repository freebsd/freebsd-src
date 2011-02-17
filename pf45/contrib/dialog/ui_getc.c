/*
 *  $Id: ui_getc.c,v 1.48 2010/01/18 10:24:06 tom Exp $
 *
 * ui_getc.c - user interface glue for getc()
 *
 * Copyright 2001-2009,2010 Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 2.1
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to
 *	Free Software Foundation, Inc.
 *	51 Franklin St., Fifth Floor
 *	Boston, MA 02110, USA.
 */

#include <dialog.h>
#include <dlg_keys.h>

#ifdef NEED_WCHAR_H
#include <wchar.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef __QNX__
#include <sys/select.h>
#endif

#ifndef WEXITSTATUS
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WEXITSTATUS(status)	(status.w_retcode)
# else
#  define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
# endif
#endif

#ifndef WTERMSIG
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WTERMSIG(status)	(status.w_termsig)
# else
#  define	WTERMSIG(status)	((status) & 0x7f)
# endif
#endif

void
dlg_add_callback(DIALOG_CALLBACK * p)
{
    p->next = dialog_state.getc_callbacks;
    dialog_state.getc_callbacks = p;
    wtimeout(p->win, WTIMEOUT_VAL);
}

/*
 * Like dlg_add_callback(), but providing for cleanup of caller's associated
 * state.
 */
void
dlg_add_callback_ref(DIALOG_CALLBACK ** p, DIALOG_FREEBACK freeback)
{
    (*p)->caller = p;
    (*p)->freeback = freeback;
    dlg_add_callback(*p);
}

void
dlg_remove_callback(DIALOG_CALLBACK * p)
{
    DIALOG_CALLBACK *q;

    if (p->input != 0) {
	fclose(p->input);
	if (p->input == dialog_state.pipe_input)
	    dialog_state.pipe_input = 0;
	p->input = 0;
    }

    if (!(p->keep_win))
	dlg_del_window(p->win);
    if ((q = dialog_state.getc_callbacks) == p) {
	dialog_state.getc_callbacks = p->next;
    } else {
	while (q != 0) {
	    if (q->next == p) {
		q->next = p->next;
		break;
	    }
	    q = q->next;
	}
    }

    /* handle dlg_add_callback_ref cleanup */
    if (p->freeback != 0)
	p->freeback(p);
    if (p->caller != 0)
	*(p->caller) = 0;

    free(p);
}

/*
 * FIXME: this could be replaced by a select/poll on several file descriptors
 */
static int
dlg_getc_ready(DIALOG_CALLBACK * p)
{
    fd_set read_fds;
    int fd = fileno(p->input);
    struct timeval test;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    test.tv_sec = 0;		/* Seconds.  */
    test.tv_usec = (isatty(fd)	/* Microseconds.  */
		    ? (WTIMEOUT_VAL * 1000)
		    : 1);
    return (select(fd + 1, &read_fds, (fd_set *) 0, (fd_set *) 0, &test) == 1)
	&& (FD_ISSET(fd, &read_fds));
}

int
dlg_getc_callbacks(int ch, int fkey, int *result)
{
    int code = FALSE;
    DIALOG_CALLBACK *p, *q;

    if ((p = dialog_state.getc_callbacks) != 0) {
	do {
	    q = p->next;
	    if (dlg_getc_ready(p)) {
		if (!(p->handle_getc(p, ch, fkey, result))) {
		    dlg_remove_callback(p);
		}
	    }
	} while ((p = q) != 0);
	code = (dialog_state.getc_callbacks != 0);
    }
    return code;
}

static void
dlg_raise_window(WINDOW *win)
{
    touchwin(win);
    wmove(win, getcury(win), getcurx(win));
    wnoutrefresh(win);
    doupdate();
}

/*
 * This is a work-around for the case where we actually need the wide-character
 * code versus a byte stream.
 */
static int last_getc = ERR;

#ifdef USE_WIDE_CURSES
static char last_getc_bytes[80];
static int have_last_getc;
static int used_last_getc;
#endif

int
dlg_last_getc(void)
{
#ifdef USE_WIDE_CURSES
    if (used_last_getc != 1)
	return ERR;		/* not really an error... */
#endif
    return last_getc;
}

void
dlg_flush_getc(void)
{
    last_getc = ERR;
#ifdef USE_WIDE_CURSES
    have_last_getc = 0;
    used_last_getc = 0;
#endif
}

/*
 * Check if the stream has been unexpectedly closed, returning false in that
 * case.
 */
static bool
valid_file(FILE *fp)
{
    bool code = FALSE;
    int fd = fileno(fp);

    if (fd >= 0) {
	long result = 0;
	if ((result = fcntl(fd, F_GETFL, 0)) >= 0) {
	    code = TRUE;
	}
    }
    return code;
}

/*
 * Read a character from the given window.  Handle repainting here (to simplify
 * things in the calling application).  Also, if input-callback(s) are set up,
 * poll the corresponding files and handle the updates, e.g., for displaying a
 * tailbox.
 */
int
dlg_getc(WINDOW *win, int *fkey)
{
    WINDOW *save_win = win;
    int ch = ERR;
    int before_lookup;
    int result;
    bool done = FALSE;
    bool literal = FALSE;
    DIALOG_CALLBACK *p;
    int interval = dialog_vars.timeout_secs;
    time_t expired = time((time_t *) 0) + dialog_vars.timeout_secs;
    time_t current;

    if (dialog_state.getc_callbacks != 0)
	wtimeout(win, WTIMEOUT_VAL);
    else if (interval > 0)
	wtimeout(win, interval);

    while (!done) {
#ifdef USE_WIDE_CURSES
	int code;
	mbstate_t state;
	wchar_t my_wchar;
	wint_t my_wint;

	/*
	 * We get a wide character, translate it to multibyte form to avoid
	 * having to change the rest of the code to use wide-characters.
	 */
	if (used_last_getc >= have_last_getc) {
	    used_last_getc = 0;
	    have_last_getc = 0;
	    ch = ERR;
	    *fkey = 0;
	    code = wget_wch(win, &my_wint);
	    my_wchar = (wchar_t) my_wint;
	    switch (code) {
	    case KEY_CODE_YES:
		ch = *fkey = my_wchar;
		last_getc = my_wchar;
		break;
	    case OK:
		memset(&state, 0, sizeof(state));
		have_last_getc = (int) wcrtomb(last_getc_bytes, my_wchar, &state);
		if (have_last_getc < 0) {
		    have_last_getc = used_last_getc = 0;
		    last_getc_bytes[0] = (char) my_wchar;
		}
		ch = (int) CharOf(last_getc_bytes[used_last_getc++]);
		last_getc = my_wchar;
		break;
	    case ERR:
		ch = ERR;
		last_getc = ERR;
		break;
	    default:
		break;
	    }
	} else {
	    ch = (int) CharOf(last_getc_bytes[used_last_getc++]);
	}
#else
	ch = wgetch(win);
	last_getc = ch;
	*fkey = (ch > KEY_MIN && ch < KEY_MAX);
#endif
	if (literal) {
	    done = TRUE;
	    continue;
	}

	before_lookup = ch;
	ch = dlg_lookup_key(win, ch, fkey);
	dlg_trace_chr(ch, *fkey);

	current = time((time_t *) 0);

	switch (ch) {
	case CHR_LITERAL:
	    if (!literal) {
		literal = TRUE;
		keypad(win, FALSE);
		continue;
	    }
	    break;
	case CHR_REPAINT:
	    (void) touchwin(win);
	    (void) wrefresh(curscr);
	    break;
	case ERR:		/* wtimeout() in effect; check for file I/O */
	    if (interval > 0
		&& current >= expired) {
		dlg_exiterr("timeout");
	    }
	    if (dlg_getc_callbacks(ch, *fkey, &result)) {
		dlg_raise_window(win);
	    } else {
		done = (interval <= 0);
	    }
	    if (!valid_file(stdin)
		|| !valid_file(dialog_state.screen_output)) {
		ch = ESC;
		done = TRUE;
	    }
	    break;
	case DLGK_FIELD_NEXT:
	    /* FALLTHRU */
	case TAB:
	    /* Handle tab as a special case for traversing between the nominal
	     * "current" window, and other windows having callbacks.  If the
	     * nominal (control) window closes, we'll close the windows with
	     * callbacks.
	     */
	    if (dialog_state.getc_callbacks != 0 &&
		before_lookup == TAB) {
		if ((p = dialog_state.getc_redirect) != 0) {
		    p = p->next;
		} else {
		    p = dialog_state.getc_callbacks;
		}
		if ((dialog_state.getc_redirect = p) != 0) {
		    win = p->win;
		} else {
		    win = save_win;
		}
		dlg_raise_window(win);
		break;
	    }
	    /* FALLTHRU */
	default:
#ifdef NO_LEAKS
	    if (before_lookup == DLG_CTRL('P')) {
		/* for testing, ^P closes the connection */
		close(0);
		close(1);
		close(2);
		break;
	    }
#endif
	    if ((p = dialog_state.getc_redirect) != 0) {
		if (!(p->handle_getc(p, ch, *fkey, &result))) {
		    dlg_remove_callback(p);
		    dialog_state.getc_redirect = 0;
		    win = save_win;
		}
	    } else {
		done = TRUE;
	    }
	    break;
#ifdef HAVE_DLG_TRACE
	case CHR_TRACE:
	    dlg_trace_win(win);
	    break;
#endif
	}
    }
    if (literal)
	keypad(win, TRUE);
    return ch;
}

static void
finish_bg(int sig GCC_UNUSED)
{
    end_dialog();
    dlg_exit(DLG_EXIT_ERROR);
}

/*
 * If we have callbacks active, purge the list of all that are not marked
 * to keep in the background.  If any remain, run those in a background
 * process.
 */
void
dlg_killall_bg(int *retval)
{
    DIALOG_CALLBACK *cb;
    int pid;
#ifdef HAVE_TYPE_UNIONWAIT
    union wait wstatus;
#else
    int wstatus;
#endif

    if ((cb = dialog_state.getc_callbacks) != 0) {
	while (cb != 0) {
	    if (cb->keep_bg) {
		cb = cb->next;
	    } else {
		dlg_remove_callback(cb);
		cb = dialog_state.getc_callbacks;
	    }
	}
	if (dialog_state.getc_callbacks != 0) {

	    refresh();
	    fflush(stdout);
	    fflush(stderr);
	    reset_shell_mode();
	    if ((pid = fork()) != 0) {
		_exit(pid > 0 ? DLG_EXIT_OK : DLG_EXIT_ERROR);
	    } else if (pid == 0) {	/* child */
		if ((pid = fork()) != 0) {
		    /*
		     * Echo the process-id of the grandchild so a shell script
		     * can read that, and kill that process.  We'll wait around
		     * until then.  Our parent has already left, leaving us
		     * temporarily orphaned.
		     */
		    if (pid > 0) {	/* parent */
			fprintf(stderr, "%d\n", pid);
			fflush(stderr);
		    }
		    /* wait for child */
#ifdef HAVE_WAITPID
		    while (-1 == waitpid(pid, &wstatus, 0)) {
#ifdef EINTR
			if (errno == EINTR)
			    continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
			if (errno == ERESTARTSYS)
			    continue;
#endif /* ERESTARTSYS */
			break;
		    }
#else
		    while (wait(&wstatus) != pid)	/* do nothing */
			;
#endif
		    _exit(WEXITSTATUS(wstatus));
		} else if (pid == 0) {
		    if (!dialog_vars.cant_kill)
			(void) signal(SIGHUP, finish_bg);
		    (void) signal(SIGINT, finish_bg);
		    (void) signal(SIGQUIT, finish_bg);
		    (void) signal(SIGSEGV, finish_bg);
		    while (dialog_state.getc_callbacks != 0) {
			int fkey = 0;
			dlg_getc_callbacks(ERR, fkey, retval);
			napms(1000);
		    }
		}
	    }
	}
    }
}
