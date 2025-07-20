/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Routines to decode user commands.
 *
 * This is all table driven.
 * A command table is a sequence of command descriptors.
 * Each command descriptor is a sequence of bytes with the following format:
 *     <c1><c2>...<cN><0><action>
 * The characters c1,c2,...,cN are the command string; that is,
 * the characters which the user must type.
 * It is terminated by a null <0> byte.
 * The byte after the null byte is the action code associated
 * with the command string.
 * If an action byte is OR-ed with A_EXTRA, this indicates
 * that the option byte is followed by an extra string.
 *
 * There may be many command tables.
 * The first (default) table is built-in.
 * Other tables are read in from "lesskey" files.
 * All the tables are linked together and are searched in order.
 */

#include "less.h"
#include "cmd.h"
#include "lesskey.h"

extern int erase_char, erase2_char, kill_char;
extern int mousecap;
extern int sc_height;

static constant lbool allow_drag = TRUE;

#if USERFILE
/* "content" is lesskey source, never binary. */
static void add_content_table(int (*call_lesskey)(constant char *, lbool), constant char *envname, lbool sysvar);
static int add_hometable(int (*call_lesskey)(constant char *, lbool), constant char *envname, constant char *def_filename, lbool sysvar);
#endif /* USERFILE */

#define SK(k) \
	SK_SPECIAL_KEY, (k), 6, 1, 1, 1
/*
 * Command table is ordered roughly according to expected
 * frequency of use, so the common commands are near the beginning.
 */

static unsigned char cmdtable[] =
{
	'\r',0,                         A_F_LINE,
	'\n',0,                         A_F_LINE,
	'e',0,                          A_F_LINE,
	'j',0,                          A_F_LINE,
	SK(SK_DOWN_ARROW),0,            A_F_LINE,
	CONTROL('E'),0,                 A_F_LINE,
	CONTROL('N'),0,                 A_F_LINE,
	'k',0,                          A_B_LINE,
	'y',0,                          A_B_LINE,
	CONTROL('Y'),0,                 A_B_LINE,
	SK(SK_CONTROL_K),0,             A_B_LINE,
	CONTROL('P'),0,                 A_B_LINE,
	SK(SK_UP_ARROW),0,              A_B_LINE,
	'J',0,                          A_FF_LINE,
	'K',0,                          A_BF_LINE,
	'Y',0,                          A_BF_LINE,
	'd',0,                          A_F_SCROLL,
	CONTROL('D'),0,                 A_F_SCROLL,
	'u',0,                          A_B_SCROLL,
	CONTROL('U'),0,                 A_B_SCROLL,
	ESC,'[','M',0,                  A_X11MOUSE_IN,
	ESC,'[','<',0,                  A_X116MOUSE_IN,
	' ',0,                          A_F_SCREEN,
	'f',0,                          A_F_SCREEN,
	CONTROL('F'),0,                 A_F_SCREEN,
	CONTROL('V'),0,                 A_F_SCREEN,
	SK(SK_PAGE_DOWN),0,             A_F_SCREEN,
	'b',0,                          A_B_SCREEN,
	CONTROL('B'),0,                 A_B_SCREEN,
	ESC,'v',0,                      A_B_SCREEN,
	SK(SK_PAGE_UP),0,               A_B_SCREEN,
	'z',0,                          A_F_WINDOW,
	'w',0,                          A_B_WINDOW,
	ESC,' ',0,                      A_FF_SCREEN,
	ESC,'b',0,                      A_BF_SCREEN,
	ESC,'j',0,                      A_F_NEWLINE,
	ESC,'k',0,                      A_B_NEWLINE,
	'F',0,                          A_F_FOREVER,
	ESC,'F',0,                      A_F_UNTIL_HILITE,
	'R',0,                          A_FREPAINT,
	'r',0,                          A_REPAINT,
	CONTROL('R'),0,                 A_REPAINT,
	CONTROL('L'),0,                 A_REPAINT,
	ESC,'u',0,                      A_UNDO_SEARCH,
	ESC,'U',0,                      A_CLR_SEARCH,
	'g',0,                          A_GOLINE,
	SK(SK_HOME),0,                  A_GOLINE,
	'<',0,                          A_GOLINE,
	ESC,'<',0,                      A_GOLINE,
	'p',0,                          A_PERCENT,
	'%',0,                          A_PERCENT,
	ESC,'(',0,                      A_LSHIFT,
	ESC,')',0,                      A_RSHIFT,
	ESC,'{',0,                      A_LLSHIFT,
	ESC,'}',0,                      A_RRSHIFT,
	SK(SK_RIGHT_ARROW),0,           A_RSHIFT,
	SK(SK_LEFT_ARROW),0,            A_LSHIFT,
	SK(SK_CTL_RIGHT_ARROW),0,       A_RRSHIFT,
	SK(SK_CTL_LEFT_ARROW),0,        A_LLSHIFT,
	'{',0,                          A_F_BRACKET|A_EXTRA,        '{','}',0,
	'}',0,                          A_B_BRACKET|A_EXTRA,        '{','}',0,
	'(',0,                          A_F_BRACKET|A_EXTRA,        '(',')',0,
	')',0,                          A_B_BRACKET|A_EXTRA,        '(',')',0,
	'[',0,                          A_F_BRACKET|A_EXTRA,        '[',']',0,
	']',0,                          A_B_BRACKET|A_EXTRA,        '[',']',0,
	ESC,CONTROL('F'),0,             A_F_BRACKET,
	ESC,CONTROL('B'),0,             A_B_BRACKET,
	'G',0,                          A_GOEND,
	ESC,'G',0,                      A_GOEND_BUF,
	ESC,'>',0,                      A_GOEND,
	'>',0,                          A_GOEND,
	SK(SK_END),0,                   A_GOEND,
	'P',0,                          A_GOPOS,

	'0',0,                          A_DIGIT,
	'1',0,                          A_DIGIT,
	'2',0,                          A_DIGIT,
	'3',0,                          A_DIGIT,
	'4',0,                          A_DIGIT,
	'5',0,                          A_DIGIT,
	'6',0,                          A_DIGIT,
	'7',0,                          A_DIGIT,
	'8',0,                          A_DIGIT,
	'9',0,                          A_DIGIT,
	'.',0,                          A_DIGIT,

	'=',0,                          A_STAT,
	CONTROL('G'),0,                 A_STAT,
	':','f',0,                      A_STAT,
	'/',0,                          A_F_SEARCH,
	'?',0,                          A_B_SEARCH,
	ESC,'/',0,                      A_F_SEARCH|A_EXTRA,        '*',0,
	ESC,'?',0,                      A_B_SEARCH|A_EXTRA,        '*',0,
	'n',0,                          A_AGAIN_SEARCH,
	ESC,'n',0,                      A_T_AGAIN_SEARCH,
	'N',0,                          A_REVERSE_SEARCH,
	ESC,'N',0,                      A_T_REVERSE_SEARCH,
	'&',0,                          A_FILTER,
	'm',0,                          A_SETMARK,
	'M',0,                          A_SETMARKBOT,
	ESC,'m',0,                      A_CLRMARK,
	'\'',0,                         A_GOMARK,
	CONTROL('X'),CONTROL('X'),0,    A_GOMARK,
	'E',0,                          A_EXAMINE,
	':','e',0,                      A_EXAMINE,
	CONTROL('X'),CONTROL('V'),0,    A_EXAMINE,
	':','n',0,                      A_NEXT_FILE,
	':','p',0,                      A_PREV_FILE,
	CONTROL('O'),CONTROL('N'),0,    A_OSC8_F_SEARCH,
	CONTROL('O'),'n',0,             A_OSC8_F_SEARCH,
	CONTROL('O'),CONTROL('P'),0,    A_OSC8_B_SEARCH,
	CONTROL('O'),'p',0,             A_OSC8_B_SEARCH,
	CONTROL('O'),CONTROL('O'),0,    A_OSC8_OPEN,
	CONTROL('O'),'o',0,             A_OSC8_OPEN,
	CONTROL('O'),CONTROL('L'),0,    A_OSC8_JUMP,
	CONTROL('O'),'l',0,             A_OSC8_JUMP,
	't',0,                          A_NEXT_TAG,
	'T',0,                          A_PREV_TAG,
	':','x',0,                      A_INDEX_FILE,
	':','d',0,                      A_REMOVE_FILE,
	'-',0,                          A_OPT_TOGGLE,
	':','t',0,                      A_OPT_TOGGLE|A_EXTRA,        't',0,
	's',0,                          A_OPT_TOGGLE|A_EXTRA,        'o',0,
	'_',0,                          A_DISP_OPTION,
	'|',0,                          A_PIPE,
	'v',0,                          A_VISUAL,
	'!',0,                          A_SHELL,
	'#',0,                          A_PSHELL,
	'+',0,                          A_FIRSTCMD,
	ESC,'[','2','0','0','~',0,      A_START_PASTE,
	ESC,'[','2','0','1','~',0,      A_END_PASTE,

	'H',0,                          A_HELP,
	'h',0,                          A_HELP,
	SK(SK_F1),0,                    A_HELP,
	'V',0,                          A_VERSION,
	'q',0,                          A_QUIT,
	'Q',0,                          A_QUIT,
	':','q',0,                      A_QUIT,
	':','Q',0,                      A_QUIT,
	'Z','Z',0,                      A_QUIT
};

