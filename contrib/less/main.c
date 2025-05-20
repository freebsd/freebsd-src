/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Entry point, initialization, miscellaneous routines.
 */

#include "less.h"
#if MSDOS_COMPILER==WIN32C
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(MINGW) || defined(_MSC_VER)
#include <locale.h>
#include <shellapi.h>
#endif

public unsigned less_acp = CP_ACP;
#endif

#include "option.h"

public char *   every_first_cmd = NULL;
public lbool    new_file;
public int      is_tty;
public IFILE    curr_ifile = NULL_IFILE;
public IFILE    old_ifile = NULL_IFILE;
public struct scrpos initial_scrpos;
public POSITION start_attnpos = NULL_POSITION;
public POSITION end_attnpos = NULL_POSITION;
public int      wscroll;
public constant char *progname;
public lbool    quitting = FALSE;
public int      dohelp;
public char *   init_header = NULL;
static int      secure_allow_features;

#if LOGFILE
public int      logfile = -1;
public lbool    force_logfile = FALSE;
public char *   namelogfile = NULL;
#endif

#if EDITOR
public constant char *   editor;
public constant char *   editproto;
#endif

#if TAGS
extern char *   tags;
extern char *   tagoption;
extern int      jump_sline;
#endif

#if HAVE_TIME
public time_type less_start_time;
#endif

#ifdef WIN32
static wchar_t consoleTitle[256];
#endif

public int      one_screen;
extern int      less_is_more;
extern lbool    missing_cap;
extern int      know_dumb;
extern int      quit_if_one_screen;
extern int      no_init;
extern int      errmsgs;
extern int      redraw_on_quit;
extern int      term_init_done;
extern lbool    first_time;

#if MSDOS_COMPILER==WIN32C && (defined(MINGW) || defined(_MSC_VER))
/* malloc'ed 0-terminated utf8 of 0-terminated wide ws, or null on errors */
static char *utf8_from_wide(constant wchar_t *ws)
{
	char *u8 = NULL;
	int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
	if (n > 0)
	{
		u8 = ecalloc(n, sizeof(char));
		WideCharToMultiByte(CP_UTF8, 0, ws, -1, u8, n, NULL, NULL);
	}
	return u8;
}

/*
 * similar to using UTF8 manifest to make the ANSI APIs UTF8, but dynamically
 * with setlocale. unlike the manifest, argv and environ are already ACP, so
 * make them UTF8. Additionally, this affects only the libc/crt API, and so
 * e.g. fopen filename becomes UTF-8, but CreateFileA filename remains CP_ACP.
 * CP_ACP remains the original codepage - use the dynamic less_acp instead.
 * effective on win 10 1803 or later when compiled with ucrt, else no-op.
 */
static void try_utf8_locale(int *pargc, constant char ***pargv)
{
	char *locale_orig = strdup(setlocale(LC_ALL, NULL));
	wchar_t **wargv = NULL, *wenv, *wp;
	constant char **u8argv;
	char *u8e;
	int i, n;

	if (!setlocale(LC_ALL, ".UTF8"))
		goto cleanup;  /* not win10 1803+ or not ucrt */

	/*
	 * wargv is before glob expansion. some ucrt builds may expand globs
	 * before main is entered, so n may be smaller than the original argc.
	 * that's ok, because later code at main expands globs anyway.
	 */
	wargv = CommandLineToArgvW(GetCommandLineW(), &n);
	if (!wargv)
		goto bad_args;

	u8argv = (constant char **) ecalloc(n + 1, sizeof(char *));
	for (i = 0; i < n; ++i)
	{
		if (!(u8argv[i] = utf8_from_wide(wargv[i])))
			goto bad_args;
	}
	u8argv[n] = 0;

	less_acp = CP_UTF8;
	*pargc = n;
	*pargv = u8argv;  /* leaked on exit */

	/* convert wide env to utf8 where we can, but don't abort on errors */
	if ((wenv = GetEnvironmentStringsW()))
	{
		for (wp = wenv; *wp; wp += wcslen(wp) + 1)
		{
			if ((u8e = utf8_from_wide(wp)))
				_putenv(u8e);
			free(u8e);  /* windows putenv makes a copy */
		}
		FreeEnvironmentStringsW(wenv);
	}

	goto cleanup;

bad_args:
	error("WARNING: cannot use unicode arguments", NULL_PARG);
	setlocale(LC_ALL, locale_orig);

cleanup:
	free(locale_orig);
	LocalFree(wargv);
}
#endif

