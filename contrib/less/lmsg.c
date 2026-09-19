/*
 * Copyright (C) 1984-2026  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

#include "less.h"

struct lmsg_sym { lessmsg_id id; constant char *sym; };

/*
 * lmsgs is the array of all messages, indexed by lmsg id.
 * Initialize it to the default messages.
 * Individual message can be overridden by a custom lessmsg file.
 */
#undef  M
#define M(sym,msg) msg,
static char *lmsgs[] = {
	NULL, /* id 0 (LM_NULL) is not in lessmsg.inc */
#include "lessmsg.inc"
};

/*
 * lmsg_syms is the map of symbol names to lmsg ids.
 */
#undef  M
#define M(sym,msg) { LM_##sym, #sym },
static constant struct lmsg_sym lmsg_syms[] = {
#include "lessmsg.inc"
	M(HELP,"")
};

/*
 * For historical reasons the help file is stored separately rather than
 * in the lmsgs array but it can still be overridden by a lessmsg file.
 */
public char *help_data;
public int size_help_data;
extern char helpdata[];
extern int size_helpdata;

/*
 * Report an error in a lessmsg file.
 */
static void lmsg_error(constant char *err, constant char *lmsg_file, int linenum)
{
	PARG parg[3];
	parg[0].p_string = lmsg_file;
	parg[1].p_int = linenum;
	parg[2].p_string = err;
	error("Error in lessmsg file %s line %d: %s", parg);
}

/*
 * Return a numeric lmsg id given a symbolic name.
 */
static lessmsg_id lmsg_id(constant char *sym)
{
	size_t i;
	for (i = 0; i < countof(lmsg_syms); i++)
		if (strcmp(sym, lmsg_syms[i].sym) == 0)
			return lmsg_syms[i].id;
	return LM_NULL;
}

/*
 * Return a numeric char value from a string.
 */
static char numeric_char(char **pp, int radix, constant char *lmsg_file, int linenum)
{
	constant int max_chars = (radix == 16) ? 2 : 3;
	char nbuf[4]; /* max_chars+1 */
	char *p = *pp;
	int val;
	strncpy(nbuf, p, max_chars);
	nbuf[max_chars] = '\0';
	val = lstrtoi(nbuf, &p, radix);
	if (p == nbuf || val < 0 || val > 255)
	{
		lmsg_error("invalid numeric char value", lmsg_file, linenum);
		return '?';
	}
	*pp += ptr_diff(p,nbuf);
	return (char) val;
}

/*
 * Process one character in a lessmsg file.
 */
static char * lmsg_char(char *mp, char *line, size_t line_size, FILE* fd, struct xbuffer *xbuf, constant char *lmsg_file, int *linenum)
{
	if (*mp != '\\')
		xbuf_add_char(xbuf, *mp++);
	else
	{
		/* Handle backslash escape. */
		switch (*++mp)
		{
		case '\0': break;
		case 'n': xbuf_add_char(xbuf, '\n'); ++mp; break;
		case 'r': xbuf_add_char(xbuf, '\r'); ++mp; break;
		case 't': xbuf_add_char(xbuf, '\t'); ++mp; break;
		case '\n': case '\r': /* continued on next line */
			if (fgets(line, line_size, fd) != NULL)
			{
				++*linenum;
				mp = line;
			}
			break;
		case 'x': /* hex value */
			++mp;
			xbuf_add_char(xbuf, numeric_char(&mp, 16, lmsg_file, *linenum));
			break;
		default:
			if (*mp >= '0' && *mp <= '7') /* octal value */
				xbuf_add_char(xbuf, numeric_char(&mp, 8, lmsg_file, *linenum));
			else /* literal char */
				xbuf_add_char(xbuf, *mp++);
			break;
		}
	}
	return mp;
}

/*
 * Process one line in a lessmsg file.
 */
static lbool lmsg_line(char *line, size_t line_size, FILE* fd, struct xbuffer *xbuf, constant char *lmsg_file, int *linenum)
{
	lessmsg_id id;
	char *msg;
	char *mp;
	char *sym = skipsp(line);

	if (sym[0] == '\n' || sym[0] == '\r' || sym[0] == '#')
		return TRUE; /* skip comments and blank lines */
	msg = strchr(sym, ' ');
	if (msg == NULL)
	{
		lmsg_error("missing space", lmsg_file, *linenum);
		return FALSE;
	}
	*msg++ = '\0'; /* terminate symbol */
	msg = skipsp(msg);
	id = lmsg_id(sym);
	if (id == 0)
	{
		lmsg_error("unknown lessmsg symbol", lmsg_file, *linenum);
		return FALSE;
	}
	/* Copy the message string into xbuf. */
	xbuf_reset(xbuf);
	for (mp = msg;  *mp != '\0' && *mp != '\n' && *mp != '\r'; )
		mp = lmsg_char(mp, line, line_size, fd, xbuf, lmsg_file, linenum);
	/* Store the message string in the table. */
	msg = saven(xbuf_char_data(xbuf), xbuf->end);
	if (id == LM_HELP)
	{
		help_data = msg;
		size_help_data = xbuf->end;
	} else
		lmsgs[id] = msg;
	return TRUE;
}

/*
 * Initialize the lmsgs array from an lmsg file.
 */
public void lmsg_init(constant char *lmsg_file)
{
	FILE *fd;
	char line[512];
	struct xbuffer xbuf;
	int linenum;
	PARG parg;

	help_data = helpdata;
	size_help_data = size_helpdata;
	if (isnullenv(lmsg_file))
		return;

	fd = fopen(lmsg_file, "r");
	if (fd == NULL)
	{
		parg.p_string = lmsg_file;
		error("Cannot open lessmsg file %s", &parg);
		return;
	}
	xbuf_init(&xbuf); /* for efficiency, init once here and reuse for each line */
	linenum = 0;
	while (fgets(line, sizeof(line), fd) != NULL)
	{
		++linenum;
		lmsg_line(line, sizeof(line), fd, &xbuf, lmsg_file, &linenum);
	}
	xbuf_deinit(&xbuf);
	fclose(fd);
}

/*
 * Return the string associated with a numeric lmsg id.
 */
public constant char * lmsg(lessmsg_id id)
{
#if 1
	if (id <= 0 || id >= countof(lmsgs))
	{
		PARG parg;
		parg.p_int = (int) id;
		error("invalid lessmsg id %d", &parg);
		return NULL;
	}
#endif
	return lmsgs[id];
}
