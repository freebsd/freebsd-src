/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Functions which manipulate the command buffer.
 * Used only by command() and related functions.
 */

#include "less.h"
#include "cmd.h"
#include "charset.h"
#if HAVE_STAT
#include <sys/stat.h>
#endif

extern int sc_width;
extern int utf_mode;
extern int no_hist_dups;
extern int marks_modified;
extern int no_paste;
public lbool pasting = FALSE;

static char cmdbuf[CMDBUF_SIZE]; /* Buffer for holding a multi-char command */
static int cmd_col;              /* Current column of the cursor */
static int prompt_col;           /* Column of cursor just after prompt */
static char *cp;                 /* Pointer into cmdbuf */
static int cmd_offset;           /* Index into cmdbuf of first displayed char */
static lbool literal;            /* Next input char should not be interpreted */
static size_t updown_match;      /* Prefix length in up/down movement */
static lbool have_updown_match = FALSE;

static int cmd_complete(int action);
/*
 * These variables are statics used by cmd_complete.
 */
static lbool in_completion = FALSE;
static char *tk_text;
static char *tk_original;
static constant char *tk_ipoint;
static constant char *tk_trial = NULL;
static struct textlist tk_tlist;

static int cmd_left();
static int cmd_right();

#if SPACES_IN_FILENAMES
public char openquote = '"';
public char closequote = '"';
#endif

#if CMD_HISTORY

/* History file */
#define HISTFILE_FIRST_LINE      ".less-history-file:"
#define HISTFILE_SEARCH_SECTION  ".search"
#define HISTFILE_SHELL_SECTION   ".shell"
#define HISTFILE_MARK_SECTION    ".mark"

/*
 * A mlist structure represents a command history.
 */
struct mlist
{
	struct mlist *next;
	struct mlist *prev;
	struct mlist *curr_mp;
	char *string;
	lbool modified;
};

/*
 * These are the various command histories that exist.
 */
struct mlist mlist_search =  
	{ &mlist_search,  &mlist_search,  &mlist_search,  NULL, 0 };
public void *ml_search = (void *) &mlist_search;

struct mlist mlist_examine = 
	{ &mlist_examine, &mlist_examine, &mlist_examine, NULL, 0 };
public void *ml_examine = (void *) &mlist_examine;

#if SHELL_ESCAPE || PIPEC
struct mlist mlist_shell =   
	{ &mlist_shell,   &mlist_shell,   &mlist_shell,   NULL, 0 };
public void *ml_shell = (void *) &mlist_shell;
#endif

#else /* CMD_HISTORY */

/* If CMD_HISTORY is off, these are just flags. */
public void *ml_search = (void *)1;
public void *ml_examine = (void *)2;
#if SHELL_ESCAPE || PIPEC
public void *ml_shell = (void *)3;
#endif

#endif /* CMD_HISTORY */

/*
 * History for the current command.
 */
static struct mlist *curr_mlist = NULL;
static int curr_cmdflags;

static char cmd_mbc_buf[MAX_UTF_CHAR_LEN];
static int cmd_mbc_buf_len;
static int cmd_mbc_buf_index;


/*
 * Reset command buffer (to empty).
 */
public void cmd_reset(void)
{
	cp = cmdbuf;
	*cp = '\0';
	cmd_col = 0;
	cmd_offset = 0;
	literal = FALSE;
	cmd_mbc_buf_len = 0;
	have_updown_match = FALSE;
}

/*
 * Clear command line.
 */
public void clear_cmd(void)
{
	cmd_col = prompt_col = 0;
	cmd_mbc_buf_len = 0;
	have_updown_match = FALSE;
}

/*
 * Display a string, usually as a prompt for input into the command buffer.
 */
public void cmd_putstr(constant char *s)
{
	LWCHAR prev_ch = 0;
	LWCHAR ch;
	constant char *endline = s + strlen(s);
	while (*s != '\0')
	{
		constant char *os = s;
		int width;
		ch = step_charc(&s, +1, endline);
		while (os < s)
			putchr(*os++);
		if (!utf_mode)
			width = 1;
		else if (is_composing_char(ch) || is_combining_char(prev_ch, ch))
			width = 0;
		else
			width = is_wide_char(ch) ? 2 : 1;
		cmd_col += width;
		prompt_col += width;
		prev_ch = ch;
	}
}

/*
 * How many characters are in the command buffer?
 */
public int len_cmdbuf(void)
{
	constant char *s = cmdbuf;
	constant char *endline = s + strlen(s);
	int len = 0;

	while (*s != '\0')
	{
		step_charc(&s, +1, endline);
		len++;
	}
	return (len);
}

/*
 * Is the command buffer empty?
 * It is considered nonempty if there is any text in it,
 * or if a multibyte command is being entered but not yet complete.
 */
public lbool cmdbuf_empty(void)
{
	return cp == cmdbuf && cmd_mbc_buf_len == 0;
}

/*
 * Common part of cmd_step_right() and cmd_step_left().
 * {{ Returning pwidth and bswidth separately is a historical artifact
 *    since they're always the same. Maybe clean this up someday. }}
 */
static constant char * cmd_step_common(char *p, LWCHAR ch, size_t len, int *pwidth, int *bswidth)
{
	constant char *pr;
	int width;

	if (len == 1)
	{
		pr = prchar(ch);
		width = (int) strlen(pr);
	} else
	{
		pr = prutfchar(ch);
		if (is_composing_char(ch))
			width = 0;
		else if (is_ubin_char(ch))
			width = (int) strlen(pr);
		else
		{
			LWCHAR prev_ch = step_char(&p, -1, cmdbuf);
			if (is_combining_char(prev_ch, ch))
				width = 0;
			else
				width = is_wide_char(ch) ? 2 : 1;
		}
	}
	if (pwidth != NULL)
		*pwidth = width;
	if (bswidth != NULL)
		*bswidth = width;
	return (pr);
}