static unsigned char edittable[] =
{
	'\t',0,                         EC_F_COMPLETE,  /* TAB */
	'\17',0,                        EC_B_COMPLETE,  /* BACKTAB */
	SK(SK_BACKTAB),0,               EC_B_COMPLETE,  /* BACKTAB */
	ESC,'\t',0,                     EC_B_COMPLETE,  /* ESC TAB */
	CONTROL('L'),0,                 EC_EXPAND,      /* CTRL-L */
	CONTROL('V'),0,                 EC_LITERAL,     /* BACKSLASH */
	CONTROL('A'),0,                 EC_LITERAL,     /* BACKSLASH */
	ESC,'l',0,                      EC_RIGHT,       /* ESC l */
	SK(SK_RIGHT_ARROW),0,           EC_RIGHT,       /* RIGHTARROW */
	ESC,'h',0,                      EC_LEFT,        /* ESC h */
	SK(SK_LEFT_ARROW),0,            EC_LEFT,        /* LEFTARROW */
	ESC,'b',0,                      EC_W_LEFT,      /* ESC b */
	ESC,SK(SK_LEFT_ARROW),0,        EC_W_LEFT,      /* ESC LEFTARROW */
	SK(SK_CTL_LEFT_ARROW),0,        EC_W_LEFT,      /* CTRL-LEFTARROW */
	ESC,'w',0,                      EC_W_RIGHT,     /* ESC w */
	ESC,SK(SK_RIGHT_ARROW),0,       EC_W_RIGHT,     /* ESC RIGHTARROW */
	SK(SK_CTL_RIGHT_ARROW),0,       EC_W_RIGHT,     /* CTRL-RIGHTARROW */
	ESC,'i',0,                      EC_INSERT,      /* ESC i */
	SK(SK_INSERT),0,                EC_INSERT,      /* INSERT */
	ESC,'x',0,                      EC_DELETE,      /* ESC x */
	SK(SK_DELETE),0,                EC_DELETE,      /* DELETE */
	ESC,'X',0,                      EC_W_DELETE,    /* ESC X */
	ESC,SK(SK_DELETE),0,            EC_W_DELETE,    /* ESC DELETE */
	SK(SK_CTL_DELETE),0,            EC_W_DELETE,    /* CTRL-DELETE */
	SK(SK_CTL_BACKSPACE),0,         EC_W_BACKSPACE, /* CTRL-BACKSPACE */
	ESC,SK(SK_BACKSPACE),0,         EC_W_BACKSPACE, /* ESC BACKSPACE */
	ESC,'0',0,                      EC_HOME,        /* ESC 0 */
	SK(SK_HOME),0,                  EC_HOME,        /* HOME */
	ESC,'$',0,                      EC_END,         /* ESC $ */
	SK(SK_END),0,                   EC_END,         /* END */
	ESC,'k',0,                      EC_UP,          /* ESC k */
	SK(SK_UP_ARROW),0,              EC_UP,          /* UPARROW */
	ESC,'j',0,                      EC_DOWN,        /* ESC j */
	SK(SK_DOWN_ARROW),0,            EC_DOWN,        /* DOWNARROW */
	CONTROL('G'),0,                 EC_ABORT,       /* CTRL-G */
	ESC,'[','M',0,                  EC_X11MOUSE,    /* X11 mouse report */
	ESC,'[','<',0,                  EC_X116MOUSE,   /* X11 1006 mouse report */
	ESC,'[','2','0','0','~',0,      A_START_PASTE,  /* open paste bracket */
	ESC,'[','2','0','1','~',0,      A_END_PASTE,    /* close paste bracket */
};