#if !SECURE
static int security_feature_error(constant char *type, size_t len, constant char *name)
{
	PARG parg;
	size_t msglen = len + strlen(type) + 64;
	char *msg = ecalloc(msglen, sizeof(char));
	SNPRINTF3(msg, msglen, "LESSSECURE_ALLOW: %s feature name \"%.*s\"", type, (int) len, name);
	parg.p_string = msg;
	error("%s", &parg);
	free(msg);
	return 0;
}

/*
 * Return the SF_xxx value of a secure feature given the name of the feature.
 */
static int security_feature(constant char *name, size_t len)
{
	struct secure_feature { constant char *name; int sf_value; };
	static struct secure_feature features[] = {
		{ "edit",     SF_EDIT },
		{ "examine",  SF_EXAMINE },
		{ "glob",     SF_GLOB },
		{ "history",  SF_HISTORY },
		{ "lesskey",  SF_LESSKEY },
		{ "lessopen", SF_LESSOPEN },
		{ "logfile",  SF_LOGFILE },
		{ "osc8",     SF_OSC8_OPEN },
		{ "pipe",     SF_PIPE },
		{ "shell",    SF_SHELL },
		{ "stop",     SF_STOP },
		{ "tags",     SF_TAGS },
	};
	int i;
	int match = -1;

	for (i = 0;  i < countof(features);  i++)
	{
		if (strncmp(features[i].name, name, len) == 0)
		{
			if (match >= 0) /* name is ambiguous */
				return security_feature_error("ambiguous", len, name);
			match = i;
		}
	}
	if (match < 0)
		return security_feature_error("invalid", len, name);
	return features[match].sf_value;
}
#endif /* !SECURE */

/*
 * Set the secure_allow_features bitmask, which controls
 * whether certain secure features are allowed.
 */
static void init_secure(void)
{
#if SECURE
	secure_allow_features = 0;
#else
	constant char *str = lgetenv("LESSSECURE");
	if (isnullenv(str))
		secure_allow_features = ~0; /* allow everything */
	else
		secure_allow_features = 0; /* allow nothing */

	str = lgetenv("LESSSECURE_ALLOW");
	if (!isnullenv(str))
	{
		for (;;)
		{
			constant char *estr;
			while (*str == ' ' || *str == ',') ++str; /* skip leading spaces/commas */
			if (*str == '\0') break;
			estr = strchr(str, ',');
			if (estr == NULL) estr = str + strlen(str);
			while (estr > str && estr[-1] == ' ') --estr; /* trim trailing spaces */
			secure_allow_features |= security_feature(str, ptr_diff(estr, str));
			str = estr;
		}
	}
#endif
}

/*
 * Entry point.
 */