/*
 * Step a pointer one character right in the command buffer.
 */
static constant char * cmd_step_right(char **pp, int *pwidth, int *bswidth)
{
	char *p = *pp;
	LWCHAR ch = step_char(pp, +1, p + strlen(p));

	return cmd_step_common(p, ch, ptr_diff(*pp, p), pwidth, bswidth);
}

/*
 * Step a pointer one character left in the command buffer.
 */
static constant char * cmd_step_left(char **pp, int *pwidth, int *bswidth)
{
	char *p = *pp;
	LWCHAR ch = step_char(pp, -1, cmdbuf);

	return cmd_step_common(*pp, ch, ptr_diff(p, *pp), pwidth, bswidth);
}

/*
 * Put the cursor at "home" (just after the prompt),
 * and set cp to the corresponding char in cmdbuf.
 */
static void cmd_home(void)
{
	while (cmd_col > prompt_col)
	{
		int width, bswidth;

		cmd_step_left(&cp, &width, &bswidth);
		while (bswidth-- > 0)
			putbs();
		cmd_col -= width;
	}

	cp = &cmdbuf[cmd_offset];
}

/*
 * Repaint the line from cp onwards.
 * Then position the cursor just after the char old_cp (a pointer into cmdbuf).
 */
public void cmd_repaint(constant char *old_cp)
{
	/*
	 * Repaint the line from the current position.
	 */
	if (old_cp == NULL)
	{
		old_cp = cp;
		cmd_home();
	}
	clear_eol();
	while (*cp != '\0')
	{
		char *np = cp;
		int width;
		constant char *pr = cmd_step_right(&np, &width, NULL);
		if (cmd_col + width >= sc_width)
			break;
		cp = np;
		putstr(pr);
		cmd_col += width;
	}
	while (*cp != '\0')
	{
		char *np = cp;
		int width;
		constant char *pr = cmd_step_right(&np, &width, NULL);
		if (width > 0)
			break;
		cp = np;
		putstr(pr);
	}

	/*
	 * Back up the cursor to the correct position.
	 */
	while (cp > old_cp)
		cmd_left();
}

/*
 * Repaint the entire line, without moving the cursor.
 */
static void cmd_repaint_curr(void)
{
	char *save_cp = cp;
	cmd_home();
	cmd_repaint(save_cp);
}

/*
 * Shift the cmdbuf display left a half-screen.
 */
static void cmd_lshift(void)
{
	char *s;
	char *save_cp;
	int cols;

	/*
	 * Start at the first displayed char, count how far to the
	 * right we'd have to move to reach the center of the screen.
	 */
	s = cmdbuf + cmd_offset;
	cols = 0;
	while (cols < (sc_width - prompt_col) / 2 && *s != '\0')
	{
		int width;
		cmd_step_right(&s, &width, NULL);
		cols += width;
	}
	while (*s != '\0')
	{
		int width;
		char *ns = s;
		cmd_step_right(&ns, &width, NULL);
		if (width > 0)
			break;
		s = ns;
	}

	cmd_offset = (int) (s - cmdbuf);
	save_cp = cp;
	cmd_home();
	cmd_repaint(save_cp);
}

/*
 * Shift the cmdbuf display right a half-screen.
 */
static void cmd_rshift(void)
{
	char *s;
	char *save_cp;
	int cols;

	/*
	 * Start at the first displayed char, count how far to the
	 * left we'd have to move to traverse a half-screen width
	 * of displayed characters.
	 */
	s = cmdbuf + cmd_offset;
	cols = 0;
	while (cols < (sc_width - prompt_col) / 2 && s > cmdbuf)
	{
		int width;
		cmd_step_left(&s, &width, NULL);
		cols += width;
	}

	cmd_offset = (int) (s - cmdbuf);
	save_cp = cp;
	cmd_home();
	cmd_repaint(save_cp);
}

/*
 * Move cursor right one character.
 */
static int cmd_right(void)
{
	constant char *pr;
	char *ncp;
	int width;
	
	if (*cp == '\0')
	{
		/* Already at the end of the line. */
		return (CC_OK);
	}
	ncp = cp;
	pr = cmd_step_right(&ncp, &width, NULL);
	if (cmd_col + width >= sc_width)
		cmd_lshift();
	else if (cmd_col + width == sc_width - 1 && cp[1] != '\0')
		cmd_lshift();
	cp = ncp;
	cmd_col += width;
	putstr(pr);
	while (*cp != '\0')
	{
		pr = cmd_step_right(&ncp, &width, NULL);
		if (width > 0)
			break;
		putstr(pr);
		cp = ncp;
	}
	return (CC_OK);
}

/*
 * Move cursor left one character.
 */
static int cmd_left(void)
{
	char *ncp;
	int width = 0;
	int bswidth = 0;

	if (cp <= cmdbuf)
	{
		/* Already at the beginning of the line */
		return (CC_OK);
	}
	ncp = cp;
	while (ncp > cmdbuf)
	{
		cmd_step_left(&ncp, &width, &bswidth);
		if (width > 0)
			break;
	}
	if (cmd_col < prompt_col + width)
		cmd_rshift();
	cp = ncp;
	cmd_col -= width;
	while (bswidth-- > 0)
		putbs();
	return (CC_OK);
}

/*
 * Insert a char into the command buffer, at the current position.
 */