static unsigned char dflt_vartable[] =
{
	'L','E','S','S','_','O','S','C','8','_','m','a','n', 0, EV_OK|A_EXTRA,
		/* echo '%o' | sed -e "s,^man\:\\([^(]*\\)( *\\([^)]*\\)\.*,-man '\\2' '\\1'," -e"t X" -e"s,\.*,-echo Invalid man link," -e"\: X" */
		'e','c','h','o',' ','\'','%','o','\'',' ','|',' ','s','e','d',' ','-','e',' ','"','s',',','^','m','a','n','\\',':','\\','\\','(','[','^','(',']','*','\\','\\',')','(',' ','*','\\','\\','(','[','^',')',']','*','\\','\\',')','\\','.','*',',','-','m','a','n',' ','\'','\\','\\','2','\'',' ','\'','\\','\\','1','\'',',','"',' ','-','e','"','t',' ','X','"',' ','-','e','"','s',',','\\','.','*',',','-','e','c','h','o',' ','I','n','v','a','l','i','d',' ','m','a','n',' ','l','i','n','k',',','"',' ','-','e','"','\\',':',' ','X','"',
		0,

	'L','E','S','S','_','O','S','C','8','_','f','i','l','e', 0, EV_OK|A_EXTRA,
		/* eval `echo '%o' | sed -e "s,^file://\\([^/]*\\)\\(.*\\),_H=\\1;_P=\\2;_E=0," -e"t X" -e"s,.*,_E=1," -e": X"`; if [ "$_E" = 1 ]; then echo -echo Invalid file link; elif [ -z "$_H" -o "$_H" = localhost -o "$_H" = $HOSTNAME ]; then echo ":e $_P"; else echo -echo Cannot open remote file on "$_H"; fi */
		'e','v','a','l',' ','`','e','c','h','o',' ','\'','%','o','\'',' ','|',' ','s','e','d',' ','-','e',' ','"','s',',','^','f','i','l','e','\\',':','/','/','\\','\\','(','[','^','/',']','*','\\','\\',')','\\','\\','(','\\','.','*','\\','\\',')',',','_','H','=','\\','\\','1',';','_','P','=','\\','\\','2',';','_','E','=','0',',','"',' ','-','e','"','t',' ','X','"',' ','-','e','"','s',',','\\','.','*',',','_','E','=','1',',','"',' ','-','e','"','\\',':',' ','X','"','`',';',' ','i','f',' ','[',' ','"','$','_','E','"',' ','=',' ','1',' ',']',';',' ','t','h','e','n',' ','e','c','h','o',' ','-','e','c','h','o',' ','I','n','v','a','l','i','d',' ','f','i','l','e',' ','l','i','n','k',';',' ','e','l','i','f',' ','[',' ','-','z',' ','"','$','_','H','"',' ','-','o',' ','"','$','_','H','"',' ','=',' ','l','o','c','a','l','h','o','s','t',' ','-','o',' ','"','$','_','H','"',' ','=',' ','$','H','O','S','T','N','A','M','E',' ',']',';',' ','t','h','e','n',' ','e','c','h','o',' ','"','\\',':','e',' ','$','_','P','"',';',' ','e','l','s','e',' ','e','c','h','o',' ','-','e','c','h','o',' ','C','a','n','n','o','t',' ','o','p','e','n',' ','r','e','m','o','t','e',' ','f','i','l','e',' ','o','n',' ','"','$','_','H','"',';',' ','f','i',
		0,
};

