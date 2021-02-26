/*
 *  $Id: rc.c,v 1.60 2020/11/25 00:06:40 tom Exp $
 *
 *  rc.c -- routines for processing the configuration file
 *
 *  Copyright 2000-2019,2020	Thomas E. Dickey
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
 *
 *  An earlier version of this program lists as authors
 *	Savio Lam (lam836@cs.cuhk.hk)
 */

#include <dialog.h>

#include <dlg_keys.h>

#ifdef HAVE_COLOR
#include <dlg_colors.h>
#include <dlg_internals.h>

#define L_PAREN '('
#define R_PAREN ')'

#define MIN_TOKEN 3
#ifdef HAVE_RC_FILE2
#define MAX_TOKEN 5
#else
#define MAX_TOKEN MIN_TOKEN
#endif

#define UNKNOWN_COLOR -2

/*
 * For matching color names with color values
 */
static const color_names_st color_names[] =
{
#ifdef HAVE_USE_DEFAULT_COLORS
    {"DEFAULT", -1},
#endif
    {"BLACK", COLOR_BLACK},
    {"RED", COLOR_RED},
    {"GREEN", COLOR_GREEN},
    {"YELLOW", COLOR_YELLOW},
    {"BLUE", COLOR_BLUE},
    {"MAGENTA", COLOR_MAGENTA},
    {"CYAN", COLOR_CYAN},
    {"WHITE", COLOR_WHITE},
};				/* color names */
#define COLOR_COUNT     TableSize(color_names)
#endif /* HAVE_COLOR */

#define GLOBALRC "/etc/dialogrc"
#define DIALOGRC ".dialogrc"

/* Types of values */
#define VAL_INT  0
#define VAL_STR  1
#define VAL_BOOL 2

/* Type of line in configuration file */
typedef enum {
    LINE_ERROR = -1,
    LINE_EQUALS,
    LINE_EMPTY
} PARSE_LINE;

/* number of configuration variables */
#define VAR_COUNT        TableSize(vars)

/* check if character is string quoting characters */
#define isquote(c)       ((c) == '"' || (c) == '\'')

/* get last character of string */
#define lastch(str)      str[strlen(str)-1]

/*
 * Configuration variables
 */
typedef struct {
    const char *name;		/* name of configuration variable as in DIALOGRC */
    void *var;			/* address of actual variable to change */
    int type;			/* type of value */
    const char *comment;	/* comment to put in "rc" file */
} vars_st;

/*
 * This table should contain only references to dialog_state, since dialog_vars
 * is reset specially in dialog.c before each widget.
 */
static const vars_st vars[] =
{
    {"aspect",
     &dialog_state.aspect_ratio,
     VAL_INT,
     "Set aspect-ration."},

    {"separate_widget",
     &dialog_state.separate_str,
     VAL_STR,
     "Set separator (for multiple widgets output)."},

    {"tab_len",
     &dialog_state.tab_len,
     VAL_INT,
     "Set tab-length (for textbox tab-conversion)."},

    {"visit_items",
     &dialog_state.visit_items,
     VAL_BOOL,
     "Make tab-traversal for checklist, etc., include the list."},

#ifdef HAVE_COLOR
    {"use_shadow",
     &dialog_state.use_shadow,
     VAL_BOOL,
     "Shadow dialog boxes? This also turns on color."},

    {"use_colors",
     &dialog_state.use_colors,
     VAL_BOOL,
     "Turn color support ON or OFF"},
#endif				/* HAVE_COLOR */
};				/* vars */

static int
skip_whitespace(char *str, int n)
{
    while (isblank(UCH(str[n])) && str[n] != '\0')
	n++;
    return n;
}

static int
skip_keyword(char *str, int n)
{
    while (isalnum(UCH(str[n])) && str[n] != '\0')
	n++;
    return n;
}

static void
trim_token(char **tok)
{
    char *tmp = *tok + skip_whitespace(*tok, 0);

    *tok = tmp;

    while (*tmp != '\0' && !isblank(UCH(*tmp)))
	tmp++;

    *tmp = '\0';
}

static int
from_boolean(const char *str)
{
    int code = -1;

    if (str != NULL && *str != '\0') {
	if (!dlg_strcmp(str, "ON")) {
	    code = 1;
	} else if (!dlg_strcmp(str, "OFF")) {
	    code = 0;
	}
    }
    return code;
}

static int
from_color_name(const char *str)
{
    int code = UNKNOWN_COLOR;

    if (str != NULL && *str != '\0') {
	size_t i;

	for (i = 0; i < COLOR_COUNT; ++i) {
	    if (!dlg_strcmp(str, color_names[i].name)) {
		code = color_names[i].value;
		break;
	    }
	}
    }
    return code;
}