static int cmd_ichar(constant char *cs, size_t clen)
{
	char *s;
	
	if (strlen(cmdbuf) + clen >= sizeof(cmdbuf)-1)
	{
		/* No room in the command buffer for another char. */
		bell();
		return (CC_ERROR);
	}
		
	/*
	 * Make room for the new character (shift the tail of the buffer right).
	 */
	for (s = &cmdbuf[strlen(cmdbuf)];  s >= cp;  s--)
		s[clen] = s[0];
	/*
	 * Insert the character into the buffer.
	 */
	for (s = cp;  s < cp + clen;  s++)
		*s = *cs++;
	/*
	 * Reprint the tail of the line from the inserted char.
	 */
	have_updown_match = FALSE;
	cmd_repaint(cp);
	cmd_right();
	return (CC_OK);
}

/*
 * Backspace in the command buffer.
 * Delete the char to the left of the cursor.
 */
static int cmd_erase(void)
{
	char *s;
	int clen;

	if (cp == cmdbuf)
	{
		/*
		 * Backspace past beginning of the buffer:
		 * this usually means abort the command.
		 */
		return (CC_QUIT);
	}
	/*
	 * Move cursor left (to the char being erased).
	 */
	s = cp;
	cmd_left();
	clen = (int) (s - cp);

	/*
	 * Remove the char from the buffer (shift the buffer left).
	 */
	for (s = cp;  ;  s++)
	{
		s[0] = s[clen];
		if (s[0] == '\0')
			break;
	}

	/*
	 * Repaint the buffer after the erased char.
	 */
	have_updown_match = FALSE;
	cmd_repaint(cp);
	
	/*
	 * We say that erasing the entire command string causes us
	 * to abort the current command, if CF_QUIT_ON_ERASE is set.
	 */
	if ((curr_cmdflags & CF_QUIT_ON_ERASE) && cp == cmdbuf && *cp == '\0')
		return (CC_QUIT);
	return (CC_OK);
}

/*
 * Delete the char under the cursor.
 */
static int cmd_delete(void)
{
	if (*cp == '\0')
	{
		/* At end of string; there is no char under the cursor. */
		return (CC_OK);
	}
	/*
	 * Move right, then use cmd_erase.
	 */
	cmd_right();
	cmd_erase();
	return (CC_OK);
}

/*
 * Delete the "word" to the left of the cursor.
 */
static int cmd_werase(void)
{
	if (cp > cmdbuf && cp[-1] == ' ')
	{
		/*
		 * If the char left of cursor is a space,
		 * erase all the spaces left of cursor (to the first non-space).
		 */
		while (cp > cmdbuf && cp[-1] == ' ')
			(void) cmd_erase();
	} else
	{
		/*
		 * If the char left of cursor is not a space,
		 * erase all the nonspaces left of cursor (the whole "word").
		 */
		while (cp > cmdbuf && cp[-1] != ' ')
			(void) cmd_erase();
	}
	return (CC_OK);
}

/*
 * Delete the "word" under the cursor.
 */
static int cmd_wdelete(void)
{
	if (*cp == ' ')
	{
		/*
		 * If the char under the cursor is a space,
		 * delete it and all the spaces right of cursor.
		 */
		while (*cp == ' ')
			(void) cmd_delete();
	} else
	{
		/*
		 * If the char under the cursor is not a space,
		 * delete it and all nonspaces right of cursor (the whole word).
		 */
		while (*cp != ' ' && *cp != '\0')
			(void) cmd_delete();
	}
	return (CC_OK);
}

/*
 * Delete all chars in the command buffer.
 */
static int cmd_kill(void)
{
	if (cmdbuf[0] == '\0')
	{
		/* Buffer is already empty; abort the current command. */
		return (CC_QUIT);
	}
	cmd_offset = 0;
	cmd_home();
	*cp = '\0';
	have_updown_match = FALSE;
	cmd_repaint(cp);

	/*
	 * We say that erasing the entire command string causes us
	 * to abort the current command, if CF_QUIT_ON_ERASE is set.
	 */
	if (curr_cmdflags & CF_QUIT_ON_ERASE)
		return (CC_QUIT);
	return (CC_OK);
}

/*
 * Select an mlist structure to be the current command history.
 */
public void set_mlist(void *mlist, int cmdflags)
{
#if CMD_HISTORY
	curr_mlist = (struct mlist *) mlist;
	curr_cmdflags = cmdflags;

	/* Make sure the next up-arrow moves to the last string in the mlist. */
	if (curr_mlist != NULL)
		curr_mlist->curr_mp = curr_mlist;
#endif
}

#if CMD_HISTORY
/*
 * Move up or down in the currently selected command history list.
 * Only consider entries whose first updown_match chars are equal to
 * cmdbuf's corresponding chars.
 */
static int cmd_updown(int action)
{
	constant char *s;
	struct mlist *ml;
	
	if (curr_mlist == NULL)
	{
		/*
		 * The current command has no history list.
		 */
		bell();
		return (CC_OK);
	}

	if (!have_updown_match)
	{
		updown_match = ptr_diff(cp, cmdbuf);
		have_updown_match = TRUE;
	}

	/*
	 * Find the next history entry which matches.
	 */
	for (ml = curr_mlist->curr_mp;;)
	{
		ml = (action == EC_UP) ? ml->prev : ml->next;
		if (ml == curr_mlist)
		{
			/*
			 * We reached the end (or beginning) of the list.
			 */
			break;
		}
		if (strncmp(cmdbuf, ml->string, updown_match) == 0)
		{
			/*
			 * This entry matches; stop here.
			 * Copy the entry into cmdbuf and echo it on the screen.
			 */
			curr_mlist->curr_mp = ml;
			s = ml->string;
			if (s == NULL)
				s = "";
			cmd_offset = 0;
			cmd_home();
			clear_eol();
			strcpy(cmdbuf, s);
			for (cp = cmdbuf;  *cp != '\0';  )
				cmd_right();
			return (CC_OK);
		}
	}
	/*
	 * We didn't find a history entry that matches.
	 */
	bell();
	return (CC_OK);
}