/*
 * Structure to support a list of command tables.
 */
struct tablelist
{
	struct tablelist *t_next;
	unsigned char *t_start;
	unsigned char *t_end;
};

/*
 * List of command tables and list of line-edit tables.
 */
static struct tablelist *list_fcmd_tables = NULL;
static struct tablelist *list_ecmd_tables = NULL;
static struct tablelist *list_var_tables = NULL;
static struct tablelist *list_sysvar_tables = NULL;


/*
 * Expand special key abbreviations in a command table.
 */
static void expand_special_keys(unsigned char *table, size_t len)
{
	unsigned char *fm;
	unsigned char *to;
	int a;
	constant char *repl;
	size_t klen;

	for (fm = table;  fm < table + len; )
	{
		/*
		 * Rewrite each command in the table with any
		 * special key abbreviations expanded.
		 */
		for (to = fm;  *fm != '\0'; )
		{
			if (*fm != SK_SPECIAL_KEY)
			{
				*to++ = *fm++;
				continue;
			}
			/*
			 * After SK_SPECIAL_KEY, next byte is the type
			 * of special key (one of the SK_* constants),
			 * and the byte after that is the number of bytes,
			 * N, reserved by the abbreviation (including the
			 * SK_SPECIAL_KEY and key type bytes).
			 * Replace all N bytes with the actual bytes
			 * output by the special key on this terminal.
			 */
			repl = special_key_str(fm[1]);
			klen = fm[2] & 0377;
			fm += klen;
			if (repl == NULL || strlen(repl) > klen)
				repl = "\377";
			while (*repl != '\0')
				*to++ = (unsigned char) *repl++; /*{{type-issue}}*/
		}
		*to++ = '\0';
		/*
		 * Fill any unused bytes between end of command and 
		 * the action byte with A_SKIP.
		 */
		while (to <= fm)
			*to++ = A_SKIP;
		fm++;
		a = *fm++ & 0377;
		if (a & A_EXTRA)
		{
			while (*fm++ != '\0')
				continue;
		}
	}
}

/*
 * Expand special key abbreviations in a list of command tables.
 */
static void expand_cmd_table(struct tablelist *tlist)
{
	struct tablelist *t;
	for (t = tlist;  t != NULL;  t = t->t_next)
	{
		expand_special_keys(t->t_start, ptr_diff(t->t_end, t->t_start));
	}
}

/*
 * Expand special key abbreviations in all command tables.
 */
public void expand_cmd_tables(void)
{
	expand_cmd_table(list_fcmd_tables);
	expand_cmd_table(list_ecmd_tables);
	expand_cmd_table(list_var_tables);
	expand_cmd_table(list_sysvar_tables);
}

/*
 * Initialize the command lists.
 */
public void init_cmds(void)
{
	/*
	 * Add the default command tables.
	 */
	add_fcmd_table(cmdtable, sizeof(cmdtable));
	add_ecmd_table(edittable, sizeof(edittable));
	add_sysvar_table(dflt_vartable, sizeof(dflt_vartable));
#if USERFILE
#ifdef BINDIR /* For backwards compatibility */
	/* Try to add tables in the OLD system lesskey file. */
	add_hometable(lesskey, NULL, BINDIR "/.sysless", TRUE);
#endif
	/*
	 * Try to load lesskey source file or binary file.
	 * If the source file succeeds, don't load binary file. 
	 * The binary file is likely to have been generated from 
	 * a (possibly out of date) copy of the src file, 
	 * so loading it is at best redundant.
	 */
	/*
	 * Try to add tables in system lesskey src file.
	 */
#if HAVE_LESSKEYSRC 
	if (add_hometable(lesskey_src, "LESSKEYIN_SYSTEM", LESSKEYINFILE_SYS, TRUE) != 0)
#endif
	{
		/*
		 * Try to add the tables in the system lesskey binary file.
		 */
		add_hometable(lesskey, "LESSKEY_SYSTEM", LESSKEYFILE_SYS, TRUE);
	}
	/*
	 * Try to add tables in the lesskey src file "$HOME/.lesskey".
	 */
#if HAVE_LESSKEYSRC 
	if (add_hometable(lesskey_src, "LESSKEYIN", DEF_LESSKEYINFILE, FALSE) != 0)
#endif
	{
		/*
		 * Try to add the tables in the standard lesskey binary file "$HOME/.less".
		 */
		add_hometable(lesskey, "LESSKEY", LESSKEYFILE, FALSE);
	}
	
	add_content_table(lesskey_content, "LESSKEY_CONTENT_SYSTEM", TRUE);
	add_content_table(lesskey_content, "LESSKEY_CONTENT", FALSE);
#endif /* USERFILE */
}

/*
 * Add a command table.
 */
static int add_cmd_table(struct tablelist **tlist, unsigned char *buf, size_t len)
{
	struct tablelist *t;

	if (len == 0)
		return (0);
	/*
	 * Allocate a tablelist structure, initialize it, 
	 * and link it into the list of tables.
	 */
	if ((t = (struct tablelist *) 
			calloc(1, sizeof(struct tablelist))) == NULL)
	{
		return (-1);
	}
	t->t_start = buf;
	t->t_end = buf + len;
	t->t_next = NULL;
	if (*tlist == NULL)
		*tlist = t;
	else
	{
		struct tablelist *e;
		for (e = *tlist;  e->t_next != NULL;  e = e->t_next)
			continue;
		e->t_next = t;
	}
	return (0);
}

/*
 * Remove the last command table in a list.
 */