int main(int argc, constant char *argv[])
{
	IFILE ifile;
	constant char *s;

#if MSDOS_COMPILER==WIN32C && (defined(MINGW) || defined(_MSC_VER))
	if (GetACP() != CP_UTF8)  /* not using a UTF-8 manifest */
		try_utf8_locale(&argc, &argv);
#endif

#ifdef __EMX__
	_response(&argc, &argv);
	_wildcard(&argc, &argv);
#endif

	progname = *argv++;
	argc--;
	init_secure();

#ifdef WIN32
	if (getenv("HOME") == NULL)
	{
		/*
		 * If there is no HOME environment variable,
		 * try the concatenation of HOMEDRIVE + HOMEPATH.
		 */
		char *drive = getenv("HOMEDRIVE");
		char *path  = getenv("HOMEPATH");
		if (drive != NULL && path != NULL)
		{
			char *env = (char *) ecalloc(strlen(drive) + 
					strlen(path) + 6, sizeof(char));
			strcpy(env, "HOME=");
			strcat(env, drive);
			strcat(env, path);
			putenv(env);
		}
	}
	/* on failure, consoleTitle is already a valid empty string */
	GetConsoleTitleW(consoleTitle, countof(consoleTitle));
#endif /* WIN32 */

	/*
	 * Process command line arguments and LESS environment arguments.
	 * Command line arguments override environment arguments.
	 */
	is_tty = isatty(1);
	init_mark();
	init_cmds();
	init_poll();
	init_charset();
	init_line();
	init_cmdhist();
	init_option();
	init_search();

	/*
	 * If the name of the executable program is "more",
	 * act like LESS_IS_MORE is set.
	 */
	if (strcmp(last_component(progname), "more") == 0 &&
			isnullenv(lgetenv("LESS_IS_MORE"))) {
		less_is_more = 1;
		scan_option("-fG", FALSE);
	}

	init_prompt();

	init_unsupport();
	s = lgetenv(less_is_more ? "MORE" : "LESS");
	if (s != NULL)
		scan_option(s, TRUE);

#define isoptstring(s)  less_is_more ? (((s)[0] == '-') && (s)[1] != '\0') : \
			(((s)[0] == '-' || (s)[0] == '+') && (s)[1] != '\0')
	while (argc > 0 && (isoptstring(*argv) || isoptpending()))
	{
		s = *argv++;
		argc--;
		if (strcmp(s, "--") == 0)
			break;
		scan_option(s, FALSE);
	}
#undef isoptstring

	if (isoptpending())
	{
		/*
		 * Last command line option was a flag requiring a
		 * following string, but there was no following string.
		 */
		nopendopt();
		quit(QUIT_OK);
	}

	if (less_is_more)
		no_init = TRUE;

	get_term();
	expand_cmd_tables();

#if EDITOR
	editor = lgetenv("VISUAL");
	if (isnullenv(editor))
	{
		editor = lgetenv("EDITOR");
		if (isnullenv(editor))
			editor = EDIT_PGM;
	}
	editproto = lgetenv("LESSEDIT");
	if (isnullenv(editproto))
		editproto = "%E ?lm+%lm. %g";
#endif

	/*
	 * Call get_ifile with all the command line filenames
	 * to "register" them with the ifile system.
	 */
	ifile = NULL_IFILE;
	if (dohelp)
		ifile = get_ifile(FAKE_HELPFILE, ifile);
	while (argc-- > 0)
	{
#if (MSDOS_COMPILER && MSDOS_COMPILER != DJGPPC)
		/*
		 * Because the "shell" doesn't expand filename patterns,
		 * treat each argument as a filename pattern rather than
		 * a single filename.  
		 * Expand the pattern and iterate over the expanded list.
		 */
		struct textlist tlist;
		constant char *filename;
		char *gfilename;
		char *qfilename;
		
		gfilename = lglob(*argv++);
		init_textlist(&tlist, gfilename);
		filename = NULL;
		while ((filename = forw_textlist(&tlist, filename)) != NULL)
		{
			qfilename = shell_unquote(filename);
			(void) get_ifile(qfilename, ifile);
			free(qfilename);
			ifile = prev_ifile(NULL_IFILE);
		}
		free(gfilename);
#else
		(void) get_ifile(*argv++, ifile);
		ifile = prev_ifile(NULL_IFILE);
#endif
	}
	/*
	 * Set up terminal, etc.
	 */
	if (!is_tty)
	{
		/*
		 * Output is not a tty.
		 * Just copy the input file(s) to output.
		 */
		set_output(1); /* write to stdout */
		SET_BINARY(1);
		if (edit_first() == 0)
		{
			do {
				cat_file();
			} while (edit_next(1) == 0);
		}
		quit(QUIT_OK);
	}

	if (missing_cap && !know_dumb && !less_is_more)
		error("WARNING: terminal is not fully functional", NULL_PARG);
	open_getchr();
	raw_mode(1);
	init_signals(1);
#if HAVE_TIME
	less_start_time = get_time();
#endif

	/*
	 * Select the first file to examine.
	 */
#if TAGS
	if (tagoption != NULL || strcmp(tags, "-") == 0)
	{
		/*
		 * A -t option was given.
		 * Verify that no filenames were also given.
		 * Edit the file selected by the "tags" search,
		 * and search for the proper line in the file.
		 */
		if (nifile() > 0)
		{
			error("No filenames allowed with -t option", NULL_PARG);
			quit(QUIT_ERROR);
		}
		findtag(tagoption);
		if (edit_tagfile())  /* Edit file which contains the tag */
			quit(QUIT_ERROR);
		/*
		 * Search for the line which contains the tag.
		 * Set up initial_scrpos so we display that line.
		 */
		initial_scrpos.pos = tagsearch();
		if (initial_scrpos.pos == NULL_POSITION)
			quit(QUIT_ERROR);
		initial_scrpos.ln = jump_sline;
	} else
#endif
	{
		if (edit_first())
			quit(QUIT_ERROR);
		/*
		 * See if file fits on one screen to decide whether 
		 * to send terminal init. But don't need this 
		 * if -X (no_init) overrides this (see init()).
		 */
		if (quit_if_one_screen)
		{
			if (nifile() > 1) /* If more than one file, -F cannot be used */
				quit_if_one_screen = FALSE;
			else if (!no_init)
				one_screen = get_one_screen();
		}
	}
	if (init_header != NULL)
	{
		opt_header(TOGGLE, init_header);
		free(init_header);
		init_header = NULL;
	}

	if (errmsgs > 0)
	{
		/*
		 * We displayed some messages on error output
		 * (file descriptor 2; see flush()).
		 * Before erasing the screen contents, wait for a keystroke.
		 */
		less_printf("Press RETURN to continue ", NULL_PARG);
		get_return();
		putchr('\n');
	}
	set_output(1);
	init();
	commands();
	quit(QUIT_OK);
	/*NOTREACHED*/
	return (0);
}