/*
 * Yet another lesson in the evils of global variables.
 */
public ssize_t save_updown_match(void)
{
	if (!have_updown_match)
		return (ssize_t)(-1);
	return (ssize_t) updown_match;
}

public void restore_updown_match(ssize_t udm)
{
	updown_match = udm;
	have_updown_match = (udm != (ssize_t)(-1));
}
#endif /* CMD_HISTORY */

/*
 *
 */
static void ml_link(struct mlist *mlist, struct mlist *ml)
{
	ml->next = mlist;
	ml->prev = mlist->prev;
	mlist->prev->next = ml;
	mlist->prev = ml;
}

/*
 *
 */
static void ml_unlink(struct mlist *ml)
{
	ml->prev->next = ml->next;
	ml->next->prev = ml->prev;
}

/*
 * Add a string to an mlist.
 */
public void cmd_addhist(struct mlist *mlist, constant char *cmd, lbool modified)
{
#if CMD_HISTORY
	struct mlist *ml;
	
	/*
	 * Don't save a trivial command.
	 */
	if (strlen(cmd) == 0)
		return;

	if (no_hist_dups)
	{
		struct mlist *next = NULL;
		for (ml = mlist->next;  ml->string != NULL;  ml = next)
		{
			next = ml->next;
			if (strcmp(ml->string, cmd) == 0)
			{
				ml_unlink(ml);
				free(ml->string);
				free(ml);
			}
		}
	}

	/*
	 * Save the command unless it's a duplicate of the
	 * last command in the history.
	 */
	ml = mlist->prev;
	if (ml == mlist || strcmp(ml->string, cmd) != 0)
	{
		/*
		 * Did not find command in history.
		 * Save the command and put it at the end of the history list.
		 */
		ml = (struct mlist *) ecalloc(1, sizeof(struct mlist));
		ml->string = save(cmd);
		ml->modified = modified;
		ml_link(mlist, ml);
	}
	/*
	 * Point to the cmd just after the just-accepted command.
	 * Thus, an UPARROW will always retrieve the previous command.
	 */
	mlist->curr_mp = ml->next;
#endif
}

/*
 * Accept the command in the command buffer.
 * Add it to the currently selected history list.
 */
public void cmd_accept(void)
{
#if CMD_HISTORY
	/*
	 * Nothing to do if there is no currently selected history list.
	 */
	if (curr_mlist == NULL || curr_mlist == ml_examine)
		return;
	cmd_addhist(curr_mlist, cmdbuf, TRUE);
	curr_mlist->modified = TRUE;
#endif
}

/*
 * Try to perform a line-edit function on the command buffer,
 * using a specified char as a line-editing command.
 * Returns:
 *      CC_PASS The char does not invoke a line edit function.
 *      CC_OK   Line edit function done.
 *      CC_QUIT The char requests the current command to be aborted.
 */
static int cmd_edit(char c, lbool stay_in_completion)
{
	int action;
	int flags;

#define not_in_completion()     do { if (!stay_in_completion) in_completion = FALSE; } while(0)

	/*
	 * See if the char is indeed a line-editing command.
	 */
	flags = 0;
#if CMD_HISTORY
	if (curr_mlist == NULL)
		/*
		 * No current history; don't accept history manipulation cmds.
		 */
		flags |= ECF_NOHISTORY;
#endif

	/*
	 * Don't accept completion cmds in contexts 
	 * such as search pattern, digits, etc.
	 */
	if ((curr_mlist == NULL && (curr_cmdflags & CF_OPTION))
#if TAB_COMPLETE_FILENAME
		|| curr_mlist == ml_examine || curr_mlist == ml_shell
#endif
		)
		; /* allow completion */
	else
		flags |= ECF_NOCOMPLETE;

	action = editchar(c, flags);
	if (is_ignoring_input(action))
		return (CC_OK);

	switch (action)
	{
	case A_NOACTION:
		return (CC_OK);
	case EC_START_PASTE:
		if (no_paste)
			pasting = TRUE;
		return (CC_OK);
	case EC_END_PASTE:
		stop_ignoring_input();
		return (CC_OK);
	case EC_RIGHT:
		not_in_completion();
		return (cmd_right());
	case EC_LEFT:
		not_in_completion();
		return (cmd_left());
	case EC_W_RIGHT:
		not_in_completion();
		while (*cp != '\0' && *cp != ' ')
			cmd_right();
		while (*cp == ' ')
			cmd_right();
		return (CC_OK);
	case EC_W_LEFT:
		not_in_completion();
		while (cp > cmdbuf && cp[-1] == ' ')
			cmd_left();
		while (cp > cmdbuf && cp[-1] != ' ')
			cmd_left();
		return (CC_OK);
	case EC_HOME:
		not_in_completion();
		cmd_offset = 0;
		cmd_home();
		cmd_repaint(cp);
		return (CC_OK);
	case EC_END:
		not_in_completion();
		while (*cp != '\0')
			cmd_right();
		return (CC_OK);
	case EC_INSERT:
		not_in_completion();
		return (CC_OK);
	case EC_BACKSPACE:
		not_in_completion();
		return (cmd_erase());
	case EC_LINEKILL:
		not_in_completion();
		return (cmd_kill());
	case EC_ABORT:
		not_in_completion();
		(void) cmd_kill();
		return (CC_QUIT);
	case EC_W_BACKSPACE:
		not_in_completion();
		return (cmd_werase());
	case EC_DELETE:
		not_in_completion();
		return (cmd_delete());
	case EC_W_DELETE:
		not_in_completion();
		return (cmd_wdelete());
	case EC_LITERAL:
		literal = TRUE;
		return (CC_OK);
#if CMD_HISTORY
	case EC_UP:
	case EC_DOWN:
		not_in_completion();
		return (cmd_updown(action));
#endif
	case EC_F_COMPLETE:
	case EC_B_COMPLETE:
	case EC_EXPAND:
		return (cmd_complete(action));
	default:
		not_in_completion();
		return (CC_PASS);
	}
}