static void pop_cmd_table(struct tablelist **tlist)
{
	struct tablelist *t;
	if (*tlist == NULL)
		return;
	if ((*tlist)->t_next == NULL)
	{
		t = *tlist;
		*tlist = NULL;
	} else
	{
		struct tablelist *e;
		for (e = *tlist;  e->t_next->t_next != NULL;  e = e->t_next)
			continue;
		t = e->t_next;
		e->t_next = NULL;
	}
	free(t);
}

/*
 * Add a command table.
 */
public void add_fcmd_table(unsigned char *buf, size_t len)
{
	if (add_cmd_table(&list_fcmd_tables, buf, len) < 0)
		error("Warning: some commands disabled", NULL_PARG);
}

/*
 * Add an editing command table.
 */
public void add_ecmd_table(unsigned char *buf, size_t len)
{
	if (add_cmd_table(&list_ecmd_tables, buf, len) < 0)
		error("Warning: some edit commands disabled", NULL_PARG);
}

/*
 * Add an environment variable table.
 */
static void add_var_table(struct tablelist **tlist, unsigned char *buf, size_t len)
{
	struct xbuffer xbuf;

	xbuf_init(&xbuf);
	expand_evars((char*)buf, len, &xbuf); /*{{unsigned-issue}}*/
	/* {{ We leak the table in buf. expand_evars scribbled in it so it's useless anyway. }} */
	if (add_cmd_table(tlist, xbuf.data, xbuf.end) < 0)
		error("Warning: environment variables from lesskey file unavailable", NULL_PARG);
}

public void add_uvar_table(unsigned char *buf, size_t len)
{
	add_var_table(&list_var_tables, buf, len);
}

public void add_sysvar_table(unsigned char *buf, size_t len)
{
	add_var_table(&list_sysvar_tables, buf, len);
}

/*
 * Return action for a mouse wheel down event.
 */
static int mouse_wheel_down(void)
{
	return ((mousecap == OPT_ONPLUS) ? A_B_MOUSE : A_F_MOUSE);
}

/*
 * Return action for a mouse wheel up event.
 */
static int mouse_wheel_up(void)
{
	return ((mousecap == OPT_ONPLUS) ? A_F_MOUSE : A_B_MOUSE);
}

/*
 * Return action for the left mouse button trigger.
 */
static int mouse_button_left(int x, int y, lbool down, lbool drag)
{
	static int last_drag_y = -1;
	static int last_click_y = -1;

	if (down && !drag)
	{
		last_drag_y = last_click_y = y;
	}
	if (allow_drag && drag && last_drag_y >= 0)
	{
		/* Drag text up/down */
		if (y > last_drag_y)
		{
			cmd_exec();
			backward(y - last_drag_y, FALSE, FALSE, FALSE);
			last_drag_y = y;
		} else if (y < last_drag_y)
		{
			cmd_exec();
			forward(last_drag_y - y, FALSE, FALSE, FALSE);
			last_drag_y = y;
		}
	} else if (!down)
	{
#if OSC8_LINK
		if (osc8_click(y, x))
			return (A_NOACTION);
#else
		(void) x;
#endif /* OSC8_LINK */
		if (y < sc_height-1 && y == last_click_y)
		{
			setmark('#', y);
			screen_trashed();
		}
	}
	return (A_NOACTION);
}

/*
 * Return action for the right mouse button trigger.
 */
static int mouse_button_right(int x, int y, lbool down, lbool drag)
{
	(void) x; (void) drag;
	/*
	 * {{ unlike mouse_button_left, we could return an action,
	 *    but keep it near mouse_button_left for readability. }}
	 */
	if (!down && y < sc_height-1)
	{
		gomark('#');
		screen_trashed();
	}
	return (A_NOACTION);
}

/*
 * Read a decimal integer. Return the integer and set *pterm to the terminating char.
 */
static int getcc_int(char *pterm)
{
	int num = 0;
	int digits = 0;
	for (;;)
	{
		char ch = getcc();
		if (ch < '0' || ch > '9')
		{
			if (pterm != NULL) *pterm = ch;
			if (digits == 0)
				return (-1);
			return (num);
		}
		if (ckd_mul(&num, num, 10) || ckd_add(&num, num, ch - '0'))
			return -1;
		++digits;
	}
}

static int x11mouse_button(int btn, int x, int y, lbool down, lbool drag)
{
	switch (btn) {
	case X11MOUSE_BUTTON1:
		return mouse_button_left(x, y, down, drag);
	/* is BUTTON2 the rightmost with 2-buttons mouse? */
	case X11MOUSE_BUTTON2:
	case X11MOUSE_BUTTON3:
		return mouse_button_right(x, y, down, drag);
	}
	return (A_NOACTION);
}

/*
 * Read suffix of mouse input and return the action to take.
 * The prefix ("\e[M") has already been read.
 */
static int x11mouse_action(lbool skip)
{
	static int prev_b = X11MOUSE_BUTTON_REL;
	int x, y;
	int b = getcc() - X11MOUSE_OFFSET;
	lbool drag = ((b & X11MOUSE_DRAG) != 0);
	b &= ~X11MOUSE_DRAG;
	x = getcc() - X11MOUSE_OFFSET-1;
	y = getcc() - X11MOUSE_OFFSET-1;
	if (skip)
		return (A_NOACTION);
	switch (b) {
	case X11MOUSE_WHEEL_DOWN:
		return mouse_wheel_down();
	case X11MOUSE_WHEEL_UP:
		return mouse_wheel_up();
	case X11MOUSE_BUTTON1:
	case X11MOUSE_BUTTON2:
	case X11MOUSE_BUTTON3:
		prev_b = b;
		return x11mouse_button(b, x, y, TRUE, drag);
	case X11MOUSE_BUTTON_REL: /* button up */
		return x11mouse_button(prev_b, x, y, FALSE, drag);
	}
	return (A_NOACTION);
}

