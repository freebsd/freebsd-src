/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Functions which manipulate the command buffer.
 * Used only by command() and related functions.
 */

#include "less.h"
#include "cmd.h"

extern int sc_width;

static char cmdbuf[CMDBUF_SIZE]; /* Buffer for holding a multi-char command */
static int cmd_col;		/* Current column of the cursor */
static int prompt_col;		/* Column of cursor just after prompt */
static char *cp;		/* Pointer into cmdbuf */
static int cmd_offset;		/* Index into cmdbuf of first displayed char */
static int literal;		/* Next input char should not be interpreted */

#if TAB_COMPLETE_FILENAME
static int cmd_complete();
/*
 * These variables are statics used by cmd_complete.
 */
static int in_completion = 0;
static char *tk_text;
static char *tk_original;
static char *tk_ipoint;
static char *tk_trial;
static struct textlist tk_tlist;
#endif

static int cmd_left();
static int cmd_right();

#if SPACES_IN_FILENAMES
public char openquote = '"';
public char closequote = '"';
#endif

#if CMD_HISTORY
/*
 * A mlist structure represents a command history.
 */
struct mlist
{
	struct mlist *next;
	struct mlist *prev;
	struct mlist *curr_mp;
	char *string;
};

/*
 * These are the various command histories that exist.
 */
struct mlist mlist_search =  
	{ &mlist_search,  &mlist_search,  &mlist_search,  NULL };
public void constant *ml_search = (void *) &mlist_search;

struct mlist mlist_examine = 
	{ &mlist_examine, &mlist_examine, &mlist_examine, NULL };
public void constant *ml_examine = (void *) &mlist_examine;

#if SHELL_ESCAPE || PIPEC
struct mlist mlist_shell =   
	{ &mlist_shell,   &mlist_shell,   &mlist_shell,   NULL };
public void constant *ml_shell = (void *) &mlist_shell;
#endif

#else /* CMD_HISTORY */

/* If CMD_HISTORY is off, these are just flags. */
public void constant *ml_search = (void *)1;
public void constant *ml_examine = (void *)2;
#if SHELL_ESCAPE || PIPEC
public void constant *ml_shell = (void *)3;
#endif

#endif /* CMD_HISTORY */

/*
 * History for the current command.
 */
static struct mlist *curr_mlist = NULL;
static int curr_cmdflags;


/*
 * Reset command buffer (to empty).
 */
	public void
cmd_reset()
{
	cp = cmdbuf;
	*cp = '\0';
	cmd_col = 0;
	cmd_offset = 0;
	literal = 0;
}

/*
 * Clear command line on display.
 */
	public void
clear_cmd()
{
	clear_bot();
	cmd_col = prompt_col = 0;
}

/*
 * Display a string, usually as a prompt for input into the command buffer.
 */
	public void
cmd_putstr(s)
	char *s;
{
	putstr(s);
	cmd_col += strlen(s);
	prompt_col += strlen(s);
}

/*
 * How many characters are in the command buffer?
 */
	public int
len_cmdbuf()
{
	return (strlen(cmdbuf));
}

/*
 * Repaint the line from cp onwards.
 * Then position the cursor just after the char old_cp (a pointer into cmdbuf).
 */
	static void
cmd_repaint(old_cp)
	char *old_cp;
{
	char *p;

	/*
	 * Repaint the line from the current position.
	 */
	clear_eol();
	for ( ;  *cp != '\0';  cp++)
	{
		p = prchar(*cp);
		if (cmd_col + strlen(p) >= sc_width)
			break;
		putstr(p);
		cmd_col += strlen(p);
	}

	/*
	 * Back up the cursor to the correct position.
	 */
	while (cp > old_cp)
		cmd_left();
}

/*
 * Put the cursor at "home" (just after the prompt),
 * and set cp to the corresponding char in cmdbuf.
 */
	static void
cmd_home()
{
	while (cmd_col > prompt_col)
	{
		putbs();
		cmd_col--;
	}

	cp = &cmdbuf[cmd_offset];
}

/*
 * Shift the cmdbuf display left a half-screen.
 */
	static void
cmd_lshift()
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
		cols += strlen(prchar(*s++));

	cmd_offset = s - cmdbuf;
	save_cp = cp;
	cmd_home();
	cmd_repaint(save_cp);
}

/*
 * Shift the cmdbuf display right a half-screen.
 */
	static void
cmd_rshift()
{
	char *s;
	char *p;
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
		p = prchar(*--s);
		cols += strlen(p);
	}

	cmd_offset = s - cmdbuf;
	save_cp = cp;
	cmd_home();
	cmd_repaint(save_cp);
}

/*
 * Move cursor right one character.
 */
	static int
cmd_right()
{
	char *p;
	
	if (*cp == '\0')
	{
		/* 
		 * Already at the end of the line.
		 */
		return (CC_OK);
	}
	p = prchar(*cp);
	if (cmd_col + strlen(p) >= sc_width)
		cmd_lshift();
	else if (cmd_col + strlen(p) == sc_width - 1 && cp[1] != '\0')
		cmd_lshift();
	cp++;
	putstr(p);
	cmd_col += strlen(p);
	return (CC_OK);
}