/*
 * Insert a string into the command buffer, at the current position.
 */
static int cmd_istr(constant char *str)
{
	constant char *endline = str + strlen(str);
	constant char *s;
	int action;
	
	for (s = str;  *s != '\0';  )
	{
		constant char *os = s;
		step_charc(&s, +1, endline);
		action = cmd_ichar(os, ptr_diff(s, os));
		if (action != CC_OK)
			return (action);
	}
	return (CC_OK);
}

/*
 * Set tk_original to word.
 */
static void set_tk_original(constant char *word)
{
	if (tk_original != NULL)
		free(tk_original);
	tk_original = (char *) ecalloc(ptr_diff(cp,word)+1, sizeof(char));
	strncpy(tk_original, word, ptr_diff(cp,word));
}

#if TAB_COMPLETE_FILENAME
/*
 * Find the beginning and end of the "current" word.
 * This is the word which the cursor (cp) is inside or at the end of.
 * Return pointer to the beginning of the word and put the
 * cursor at the end of the word.
 */
static char * delimit_word(void)
{
	char *word;
#if SPACES_IN_FILENAMES
	char *p;
	int delim_quoted = FALSE;
	int meta_quoted = FALSE;
	constant char *esc = get_meta_escape();
	size_t esclen = strlen(esc);
#endif
	
	/*
	 * Move cursor to end of word.
	 */
	if (*cp != ' ' && *cp != '\0')
	{
		/*
		 * Cursor is on a nonspace.
		 * Move cursor right to the next space.
		 */
		while (*cp != ' ' && *cp != '\0')
			cmd_right();
	} else if (cp > cmdbuf && cp[-1] != ' ')
	{
		/*
		 * Cursor is on a space, and char to the left is a nonspace.
		 * We're already at the end of the word.
		 */
		;
#if 0
	} else
	{
		/*
		 * Cursor is on a space and char to the left is a space.
		 * Huh? There's no word here.
		 */
		return (NULL);
#endif
	}
	/*
	 * Find the beginning of the word which the cursor is in.
	 */
	if (cp == cmdbuf)
		return (NULL);
#if SPACES_IN_FILENAMES
	/*
	 * If we have an unbalanced quote (that is, an open quote
	 * without a corresponding close quote), we return everything
	 * from the open quote, including spaces.
	 */
	for (word = cmdbuf;  word < cp;  word++)
		if (*word != ' ')
			break;
	if (word >= cp)
		return (cp);
	for (p = cmdbuf;  p < cp;  p++)
	{
		if (meta_quoted)
		{
			meta_quoted = FALSE;
		} else if (esclen > 0 && p + esclen < cp &&
		           strncmp(p, esc, esclen) == 0)
		{
			meta_quoted = TRUE;
			p += esclen - 1;
		} else if (delim_quoted)
		{
			if (*p == closequote)
				delim_quoted = FALSE;
		} else /* (!delim_quoted) */
		{
			if (*p == openquote)
				delim_quoted = TRUE;
			else if (*p == ' ')
				word = p+1;
		}
	}
#endif
	return (word);
}

/*
 * Set things up to enter file completion mode.
 * Expand the word under the cursor into a list of filenames 
 * which start with that word, and set tk_text to that list.
 */
static void init_file_compl(void)
{
	char *word;
	char c;
	
	/*
	 * Find the original (uncompleted) word in the command buffer.
	 */
	word = delimit_word();
	if (word == NULL)
		return;
	/*
	 * Set the insertion point to the point in the command buffer
	 * where the original (uncompleted) word now sits.
	 */
	tk_ipoint = word;
	set_tk_original(word);
	/*
	 * Get the expanded filename.
	 * This may result in a single filename, or
	 * a blank-separated list of filenames.
	 */
	c = *cp;
	*cp = '\0';
	if (*word != openquote)
	{
		tk_text = fcomplete(word);
	} else
	{
#if MSDOS_COMPILER
		char *qword = NULL;
#else
		char *qword = shell_quote(word+1);
#endif
		if (qword == NULL)
			tk_text = fcomplete(word+1);
		else
		{
			tk_text = fcomplete(qword);
			free(qword);
		}
	}
	*cp = c;
}
#endif /* TAB_COMPLETE_FILENAME */

/*
 * Set things up to enter option completion mode.
 */
static void init_opt_compl(void)
{
	tk_ipoint = cmdbuf;
	set_tk_original(cmdbuf);
	tk_text = findopts_name(cmdbuf);
}

/*
 * Return the next word in the current completion list.
 */
static constant char * next_compl(int action, constant char *prev)
{
	switch (action)
	{
	case EC_F_COMPLETE:
		return (forw_textlist(&tk_tlist, prev));
	case EC_B_COMPLETE:
		return (back_textlist(&tk_tlist, prev));
	}
	/* Cannot happen */
	return ("?");
}

/*
 * Complete the filename before (or under) the cursor.
 * cmd_complete may be called multiple times.  The global in_completion
 * remembers whether this call is the first time (create the list),
 * or a subsequent time (step thru the list).
 */