/*
 * Read suffix of mouse input and return the action to take.
 * The prefix ("\e[<") has already been read.
 */
static int x116mouse_action(lbool skip)
{
	char ch;
	int x, y;
	int b = getcc_int(&ch);
	lbool drag = ((b & X11MOUSE_DRAG) != 0);
	b &= ~X11MOUSE_DRAG;
	if (b < 0 || ch != ';') return (A_NOACTION);
	x = getcc_int(&ch) - 1;
	if (x < 0 || ch != ';') return (A_NOACTION);
	y = getcc_int(&ch) - 1;
	if (y < 0) return (A_NOACTION);
	if (skip)
		return (A_NOACTION);
	switch (b) {
	case X11MOUSE_WHEEL_DOWN:
		return mouse_wheel_down();
	case X11MOUSE_WHEEL_UP:
		return mouse_wheel_up();
	case X11MOUSE_BUTTON1:
	case X11MOUSE_BUTTON2:
	case X11MOUSE_BUTTON3: {
		lbool down = (ch == 'M');
		lbool up = (ch == 'm');
		if (up || down)
			return x11mouse_button(b, x, y, down, drag);
		break; }
	}
	return (A_NOACTION);
}

/*
 * Return the largest N such that the first N chars of goal
 * are equal to the last N chars of str.
 */
static size_t cmd_match(constant char *goal, constant char *str)
{
	size_t slen = strlen(str);
	size_t len;
	for (len = slen;  len > 0;  len--)
		if (strncmp(str + slen - len, goal, len) == 0)
			break;
	return len;
}

/*
 * Return pointer to next command table entry.
 * Also return the action and the extra string from the entry.
 */
static constant unsigned char * cmd_next_entry(constant unsigned char *entry, mutable int *action, mutable constant unsigned char **extra, mutable size_t *cmdlen)
{
	int a;
	constant unsigned char *oentry = entry;
	while (*entry != '\0') /* skip cmd */
		++entry;
	if (cmdlen != NULL)
		*cmdlen = ptr_diff(entry, oentry);
	do 
		a = *++entry; /* get action */
	while (a == A_SKIP);
	++entry; /* skip action */
	if (extra != NULL)
		*extra = (a & A_EXTRA) ? entry : NULL;
	if (a & A_EXTRA)
	{
		while (*entry++ != '\0') /* skip extra string */
			continue;
		a &= ~A_EXTRA;
	}
	if (action != NULL)
		*action = a;
	return entry;
}

/*
 * Search a single command table for the command string in cmd.
 */
static int cmd_search(constant char *cmd, constant unsigned char *table, constant unsigned char *endtable, constant unsigned char **extra, size_t *mlen)
{
	int action = A_INVALID;
	size_t match_len = 0;
	if (extra != NULL)
		*extra = NULL;
	while (table < endtable)
	{
		int taction;
		const unsigned char *textra;
		size_t cmdlen;
		size_t match = cmd_match((constant char *) table, cmd);
		table = cmd_next_entry(table, &taction, &textra, &cmdlen);
		if (taction == A_END_LIST)
			return (-action);
		if (match >= match_len)
		{
			if (match == cmdlen) /* (last chars of) cmd matches this table entry */
			{
				action = taction;
				*extra = textra;
			} else if (match > 0 && action == A_INVALID) /* cmd is a prefix of this table entry */
			{
				action = A_PREFIX;
			}
			match_len = match;
		}
	}
	if (mlen != NULL)
		*mlen = match_len;
	return (action);
}

/*
 * Decode a command character and return the associated action.
 * The "extra" string, if any, is returned in sp.
 */
static int cmd_decode(struct tablelist *tlist, constant char *cmd, constant char **sp)
{
	struct tablelist *t;
	int action = A_INVALID;
	size_t match_len = 0;

	/*
	 * Search for the cmd thru all the command tables.
	 * If we find it more than once, take the last one.
	 */
	*sp = NULL;
	for (t = tlist;  t != NULL;  t = t->t_next)
	{
		constant unsigned char *tsp;
		size_t mlen;
		int taction = cmd_search(cmd, t->t_start, t->t_end, &tsp, &mlen);
		if (mlen >= match_len)
		{
			match_len = mlen;
			if (taction == A_UINVALID)
				taction = A_INVALID;
			if (taction != A_INVALID)
			{
				*sp = (constant char *) tsp;
				if (taction < 0)
				{
					action = -taction;
					break;
				}
				action = taction;
			}
		}
	}
	if (action == A_X11MOUSE_IN)
		action = x11mouse_action(FALSE);
	else if (action == A_X116MOUSE_IN)
		action = x116mouse_action(FALSE);
	return (action);
}

/*
 * Decode a command from the cmdtables list.
 */
public int fcmd_decode(constant char *cmd, constant char **sp)
{
	return (cmd_decode(list_fcmd_tables, cmd, sp));
}

/*
 * Decode a command from the edittables list.
 */
public int ecmd_decode(constant char *cmd, constant char **sp)
{
	return (cmd_decode(list_ecmd_tables, cmd, sp));
}


/*
 * Get the value of an environment variable.
 * Looks first in the lesskey file, then in the real environment.
 */