/*
 * Copy a string to a "safe" place
 * (that is, to a buffer allocated by calloc).
 */
public char * saven(constant char *s, size_t n)
{
	char *p = (char *) ecalloc(n+1, sizeof(char));
	strncpy(p, s, n);
	p[n] = '\0';
	return (p);
}

public char * save(constant char *s)
{
	return saven(s, strlen(s));
}

public void out_of_memory(void)
{
	error("Cannot allocate memory", NULL_PARG);
	quit(QUIT_ERROR);
}

/*
 * Allocate memory.
 * Like calloc(), but never returns an error (NULL).
 */
public void * ecalloc(size_t count, size_t size)
{
	void * p;

	p = (void *) calloc(count, size);
	if (p == NULL)
		out_of_memory();
	return p;
}

/*
 * Skip leading spaces in a string.
 */
public char * skipsp(char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

/* {{ There must be a better way. }} */
public constant char * skipspc(constant char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return (s);
}

/*
 * See how many characters of two strings are identical.
 * If uppercase is true, the first string must begin with an uppercase
 * character; the remainder of the first string may be either case.
 */
public size_t sprefix(constant char *ps, constant char *s, int uppercase)
{
	char c;
	char sc;
	size_t len = 0;

	for ( ;  *s != '\0';  s++, ps++)
	{
		c = *ps;
		if (uppercase)
		{
			if (len == 0 && ASCII_IS_LOWER(c))
				return (0);
			if (ASCII_IS_UPPER(c))
				c = ASCII_TO_LOWER(c);
		}
		sc = *s;
		if (len > 0 && ASCII_IS_UPPER(sc))
			sc = ASCII_TO_LOWER(sc);
		if (c != sc)
			break;
		len++;
	}
	return (len);
}

/*
 * Exit the program.
 */
public void quit(int status)
{
	static int save_status;

	/*
	 * Put cursor at bottom left corner, clear the line,
	 * reset the terminal modes, and exit.
	 */
	if (status < 0)
		status = save_status;
	else
		save_status = status;
	quitting = TRUE;
	check_altpipe_error();
	if (interactive())
		clear_bot();
	deinit();
	flush();
	if (redraw_on_quit && term_init_done)
	{
		/*
		 * The last file text displayed might have been on an 
		 * alternate screen, which now (since deinit) cannot be seen.
		 * redraw_on_quit tells us to redraw it on the main screen.
		 */
		first_time = TRUE; /* Don't print "skipping" or tildes */
		repaint();
		flush();
	}
	edit((char*)NULL);
	save_cmdhist();
	raw_mode(0);
#if MSDOS_COMPILER && MSDOS_COMPILER != DJGPPC
	/* 
	 * If we don't close 2, we get some garbage from
	 * 2's buffer when it flushes automatically.
	 * I cannot track this one down  RB
	 * The same bug shows up if we use ^C^C to abort.
	 */
	close(2);
#endif
#ifdef WIN32
	SetConsoleTitleW(consoleTitle);
#endif
	close_getchr();
	exit(status);
} 
	
/*
 * Are all the features in the features mask allowed by security?
 */
public int secure_allow(int features)
{
	return ((secure_allow_features & features) == features);
}