static int cmd_complete(int action)
{
	constant char *s;

	if (!in_completion || action == EC_EXPAND)
	{
		/*
		 * Expand the word under the cursor and 
		 * use the first word in the expansion 
		 * (or the entire expansion if we're doing EC_EXPAND).
		 */
		if (tk_text != NULL)
		{
			free(tk_text);
			tk_text = NULL;
		}
		if (curr_cmdflags & CF_OPTION)
			init_opt_compl();
		else
#if TAB_COMPLETE_FILENAME
			init_file_compl();
#else
			quit(QUIT_ERROR); /* cannot happen */
#endif /* TAB_COMPLETE_FILENAME */
		if (tk_text == NULL)
		{
			bell();
			return (CC_OK);
		}
		if (action == EC_EXPAND)
		{
			/*
			 * Use the whole list.
			 */
			tk_trial = tk_text;
		} else
		{
			/*
			 * Use the first filename in the list.
			 */
			in_completion = TRUE;
			init_textlist(&tk_tlist, tk_text);
			tk_trial = next_compl(action, (char*)NULL);
		}
	} else
	{
		/*
		 * We already have a completion list.
		 * Use the next/previous filename from the list.
		 */
		tk_trial = next_compl(action, tk_trial);
	}
	
	/*
	 * Remove the original word, or the previous trial completion.
	 */
	while (cp > tk_ipoint)
		(void) cmd_erase();
	
	if (tk_trial == NULL)
	{
		/*
		 * There are no more trial completions.
		 * Insert the original (uncompleted) filename.
		 */
		in_completion = FALSE;
		if (cmd_istr(tk_original) != CC_OK)
			goto fail;
	} else
	{
		/*
		 * Insert trial completion.
		 */
		if (cmd_istr(tk_trial) != CC_OK)
			goto fail;
		/*
		 * If it is a directory, append a slash.
		 */
		if (is_dir(tk_trial))
		{
			if (cp > cmdbuf && cp[-1] == closequote)
				(void) cmd_erase();
			s = lgetenv("LESSSEPARATOR");
			if (s == NULL)
				s = PATHNAME_SEP;
			if (cmd_istr(s) != CC_OK)
				goto fail;
		}
	}
	
	return (CC_OK);
	
fail:
	in_completion = FALSE;
	bell();
	return (CC_OK);
}

/*
 * Build a UTF-8 char in cmd_mbc_buf.
 * Returns:
 *      CC_OK    Char has been stored but we don't have a complete UTF-8 sequence yet.
 *      CC_ERROR This is an invalid UTF-8 sequence.
 *      CC_PASS  There is a complete UTF-8 sequence in cmd_mbc_buf.
 *               The length of the complete sequence is returned in *plen.
 */
static int cmd_uchar(char c, size_t *plen)
{
	if (!utf_mode)
	{
		cmd_mbc_buf[0] = c;
		*plen = 1;
	} else
	{
		/* Perform strict validation in all possible cases.  */
		if (cmd_mbc_buf_len == 0)
		{
		 retry:
			cmd_mbc_buf_index = 1;
			*cmd_mbc_buf = c;
			if (IS_ASCII_OCTET(c))
				cmd_mbc_buf_len = 1;
#if MSDOS_COMPILER || OS2
			else if (c == '\340' && IS_ASCII_OCTET(peekcc()))
			{
				/* Assume a special key. */
				cmd_mbc_buf_len = 1;
			}
#endif
			else if (IS_UTF8_LEAD(c))
			{
				cmd_mbc_buf_len = utf_len(c);
				return (CC_OK);
			} else
			{
				/* UTF8_INVALID or stray UTF8_TRAIL */
				bell();
				return (CC_ERROR);
			}
		} else if (IS_UTF8_TRAIL(c))
		{
			cmd_mbc_buf[cmd_mbc_buf_index++] = c;
			if (cmd_mbc_buf_index < cmd_mbc_buf_len)
				return (CC_OK);
			if (!is_utf8_well_formed(cmd_mbc_buf, cmd_mbc_buf_index))
			{
				/* complete, but not well formed (non-shortest form), sequence */
				cmd_mbc_buf_len = 0;
				bell();
				return (CC_ERROR);
			}
		} else
		{
			/* Flush incomplete (truncated) sequence.  */
			cmd_mbc_buf_len = 0;
			bell();
			/* Handle new char.  */
			goto retry;
		}

		*plen = (size_t) cmd_mbc_buf_len; /*{{type-issue}}*/
		cmd_mbc_buf_len = 0;
	}
	return (CC_PASS);
}

/*
 * Process a single character of a multi-character command, such as
 * a number, or the pattern of a search command.
 * Returns:
 *      CC_OK           The char was accepted.
 *      CC_QUIT         The char requests the command to be aborted.
 *      CC_ERROR        The char could not be accepted due to an error.
 */
static int cmd_char2(char c, lbool stay_in_completion)
{
	size_t len;
	int action = cmd_uchar(c, &len);
	if (action != CC_PASS)
		return (action);

	if (literal)
	{
		/*
		 * Insert the char, even if it is a line-editing char.
		 */
		literal = FALSE;
		return (cmd_ichar(cmd_mbc_buf, len));
	}
		
	/*
	 * See if it is a line-editing character.
	 */
	if (in_mca() && len == 1)
	{
		action = cmd_edit(c, stay_in_completion);
		switch (action)
		{
		case CC_OK:
		case CC_QUIT:
			return (action);
		case CC_PASS:
			break;
		}
	}
	
	/*
	 * Insert the char into the command buffer.
	 */
	return (cmd_ichar(cmd_mbc_buf, len));
}

public int cmd_char(char c)
{
	return cmd_char2(c, FALSE);
}

/*
 * Copy an ASCII string to the command buffer.
 */
public int cmd_setstring(constant char *s, lbool uc)
{
	while (*s != '\0')
	{
		int action;
		char c = *s++;
		if (uc && ASCII_IS_LOWER(c))
			c = ASCII_TO_UPPER(c);
		action = cmd_char2(c, TRUE);
		if (action != CC_OK)
			return (action);
	}
	cmd_repaint_curr();
	return (CC_OK);
}