public constant char * lgetenv(constant char *var)
{
	int a;
	constant char *s;

	a = cmd_decode(list_var_tables, var, &s);
	if (a == EV_OK)
		return (s);
	s = getenv(var);
	if (s != NULL && *s != '\0')
		return (s);
	a = cmd_decode(list_sysvar_tables, var, &s);
	if (a == EV_OK)
		return (s);
	return (NULL);
}

/*
 * Like lgetenv, but also uses a buffer partially filled with an env table.
 */
public constant char * lgetenv_ext(constant char *var, unsigned char *env_buf, size_t env_buf_len)
{
	constant char *r;
	size_t e;
	size_t env_end = 0;

	for (e = 0;;)
	{
		for (; e < env_buf_len; e++)
			if (env_buf[e] == '\0')
				break;
		if (e >= env_buf_len) break;
		if (env_buf[++e] & A_EXTRA)
		{
			for (e = e+1; e < env_buf_len; e++)
				if (env_buf[e] == '\0')
					break;
		}
		e++;
		if (e >= env_buf_len) break;
		env_end = e;
	}
	/* Temporarily add env_buf to var_tables, do the lookup, then remove it. */
	add_uvar_table(env_buf, env_end);
	r = lgetenv(var);
	pop_cmd_table(&list_var_tables);
	return r;
}

/*
 * Is a string null or empty? 
 */
public lbool isnullenv(constant char *s)
{
	return (s == NULL || *s == '\0');
}

#if USERFILE
/*
 * Get an "integer" from a lesskey file.
 * Integers are stored in a funny format: 
 * two bytes, low order first, in radix KRADIX.
 */
static size_t gint(unsigned char **sp)
{
	size_t n;

	n = *(*sp)++;
	n += *(*sp)++ * KRADIX;
	return (n);
}

/*
 * Process an old (pre-v241) lesskey file.
 */
static int old_lesskey(unsigned char *buf, size_t len)
{
	/*
	 * Old-style lesskey file.
	 * The file must end with either 
	 *     ...,cmd,0,action
	 * or  ...,cmd,0,action|A_EXTRA,string,0
	 * So the last byte or the second to last byte must be zero.
	 */
	if (buf[len-1] != '\0' && buf[len-2] != '\0')
		return (-1);
	add_fcmd_table(buf, len);
	return (0);
}

/* 
 * Process a new (post-v241) lesskey file.
 */
static int new_lesskey(unsigned char *buf, size_t len, lbool sysvar)
{
	unsigned char *p;
	unsigned char *end;
	int c;
	size_t n;

	/*
	 * New-style lesskey file.
	 * Extract the pieces.
	 */
	if (buf[len-3] != C0_END_LESSKEY_MAGIC ||
	    buf[len-2] != C1_END_LESSKEY_MAGIC ||
	    buf[len-1] != C2_END_LESSKEY_MAGIC)
		return (-1);
	p = buf + 4;
	end = buf + len;
	for (;;)
	{
		c = *p++;
		switch (c)
		{
		case CMD_SECTION:
			n = gint(&p);
			if (p+n >= end)
				return (-1);
			add_fcmd_table(p, n);
			p += n;
			break;
		case EDIT_SECTION:
			n = gint(&p);
			if (p+n >= end)
				return (-1);
			add_ecmd_table(p, n);
			p += n;
			break;
		case VAR_SECTION:
			n = gint(&p);
			if (p+n >= end)
				return (-1);
			if (sysvar)
				add_sysvar_table(p, n);
			else
				add_uvar_table(p, n);
			p += n;
			break;
		case END_SECTION:
			return (0);
		default:
			/*
			 * Unrecognized section type.
			 */
			return (-1);
		}
	}
}

/*
 * Set up a user command table, based on a "lesskey" file.
 */
public int lesskey(constant char *filename, lbool sysvar)
{
	unsigned char *buf;
	POSITION len;
	ssize_t n;
	int f;

	if (!secure_allow(SF_LESSKEY))
		return (1);
	/*
	 * Try to open the lesskey file.
	 */
	f = open(filename, OPEN_READ);
	if (f < 0)
		return (1);

	/*
	 * Read the file into a buffer.
	 * We first figure out the size of the file and allocate space for it.
	 * {{ Minimal error checking is done here.
	 *    A garbage .less file will produce strange results.
	 *    To avoid a large amount of error checking code here, we
	 *    rely on the lesskey program to generate a good .less file. }}
	 */
	len = filesize(f);
	if (len == NULL_POSITION || len < 3)
	{
		/*
		 * Bad file (valid file must have at least 3 chars).
		 */
		close(f);
		return (-1);
	}
	if ((buf = (unsigned char *) calloc((size_t)len, sizeof(char))) == NULL)
	{
		close(f);
		return (-1);
	}
	if (less_lseek(f, (less_off_t)0, SEEK_SET) == BAD_LSEEK)
	{
		free(buf);
		close(f);
		return (-1);
	}
	n = read(f, buf, (size_t) len);
	close(f);
	if (n != len)
	{
		free(buf);
		return (-1);
	}

	/*
	 * Figure out if this is an old-style (before version 241)
	 * or new-style lesskey file format.
	 */
	if (len < 4 || 
	    buf[0] != C0_LESSKEY_MAGIC || buf[1] != C1_LESSKEY_MAGIC ||
	    buf[2] != C2_LESSKEY_MAGIC || buf[3] != C3_LESSKEY_MAGIC)
		return (old_lesskey(buf, (size_t) len));
	return (new_lesskey(buf, (size_t) len, sysvar));
}