static int
find_vars(char *name)
{
    int result = -1;
    unsigned i;

    for (i = 0; i < VAR_COUNT; i++) {
	if (dlg_strcmp(vars[i].name, name) == 0) {
	    result = (int) i;
	    break;
	}
    }
    return result;
}

#ifdef HAVE_COLOR
static int
find_color(char *name)
{
    int result = -1;
    int i;
    int limit = dlg_color_count();

    for (i = 0; i < limit; i++) {
	if (dlg_strcmp(dlg_color_table[i].name, name) == 0) {
	    result = i;
	    break;
	}
    }
    return result;
}

static const char *
to_color_name(int code)
{
    const char *result = "?";
    size_t n;
    for (n = 0; n < TableSize(color_names); ++n) {
	if (code == color_names[n].value) {
	    result = color_names[n].name;
	    break;
	}
    }
    return result;
}

static const char *
to_boolean(int code)
{
    return code ? "ON" : "OFF";
}

/*
 * Extract the foreground, background and highlight values from an attribute
 * represented as a string in one of these forms:
 *
 * "(foreground,background,highlight,underline,reverse)"
 * "(foreground,background,highlight,underline)"
 * "(foreground,background,highlight)"
 * "xxxx_color"
 */
static int
str_to_attr(char *str, DIALOG_COLORS * result)
{
    char *tokens[MAX_TOKEN + 1];
    char tempstr[MAX_LEN + 1];
    size_t have;
    size_t i = 0;
    size_t tok_count = 0;

    memset(result, 0, sizeof(*result));
    result->fg = -1;
    result->bg = -1;
    result->hilite = -1;

    if (str[0] != L_PAREN || lastch(str) != R_PAREN) {
	int ret;

	if ((ret = find_color(str)) >= 0) {
	    *result = dlg_color_table[ret];
	    return 0;
	}
	/* invalid representation */
	return -1;
    }

    /* remove the parenthesis */
    have = strlen(str);
    if (have > MAX_LEN) {
	have = MAX_LEN - 1;
    } else {
	have -= 2;
    }
    memcpy(tempstr, str + 1, have);
    tempstr[have] = '\0';

    /* parse comma-separated tokens, allow up to
     * one more than max tokens to detect extras */
    while (tok_count < TableSize(tokens)) {

	tokens[tok_count++] = &tempstr[i];

	while (tempstr[i] != '\0' && tempstr[i] != ',')
	    i++;

	if (tempstr[i] == '\0')
	    break;

	tempstr[i++] = '\0';
    }

    if (tok_count < MIN_TOKEN || tok_count > MAX_TOKEN) {
	/* invalid representation */
	return -1;
    }

    for (i = 0; i < tok_count; ++i)
	trim_token(&tokens[i]);

    /* validate */
    if (UNKNOWN_COLOR == (result->fg = from_color_name(tokens[0]))
	|| UNKNOWN_COLOR == (result->bg = from_color_name(tokens[1]))
	|| UNKNOWN_COLOR == (result->hilite = from_boolean(tokens[2]))
#ifdef HAVE_RC_FILE2
	|| (tok_count >= 4 && (result->ul = from_boolean(tokens[3])) == -1)
	|| (tok_count >= 5 && (result->rv = from_boolean(tokens[4])) == -1)
#endif /* HAVE_RC_FILE2 */
	) {
	/* invalid representation */
	return -1;
    }

    return 0;
}
#endif /* HAVE_COLOR */

/*
 * Check if the line begins with a special keyword; if so, return true while
 * pointing params to its parameters.
 */
static int
begins_with(char *line, const char *keyword, char **params)
{
    int i = skip_whitespace(line, 0);
    int j = skip_keyword(line, i);

    if ((j - i) == (int) strlen(keyword)) {
	char save = line[j];
	line[j] = 0;
	if (!dlg_strcmp(keyword, line + i)) {
	    *params = line + skip_whitespace(line, j + 1);
	    return 1;
	}
	line[j] = save;
    }

    return 0;
}

/*
 * Parse a line in the configuration file
 *
 * Each line is of the form:  "variable = value". On exit, 'var' will contain
 * the variable name, and 'value' will contain the value string.
 *
 * Return values:
 *
 * LINE_EMPTY   - line is blank or comment
 * LINE_EQUALS  - line contains "variable = value"
 * LINE_ERROR   - syntax error in line
 */