/*
 * Return the number currently in the command buffer.
 */
public LINENUM cmd_int(mutable long *frac)
{
	constant char *p;
	LINENUM n = 0;
	lbool err;

	for (p = cmdbuf;  *p >= '0' && *p <= '9';  p++)
	{
		if (ckd_mul(&n, n, 10) || ckd_add(&n, n, *p - '0'))
		{
			error("Integer is too big", NULL_PARG);
			return (0);
		}
	}
	*frac = 0;
	if (*p++ == '.')
	{
		*frac = getfraction(&p, NULL, &err);
		/* {{ do something if err is set? }} */
	}
	return (n);
}

/*
 * Return a pointer to the command buffer.
 */
public constant char * get_cmdbuf(void)
{
	if (cmd_mbc_buf_index < cmd_mbc_buf_len)
		/* Don't return buffer containing an incomplete multibyte char. */
		return (NULL);
	return (cmdbuf);
}

#if CMD_HISTORY
/*
 * Return the last (most recent) string in the current command history.
 */
public constant char * cmd_lastpattern(void)
{
	if (curr_mlist == NULL)
		return (NULL);
	return (curr_mlist->curr_mp->prev->string);
}
#endif

#if CMD_HISTORY
/*
 */
static int mlist_size(struct mlist *ml)
{
	int size = 0;
	for (ml = ml->next;  ml->string != NULL;  ml = ml->next)
		++size;
	return size;
}

/*
 * Get the name of the history file.
 */
static char * histfile_find(lbool must_exist)
{
	constant char *home = lgetenv("HOME");
	char *name = NULL;

	/* Try in $XDG_STATE_HOME, then in $HOME/.local/state, then in $XDG_DATA_HOME, then in $HOME. */
#if OS2
	if (isnullenv(home))
		home = lgetenv("INIT");
#endif
	name = dirfile(lgetenv("XDG_STATE_HOME"), &LESSHISTFILE[1], must_exist);
	if (name == NULL)
	{
		char *dir = dirfile(home, ".local/state", 1);
		if (dir != NULL)
		{
			name = dirfile(dir, &LESSHISTFILE[1], must_exist);
			free(dir);
		}
	}
	if (name == NULL)
		name = dirfile(lgetenv("XDG_DATA_HOME"), &LESSHISTFILE[1], must_exist);
	if (name == NULL)
		name = dirfile(home, LESSHISTFILE, must_exist);
	return (name);
}

static char * histfile_name(lbool must_exist)
{
	constant char *name;
	char *wname;

	/* See if filename is explicitly specified by $LESSHISTFILE. */
	name = lgetenv("LESSHISTFILE");
	if (!isnullenv(name))
	{
		if (strcmp(name, "-") == 0 || strcmp(name, "/dev/null") == 0)
			/* $LESSHISTFILE == "-" means don't use a history file. */
			return (NULL);
		return (save(name));
	}

	/* See if history file is disabled in the build. */
	if (strcmp(LESSHISTFILE, "") == 0 || strcmp(LESSHISTFILE, "-") == 0)
		return (NULL);

	wname = NULL;
	if (!must_exist)
	{
	 	/* If we're writing the file and the file already exists, use it. */
		wname = histfile_find(TRUE);
	}
	if (wname == NULL)
		wname = histfile_find(must_exist);
	return (wname);
}

/*
 * Read a .lesshst file and call a callback for each line in the file.
 */
static void read_cmdhist2(void (*action)(void*,struct mlist*,constant char*), void *uparam, int skip_search, int skip_shell)
{
	struct mlist *ml = NULL;
	char line[CMDBUF_SIZE];
	char *filename;
	FILE *f;
	int *skip = NULL;

	filename = histfile_name(TRUE);
	if (filename == NULL)
		return;
	f = fopen(filename, "r");
	free(filename);
	if (f == NULL)
		return;
	if (fgets(line, sizeof(line), f) == NULL ||
	    strncmp(line, HISTFILE_FIRST_LINE, strlen(HISTFILE_FIRST_LINE)) != 0)
	{
		fclose(f);
		return;
	}
	while (fgets(line, sizeof(line), f) != NULL)
	{
		char *p;
		for (p = line;  *p != '\0';  p++)
		{
			if (*p == '\n' || *p == '\r')
			{
				*p = '\0';
				break;
			}
		}
		if (strcmp(line, HISTFILE_SEARCH_SECTION) == 0)
		{
			ml = &mlist_search;
			skip = &skip_search;
		} else if (strcmp(line, HISTFILE_SHELL_SECTION) == 0)
		{
#if SHELL_ESCAPE || PIPEC
			ml = &mlist_shell;
			skip = &skip_shell;
#else
			ml = NULL;
			skip = NULL;
			(void) skip_shell;
#endif
		} else if (strcmp(line, HISTFILE_MARK_SECTION) == 0)
		{
			ml = NULL;
		} else if (*line == '"')
		{
			if (ml != NULL)
			{
				if (skip != NULL && *skip > 0)
					--(*skip);
				else
					(*action)(uparam, ml, line+1);
			}
		} else if (*line == 'm')
		{
			(*action)(uparam, NULL, line);
		}
	}
	fclose(f);
}

static void read_cmdhist(void (*action)(void*,struct mlist*,constant char*), void *uparam, lbool skip_search, lbool skip_shell)
{
	if (!secure_allow(SF_HISTORY))
		return;
	read_cmdhist2(action, uparam, skip_search, skip_shell);
	(*action)(uparam, NULL, NULL); /* signal end of file */
}