#if HAVE_LESSKEYSRC 
static int lesskey_text(constant char *filename, lbool sysvar, lbool content)
{
	int r;
	static struct lesskey_tables tables;

	if (!secure_allow(SF_LESSKEY))
		return (1);
	r = content ? parse_lesskey_content(filename, &tables) : parse_lesskey(filename, &tables);
	if (r != 0)
		return (r);
	add_fcmd_table(tables.cmdtable.buf.data, tables.cmdtable.buf.end);
	add_ecmd_table(tables.edittable.buf.data, tables.edittable.buf.end);
	if (sysvar)
		add_sysvar_table(tables.vartable.buf.data, tables.vartable.buf.end);
	else
		add_uvar_table(tables.vartable.buf.data, tables.vartable.buf.end);
	return (0);
}

public int lesskey_src(constant char *filename, lbool sysvar)
{
	return lesskey_text(filename, sysvar, FALSE);
}

public int lesskey_content(constant char *content, lbool sysvar)
{
	return lesskey_text(content, sysvar, TRUE);
}

void lesskey_parse_error(char *s)
{
	PARG parg;
	parg.p_string = s;
	error("%s", &parg);
}
#endif /* HAVE_LESSKEYSRC */

/*
 * Add a lesskey file.
 */
static int add_hometable(int (*call_lesskey)(constant char *, lbool), constant char *envname, constant char *def_filename, lbool sysvar)
{
	char *filename = NULL;
	constant char *efilename;
	int r;

	if (envname != NULL && (efilename = lgetenv(envname)) != NULL)
		filename = save(efilename);
	else if (sysvar) /* def_filename is full path */
		filename = save(def_filename);
	else /* def_filename is just basename */
	{
		/* Remove first char (normally a dot) unless stored in $HOME. */
		constant char *xdg = lgetenv("XDG_CONFIG_HOME");
		if (!isnullenv(xdg))
			filename = dirfile(xdg, &def_filename[1], 1);
		if (filename == NULL)
		{
			constant char *home = lgetenv("HOME");
			if (!isnullenv(home))
			{
				char *cfg_dir = dirfile(home, ".config", 0);
				filename = dirfile(cfg_dir, &def_filename[1], 1);
				free(cfg_dir);
			}
		}
		if (filename == NULL)
			filename = homefile(def_filename);
	}
	if (filename == NULL)
		return -1;
	r = (*call_lesskey)(filename, sysvar);
	free(filename);
	return (r);
}

/*
 * Add the content of a lesskey source file.
 */
static void add_content_table(int (*call_lesskey)(constant char *, lbool), constant char *envname, lbool sysvar)
{
	constant char *content;

	(void) call_lesskey; /* not used */
	content = lgetenv(envname);
	if (isnullenv(content))
		return;
	lesskey_content(content, sysvar);
}
#endif /* USERFILE */

/*
 * See if a char is a special line-editing command.
 */
public int editchar(char c, int flags)
{
	int action;
	int nch;
	constant char *s;
	char usercmd[MAX_CMDLEN+1];
	
	/*
	 * An editing character could actually be a sequence of characters;
	 * for example, an escape sequence sent by pressing the uparrow key.
	 * To match the editing string, we use the command decoder
	 * but give it the edit-commands command table
	 * This table is constructed to match the user's keyboard.
	 */
	if (c == erase_char || c == erase2_char)
		return (EC_BACKSPACE);
	if (c == kill_char)
	{
#if MSDOS_COMPILER==WIN32C
		if (!win32_kbhit())
#endif
		return (EC_LINEKILL);
	}
		
	/*
	 * Collect characters in a buffer.
	 * Start with the one we have, and get more if we need them.
	 */
	nch = 0;
	do {
		if (nch > 0)
			c = getcc();
		usercmd[nch] = c;
		usercmd[nch+1] = '\0';
		nch++;
		action = ecmd_decode(usercmd, &s);
	} while (action == A_PREFIX && nch < MAX_CMDLEN);

	if (action == EC_X11MOUSE)
		return (x11mouse_action(TRUE));
	if (action == EC_X116MOUSE)
		return (x116mouse_action(TRUE));

	if (flags & ECF_NORIGHTLEFT)
	{
		switch (action)
		{
		case EC_RIGHT:
		case EC_LEFT:
			action = A_INVALID;
			break;
		}
	}
#if CMD_HISTORY
	if (flags & ECF_NOHISTORY) 
	{
		/*
		 * The caller says there is no history list.
		 * Reject any history-manipulation action.
		 */
		switch (action)
		{
		case EC_UP:
		case EC_DOWN:
			action = A_INVALID;
			break;
		}
	}
#endif
	if (flags & ECF_NOCOMPLETE) 
	{
		/*
		 * The caller says we don't want any filename completion cmds.
		 * Reject them.
		 */
		switch (action)
		{
		case EC_F_COMPLETE:
		case EC_B_COMPLETE:
		case EC_EXPAND:
			action = A_INVALID;
			break;
		}
	}
	if ((flags & ECF_PEEK) || action == A_INVALID)
	{
		/*
		 * We're just peeking, or we didn't understand the command.
		 * Unget all the characters we read in the loop above.
		 * This does NOT include the original character that was 
		 * passed in as a parameter.
		 */
		while (nch > 1) 
		{
			ungetcc(usercmd[--nch]);
		}
	} else
	{
		if (s != NULL)
			ungetsc(s);
	}
	return action;
}