static PARSE_LINE
parse_line(char *line, char **var, char **value)
{
    int i = 0;

    /* ignore white space at beginning of line */
    i = skip_whitespace(line, i);

    if (line[i] == '\0')	/* line is blank */
	return LINE_EMPTY;
    else if (line[i] == '#')	/* line is comment */
	return LINE_EMPTY;
    else if (line[i] == '=')	/* variable names cannot start with a '=' */
	return LINE_ERROR;

    /* set 'var' to variable name */
    *var = line + i++;		/* skip to next character */

    /* find end of variable name */
    while (!isblank(UCH(line[i])) && line[i] != '=' && line[i] != '\0')
	i++;

    if (line[i] == '\0')	/* syntax error */
	return LINE_ERROR;
    else if (line[i] == '=')
	line[i++] = '\0';
    else {
	line[i++] = '\0';

	/* skip white space before '=' */
	i = skip_whitespace(line, i);

	if (line[i] != '=')	/* syntax error */
	    return LINE_ERROR;
	else
	    i++;		/* skip the '=' */
    }

    /* skip white space after '=' */
    i = skip_whitespace(line, i);

    if (line[i] == '\0')
	return LINE_ERROR;
    else
	*value = line + i;	/* set 'value' to value string */

    /* trim trailing white space from 'value' */
    i = (int) strlen(*value) - 1;
    while (isblank(UCH((*value)[i])) && i > 0)
	i--;
    (*value)[i + 1] = '\0';

    return LINE_EQUALS;		/* no syntax error in line */
}

/*
 * Create the configuration file
 */
void
dlg_create_rc(const char *filename)
{
    unsigned i;
    FILE *rc_file;

    if ((rc_file = fopen(filename, "wt")) == NULL)
	dlg_exiterr("Error opening file for writing in dlg_create_rc().");

    fprintf(rc_file, "#\n\
# Run-time configuration file for dialog\n\
#\n\
# Automatically generated by \"dialog --create-rc <file>\"\n\
#\n\
#\n\
# Types of values:\n\
#\n\
# Number     -  <number>\n\
# String     -  \"string\"\n\
# Boolean    -  <ON|OFF>\n"
#ifdef HAVE_COLOR
#ifdef HAVE_RC_FILE2
	    "\
# Attribute  -  (foreground,background,highlight?,underline?,reverse?)\n"
#else /* HAVE_RC_FILE2 */
	    "\
# Attribute  -  (foreground,background,highlight?)\n"
#endif /* HAVE_RC_FILE2 */
#endif /* HAVE_COLOR */
	);

    /* Print an entry for each configuration variable */
    for (i = 0; i < VAR_COUNT; i++) {
	fprintf(rc_file, "\n# %s\n", vars[i].comment);
	switch (vars[i].type) {
	case VAL_INT:
	    fprintf(rc_file, "%s = %d\n", vars[i].name,
		    *((int *) vars[i].var));
	    break;
	case VAL_STR:
	    fprintf(rc_file, "%s = \"%s\"\n", vars[i].name,
		    (char *) vars[i].var);
	    break;
	case VAL_BOOL:
	    fprintf(rc_file, "%s = %s\n", vars[i].name,
		    *((bool *) vars[i].var) ? "ON" : "OFF");
	    break;
	}
    }
#ifdef HAVE_COLOR
    for (i = 0; i < (unsigned) dlg_color_count(); ++i) {
	unsigned j;
	bool repeat = FALSE;

	fprintf(rc_file, "\n# %s\n", dlg_color_table[i].comment);
	for (j = 0; j != i; ++j) {
	    if (dlg_color_table[i].fg == dlg_color_table[j].fg
		&& dlg_color_table[i].bg == dlg_color_table[j].bg
		&& dlg_color_table[i].hilite == dlg_color_table[j].hilite) {
		fprintf(rc_file, "%s = %s\n",
			dlg_color_table[i].name,
			dlg_color_table[j].name);
		repeat = TRUE;
		break;
	    }
	}

	if (!repeat) {
	    fprintf(rc_file, "%s = %c", dlg_color_table[i].name, L_PAREN);
	    fprintf(rc_file, "%s", to_color_name(dlg_color_table[i].fg));
	    fprintf(rc_file, ",%s", to_color_name(dlg_color_table[i].bg));
	    fprintf(rc_file, ",%s", to_boolean(dlg_color_table[i].hilite));
#ifdef HAVE_RC_FILE2
	    if (dlg_color_table[i].ul || dlg_color_table[i].rv)
		fprintf(rc_file, ",%s", to_boolean(dlg_color_table[i].ul));
	    if (dlg_color_table[i].rv)
		fprintf(rc_file, ",%s", to_boolean(dlg_color_table[i].rv));
#endif /* HAVE_RC_FILE2 */
	    fprintf(rc_file, "%c\n", R_PAREN);
	}
    }
#endif /* HAVE_COLOR */
    dlg_dump_keys(rc_file);

    (void) fclose(rc_file);
}

static void
report_error(const char *filename, int line_no, const char *msg)
{
    fprintf(stderr, "%s:%d: %s\n", filename, line_no, msg);
    dlg_trace_msg("%s:%d: %s\n", filename, line_no, msg);
}