/*
 * Move cursor left one character.
 */
	static int
cmd_left()
{
	char *p;
	
	if (cp <= cmdbuf)
	{
		/* Already at the beginning of the line */
		return (CC_OK);
	}
	p = prchar(cp[-1]);
	if (cmd_col < prompt_col + strlen(p))
		cmd_rshift();
	cp--;
	cmd_col -= strlen(p);
	while (*p++ != '\0')
		putbs();
	return (CC_OK);
}

/*
 * Insert a char into the command buffer, at the current position.
 */
	static int
cmd_ichar(c)
	int c;
{
	char *s;
	
	if (strlen(cmdbuf) >= sizeof(cmdbuf)-2)
	{
		/*
		 * No room in the command buffer for another char.
		 */
		bell();
		return (CC_ERROR);
	}
		
	/*
	 * Insert the character into the buffer.
	 */
	for (s = &cmdbuf[strlen(cmdbuf)];  s >= cp;  s--)
		s[1] = s[0];
	*cp = c;
	/*
	 * Reprint the tail of the line from the inserted char.
	 */
	cmd_repaint(cp);
	cmd_right();
	return (CC_OK);
}

/*
 * Backspace in the command buffer.
 * Delete the char to the left of the cursor.
 */
	static int
cmd_erase()
{
	register char *s;

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
	cmd_left();
	/*
	 * Remove the char from the buffer (shift the buffer left).
	 */
	for (s = cp;  *s != '\0';  s++)
		s[0] = s[1];
	/*
	 * Repaint the buffer after the erased char.
	 */
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
	static int
cmd_delete()
{
	if (*cp == '\0')
	{
		/*
		 * At end of string; there is no char under the cursor.
		 */
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
	static int
cmd_werase()
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
	static int
cmd_wdelete()
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
	static int
cmd_kill()
{
	if (cmdbuf[0] == '\0')
	{
		/*
		 * Buffer is already empty; abort the current command.
		 */
		return (CC_QUIT);
	}
	cmd_offset = 0;
	cmd_home();
	*cp = '\0';
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
	public void
set_mlist(mlist, cmdflags)
	void *mlist;
	int cmdflags;
{
	curr_mlist = (struct mlist *) mlist;
	curr_cmdflags = cmdflags;
}

#if CMD_HISTORY
/*
 * Move up or down in the currently selected command history list.
 */
	static int
cmd_updown(action)
	int action;
{
	char *s;
	
	if (curr_mlist == NULL)
	{
		/*
		 * The current command has no history list.
		 */
		bell();
		return (CC_OK);
	}
	cmd_home();
	clear_eol();
	/*
	 * Move curr_mp to the next/prev entry.
	 */
	if (action == EC_UP)
		curr_mlist->curr_mp = curr_mlist->curr_mp->prev;
	else
		curr_mlist->curr_mp = curr_mlist->curr_mp->next;
	/*
	 * Copy the entry into cmdbuf and echo it on the screen.
	 */
	s = curr_mlist->curr_mp->string;
	if (s == NULL)
		s = "";
	for (cp = cmdbuf;  *s != '\0';  s++)
	{
		*cp = *s;
		cmd_right();
	}
	*cp = '\0';
	return (CC_OK);
}
#endif

/*
 * Add a string to a history list.
 */
	public void
cmd_addhist(mlist, cmd)
	struct mlist *mlist;
	char *cmd;
{
#if CMD_HISTORY
	struct mlist *ml;
	
	/*
	 * Don't save a trivial command.
	 */
	if (strlen(cmd) == 0)
		return;
	/*
	 * Don't save if a duplicate of a command which is already 
	 * in the history.
	 * But select the one already in the history to be current.
	 */
	for (ml = mlist->next;  ml != mlist;  ml = ml->next)
	{
		if (strcmp(ml->string, cmd) == 0)
			break;
	}
	if (ml == mlist)
	{
		/*
		 * Did not find command in history.
		 * Save the command and put it at the end of the history list.
		 */
		ml = (struct mlist *) ecalloc(1, sizeof(struct mlist));
		ml->string = save(cmd);
		ml->next = mlist;
		ml->prev = mlist->prev;
		mlist->prev->next = ml;
		mlist->prev = ml;
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
	public void
cmd_accept()
{
#if CMD_HISTORY
	/*
	 * Nothing to do if there is no currently selected history list.
	 */
	if (curr_mlist == NULL)
		return;
	cmd_addhist(curr_mlist, cmdbuf);
#endif
}

/*
 * Try to perform a line-edit function on the command buffer,
 * using a specified char as a line-editing command.
 * Returns:
 *	CC_PASS	The char does not invoke a line edit function.
 *	CC_OK	Line edit function done.
 *	CC_QUIT	The char requests the current command to be aborted.
 */
	static int
cmd_edit(c)
	int c;
{
	int action;
	int flags;

#if TAB_COMPLETE_FILENAME
#define	not_in_completion()	in_completion = 0
#else
#define	not_in_completion()
#endif
	
	/*
	 * See if the char is indeed a line-editing command.
	 */
	flags = 0;
#if CMD_HISTORY
	if (curr_mlist == NULL)
		/*
		 * No current history; don't accept history manipulation cmds.
		 */
		flags |= EC_NOHISTORY;
#endif
#if TAB_COMPLETE_FILENAME
	if (curr_mlist == ml_search)
		/*
		 * In a search command; don't accept file-completion cmds.
		 */
		flags |= EC_NOCOMPLETE;
#endif

	action = editchar(c, flags);

	switch (action)
	{
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
		literal = 1;
		return (CC_OK);
#if CMD_HISTORY
	case EC_UP:
	case EC_DOWN:
		not_in_completion();
		return (cmd_updown(action));
#endif
#if TAB_COMPLETE_FILENAME
	case EC_F_COMPLETE:
	case EC_B_COMPLETE:
	case EC_EXPAND:
		return (cmd_complete(action));
#endif
	case EC_NOACTION:
		return (CC_OK);
	default:
		not_in_completion();
		return (CC_PASS);
	}
}

#if TAB_COMPLETE_FILENAME
/*
 * Insert a string into the command buffer, at the current position.
 */
	static int
cmd_istr(str)
	char *str;
{
	char *s;
	int action;
	
	for (s = str;  *s != '\0';  s++)
	{
		action = cmd_ichar(*s);
		if (action != CC_OK)
		{
			bell();
			return (action);
		}
	}
	return (CC_OK);
}

/*
 * Find the beginning and end of the "current" word.
 * This is the word which the cursor (cp) is inside or at the end of.
 * Return pointer to the beginning of the word and put the
 * cursor at the end of the word.
 */
	static char *
delimit_word()
{
	char *word;
#if SPACES_IN_FILENAMES
	char *p;
	int quoted;
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
	} else
	{
		/*
		 * Cursor is on a space and char to the left is a space.
		 * Huh? There's no word here.
		 */
		return (NULL);
	}
	/*
	 * Search backwards for beginning of the word.
	 */
	if (cp == cmdbuf)
		return (NULL);
#if SPACES_IN_FILENAMES
	/*
	 * If we have an unbalanced quote (that is, an open quote
	 * without a corresponding close quote), we return everything
	 * from the open quote, including spaces.
	 */
	quoted = 0;
	for (p = cmdbuf;  p < cp;  p++)
	{
		if (!quoted && *p == openquote)
		{
			quoted = 1;
			word = p;
		} else if (quoted && *p == closequote)
		{
			quoted = 0;
		}
	}
	if (quoted)
		return (word);
#endif
	for (word = cp-1;  word > cmdbuf;  word--)
		if (word[-1] == ' ')
			break;
	return (word);
}

/*
 * Set things up to enter completion mode.
 * Expand the word under the cursor into a list of filenames 
 * which start with that word, and set tk_text to that list.
 */
	static void
init_compl()
{
	char *word;
	char c;
	
	/*
	 * Get rid of any previous tk_text.
	 */
	if (tk_text != NULL)
	{
		free(tk_text);
		tk_text = NULL;
	}
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
	/*
	 * Save the original (uncompleted) word
	 */
	if (tk_original != NULL)
		free(tk_original);
	tk_original = (char *) ecalloc(cp-word+1, sizeof(char));
	strncpy(tk_original, word, cp-word);
	/*
	 * Get the expanded filename.
	 * This may result in a single filename, or
	 * a blank-separated list of filenames.
	 */
	c = *cp;
	*cp = '\0';
#if SPACES_IN_FILENAMES
	if (*word == openquote)
		word++;
#endif
	tk_text = fcomplete(word);
	*cp = c;
}

/*
 * Return the next word in the current completion list.
 */
	static char *
next_compl(action, prev)
	int action;
	char *prev;
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
	static int
cmd_complete(action)
	int action;
{
	char *s;

	if (!in_completion || action == EC_EXPAND)
	{
		/*
		 * Expand the word under the cursor and 
		 * use the first word in the expansion 
		 * (or the entire expansion if we're doing EC_EXPAND).
		 */
		init_compl();
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
			in_completion = 1;
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
		in_completion = 0;
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
	in_completion = 0;
	bell();
	return (CC_OK);
}

#endif /* TAB_COMPLETE_FILENAME */

/*
 * Process a single character of a multi-character command, such as
 * a number, or the pattern of a search command.
 * Returns:
 *	CC_OK		The char was accepted.
 *	CC_QUIT		The char requests the command to be aborted.
 *	CC_ERROR	The char could not be accepted due to an error.
 */
	public int
cmd_char(c)
	int c;
{
	int action;

	if (literal)
	{
		/*
		 * Insert the char, even if it is a line-editing char.
		 */
		literal = 0;
		return (cmd_ichar(c));
	}
		
	/*
	 * See if it is a special line-editing character.
	 */
	if (in_mca())
	{
		action = cmd_edit(c);
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
	return (cmd_ichar(c));
}

/*
 * Return the number currently in the command buffer.
 */
	public int
cmd_int()
{
	return (atoi(cmdbuf));
}

/*
 * Return a pointer to the command buffer.
 */
	public char *
get_cmdbuf()
{
	return (cmdbuf);
}