static void addhist_init(void *uparam, struct mlist *ml, constant char *string)
{
	(void) uparam;
	if (ml != NULL)
		cmd_addhist(ml, string, 0);
	else if (string != NULL)
		restore_mark(string);
}
#endif /* CMD_HISTORY */

/*
 * Initialize history from a .lesshist file.
 */
public void init_cmdhist(void)
{
#if CMD_HISTORY
	read_cmdhist(&addhist_init, NULL, 0, 0);
#endif /* CMD_HISTORY */
}

/*
 * Write the header for a section of the history file.
 */
#if CMD_HISTORY
static void write_mlist_header(struct mlist *ml, FILE *f)
{
	if (ml == &mlist_search)
		fprintf(f, "%s\n", HISTFILE_SEARCH_SECTION);
#if SHELL_ESCAPE || PIPEC
	else if (ml == &mlist_shell)
		fprintf(f, "%s\n", HISTFILE_SHELL_SECTION);
#endif
}

/*
 * Write all modified entries in an mlist to the history file.
 */
static void write_mlist(struct mlist *ml, FILE *f)
{
	for (ml = ml->next;  ml->string != NULL;  ml = ml->next)
	{
		if (!ml->modified)
			continue;
		fprintf(f, "\"%s\n", ml->string);
		ml->modified = FALSE;
	}
	ml->modified = FALSE; /* entire mlist is now unmodified */
}

/*
 * Make a temp name in the same directory as filename.
 */
static char * make_tempname(constant char *filename)
{
	char lastch;
	char *tempname = ecalloc(1, strlen(filename)+1);
	strcpy(tempname, filename);
	lastch = tempname[strlen(tempname)-1];
	tempname[strlen(tempname)-1] = (lastch == 'Q') ? 'Z' : 'Q';
	return tempname;
}

struct save_ctx
{
	struct mlist *mlist;
	FILE *fout;
};

/*
 * Copy entries from the saved history file to a new file.
 * At the end of each mlist, append any new entries
 * created during this session.
 */
static void copy_hist(void *uparam, struct mlist *ml, constant char *string)
{
	struct save_ctx *ctx = (struct save_ctx *) uparam;

	if (ml != NULL && ml != ctx->mlist) {
		/* We're changing mlists. */
		if (ctx->mlist)
			/* Append any new entries to the end of the current mlist. */
			write_mlist(ctx->mlist, ctx->fout);
		/* Write the header for the new mlist. */
		ctx->mlist = ml;
		write_mlist_header(ctx->mlist, ctx->fout);
	}

	if (string == NULL) /* End of file */
	{
		/* Write any sections that were not in the original file. */
		if (mlist_search.modified)
		{
			write_mlist_header(&mlist_search, ctx->fout);
			write_mlist(&mlist_search, ctx->fout);
		}
#if SHELL_ESCAPE || PIPEC
		if (mlist_shell.modified)
		{
			write_mlist_header(&mlist_shell, ctx->fout);
			write_mlist(&mlist_shell, ctx->fout);
		}
#endif
	} else if (ml != NULL)
	{
		/* Copy mlist entry. */
		fprintf(ctx->fout, "\"%s\n", string);
	}
	/* Skip marks */
}
#endif /* CMD_HISTORY */

/*
 * Make a file readable only by its owner.
 */
static void make_file_private(FILE *f)
{
#if HAVE_FCHMOD
	lbool do_chmod = TRUE;
#if HAVE_STAT
	struct stat statbuf;
	int r = fstat(fileno(f), &statbuf);
	if (r < 0 || !S_ISREG(statbuf.st_mode))
		/* Don't chmod if not a regular file. */
		do_chmod = FALSE;
#endif
	if (do_chmod)
		fchmod(fileno(f), 0600);
#endif
}

/*
 * Does the history file need to be updated?
 */
#if CMD_HISTORY
static lbool histfile_modified(void)
{
	if (mlist_search.modified)
		return TRUE;
#if SHELL_ESCAPE || PIPEC
	if (mlist_shell.modified)
		return TRUE;
#endif
	if (marks_modified)
		return TRUE;
	return FALSE;
}
#endif

/*
 * Update the .lesshst file.
 */
public void save_cmdhist(void)
{
#if CMD_HISTORY
	char *histname;
	char *tempname;
	int skip_search;
	int skip_shell;
	struct save_ctx ctx;
	constant char *s;
	FILE *fout = NULL;
	int histsize = 0;

	if (!secure_allow(SF_HISTORY) || !histfile_modified())
		return;
	histname = histfile_name(0);
	if (histname == NULL)
		return;
	tempname = make_tempname(histname);
	fout = fopen(tempname, "w");
	if (fout != NULL)
	{
		make_file_private(fout);
		s = lgetenv("LESSHISTSIZE");
		if (s != NULL)
			histsize = atoi(s);
		if (histsize <= 0)
			histsize = 100;
		skip_search = mlist_size(&mlist_search) - histsize;
#if SHELL_ESCAPE || PIPEC
		skip_shell = mlist_size(&mlist_shell) - histsize;
#else
		skip_shell = 0; /* not actually used */
#endif
		fprintf(fout, "%s\n", HISTFILE_FIRST_LINE);
		ctx.fout = fout;
		ctx.mlist = NULL;
		read_cmdhist(&copy_hist, &ctx, skip_search, skip_shell);
		save_marks(fout, HISTFILE_MARK_SECTION);
		fclose(fout);
#if MSDOS_COMPILER==WIN32C
		/*
		 * Windows rename doesn't remove an existing file,
		 * making it useless for atomic operations. Sigh.
		 */
		remove(histname);
#endif
		rename(tempname, histname);
	}
	free(tempname);
	free(histname);
#endif /* CMD_HISTORY */
}