/*
 * Parse the configuration file and set up variables
 */
int
dlg_parse_rc(void)
{
    int i;
    int l = 1;
    PARSE_LINE parse;
    char str[MAX_LEN + 1];
    char *var;
    char *value;
    char *filename;
    int result = 0;
    FILE *rc_file = 0;
    char *params;

    /*
     *  At startup, dialog determines the settings to use as follows:
     *
     *  a) if the environment variable $DIALOGRC is set, its value determines
     *     the name of the configuration file.
     *
     *  b) if the file in (a) can't be found, use the file $HOME/.dialogrc
     *     as the configuration file.
     *
     *  c) if the file in (b) can't be found, try using the GLOBALRC file.
     *     Usually this will be /etc/dialogrc.
     *
     *  d) if the file in (c) cannot be found, use the compiled-in defaults.
     */

    /* try step (a) */
    if ((filename = dlg_getenv_str("DIALOGRC")) != NULL)
	rc_file = fopen(filename, "rt");

    if (rc_file == NULL) {	/* step (a) failed? */
	/* try step (b) */
	if ((filename = dlg_getenv_str("HOME")) != NULL
	    && strlen(filename) < MAX_LEN - (sizeof(DIALOGRC) + 3)) {
	    if (filename[0] == '\0' || lastch(filename) == '/')
		sprintf(str, "%s%s", filename, DIALOGRC);
	    else
		sprintf(str, "%s/%s", filename, DIALOGRC);
	    rc_file = fopen(filename = str, "rt");
	}
    }

    if (rc_file == NULL) {	/* step (b) failed? */
	/* try step (c) */
	strcpy(str, GLOBALRC);
	if ((rc_file = fopen(filename = str, "rt")) == NULL)
	    return 0;		/* step (c) failed, use default values */
    }

    DLG_TRACE(("# opened rc file \"%s\"\n", filename));
    /* Scan each line and set variables */
    while ((result == 0) && (fgets(str, MAX_LEN, rc_file) != NULL)) {
	DLG_TRACE(("#\t%s", str));
	if (*str == '\0' || lastch(str) != '\n') {
	    /* ignore rest of file if line too long */
	    report_error(filename, l, "line too long");
	    result = -1;	/* parse aborted */
	    break;
	}

	lastch(str) = '\0';
	if (begins_with(str, "bindkey", &params)) {
	    if (!dlg_parse_bindkey(params)) {
		report_error(filename, l, "invalid bindkey");
		result = -1;
	    }
	    continue;
	}
	parse = parse_line(str, &var, &value);	/* parse current line */

	switch (parse) {
	case LINE_EMPTY:	/* ignore blank lines and comments */
	    break;
	case LINE_EQUALS:
	    /* search table for matching config variable name */
	    if ((i = find_vars(var)) >= 0) {
		switch (vars[i].type) {
		case VAL_INT:
		    *((int *) vars[i].var) = atoi(value);
		    break;
		case VAL_STR:
		    if (!isquote(value[0]) || !isquote(lastch(value))
			|| strlen(value) < 2) {
			report_error(filename, l, "expected string value");
			result = -1;	/* parse aborted */
		    } else {
			/* remove the (") quotes */
			value++;
			lastch(value) = '\0';
			strcpy((char *) vars[i].var, value);
		    }
		    break;
		case VAL_BOOL:
		    if (!dlg_strcmp(value, "ON"))
			*((bool *) vars[i].var) = TRUE;
		    else if (!dlg_strcmp(value, "OFF"))
			*((bool *) vars[i].var) = FALSE;
		    else {
			report_error(filename, l, "expected boolean value");
			result = -1;	/* parse aborted */
		    }
		    break;
		}
#ifdef HAVE_COLOR
	    } else if ((i = find_color(var)) >= 0) {
		DIALOG_COLORS temp;
		if (str_to_attr(value, &temp) == -1) {
		    report_error(filename, l, "expected attribute value");
		    result = -1;	/* parse aborted */
		} else {
		    dlg_color_table[i].fg = temp.fg;
		    dlg_color_table[i].bg = temp.bg;
		    dlg_color_table[i].hilite = temp.hilite;
#ifdef HAVE_RC_FILE2
		    dlg_color_table[i].ul = temp.ul;
		    dlg_color_table[i].rv = temp.rv;
#endif /* HAVE_RC_FILE2 */
		}
	    } else {
#endif /* HAVE_COLOR */
		report_error(filename, l, "unknown variable");
		result = -1;	/* parse aborted */
	    }
	    break;
	case LINE_ERROR:
	    report_error(filename, l, "syntax error");
	    result = -1;	/* parse aborted */
	    break;
	}
	l++;			/* next line */
    }

    (void) fclose(rc_file);
    return result;
}
