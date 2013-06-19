/* $Id: reader.c,v 1.36 2012/05/26 16:05:41 tom Exp $ */

#include "defs.h"

/*  The line size must be a positive integer.  One hundred was chosen	*/
/*  because few lines in Yacc input grammars exceed 100 characters.	*/
/*  Note that if a line exceeds LINESIZE characters, the line buffer	*/
/*  will be expanded to accomodate it.					*/

#define LINESIZE 100

#define L_CURL '{'
#define R_CURL '}'

static void start_rule(bucket *bp, int s_lineno);

static char *cache;
static int cinc, cache_size;

int ntags;
static int tagmax;
static char **tag_table;

static char saw_eof;
char unionized;
char *cptr, *line;
static int linesize;

static bucket *goal;
static Value_t prec;
static int gensym;
static char last_was_action;

static int maxitems;
static bucket **pitem;

static int maxrules;
static bucket **plhs;

static size_t name_pool_size;
static char *name_pool;

char line_format[] = "#line %d \"%s\"\n";

param *lex_param;
param *parse_param;

static void
cachec(int c)
{
    assert(cinc >= 0);
    if (cinc >= cache_size)
    {
	cache_size += 256;
	cache = TREALLOC(char, cache, cache_size);
	NO_SPACE(cache);
    }
    cache[cinc] = (char)c;
    ++cinc;
}

static void
get_line(void)
{
    FILE *f = input_file;
    int c;
    int i;

    if (saw_eof || (c = getc(f)) == EOF)
    {
	if (line)
	{
	    FREE(line);
	    line = 0;
	}
	cptr = 0;
	saw_eof = 1;
	return;
    }

    if (line == 0 || linesize != (LINESIZE + 1))
    {
	if (line)
	    FREE(line);
	linesize = LINESIZE + 1;
	line = TMALLOC(char, linesize);
	NO_SPACE(line);
    }

    i = 0;
    ++lineno;
    for (;;)
    {
	line[i] = (char)c;
	if (c == '\n')
	{
	    cptr = line;
	    return;
	}
	if (++i >= linesize)
	{
	    linesize += LINESIZE;
	    line = TREALLOC(char, line, linesize);
	    NO_SPACE(line);
	}
	c = getc(f);
	if (c == EOF)
	{
	    line[i] = '\n';
	    saw_eof = 1;
	    cptr = line;
	    return;
	}
    }
}

static char *
dup_line(void)
{
    char *p, *s, *t;

    if (line == 0)
	return (0);
    s = line;
    while (*s != '\n')
	++s;
    p = TMALLOC(char, s - line + 1);
    NO_SPACE(p);

    s = line;
    t = p;
    while ((*t++ = *s++) != '\n')
	continue;
    return (p);
}

static void
skip_comment(void)
{
    char *s;

    int st_lineno = lineno;
    char *st_line = dup_line();
    char *st_cptr = st_line + (cptr - line);

    s = cptr + 2;
    for (;;)
    {
	if (*s == '*' && s[1] == '/')
	{
	    cptr = s + 2;
	    FREE(st_line);
	    return;
	}
	if (*s == '\n')
	{
	    get_line();
	    if (line == 0)
		unterminated_comment(st_lineno, st_line, st_cptr);
	    s = cptr;
	}
	else
	    ++s;
    }
}

static int
nextc(void)
{
    char *s;

    if (line == 0)
    {
	get_line();
	if (line == 0)
	    return (EOF);
    }

    s = cptr;
    for (;;)
    {
	switch (*s)
	{
	case '\n':
	    get_line();
	    if (line == 0)
		return (EOF);
	    s = cptr;
	    break;

	case ' ':
	case '\t':
	case '\f':
	case '\r':
	case '\v':
	case ',':
	case ';':
	    ++s;
	    break;

	case '\\':
	    cptr = s;
	    return ('%');

	case '/':
	    if (s[1] == '*')
	    {
		cptr = s;
		skip_comment();
		s = cptr;
		break;
	    }
	    else if (s[1] == '/')
	    {
		get_line();
		if (line == 0)
		    return (EOF);
		s = cptr;
		break;
	    }
	    /* FALLTHRU */

	default:
	    cptr = s;
	    return (*s);
	}
    }
}

/*
 * Compare keyword to cached token, treating '_' and '-' the same.  Some
 * grammars rely upon this misfeature.
 */
static int
matchec(const char *name)
{
    const char *p = cache;
    const char *q = name;
    int code = 0;	/* assume mismatch */

    while (*p != '\0' && *q != '\0')
    {
	char a = *p++;
	char b = *q++;
	if (a == '_')
	    a = '-';
	if (b == '_')
	    b = '-';
	if (a != b)
	    break;
	if (*p == '\0' && *q == '\0')
	{
	    code = 1;
	    break;
	}
    }
    return code;
}

static int
keyword(void)
{
    int c;
    char *t_cptr = cptr;

    c = *++cptr;
    if (isalpha(c))
    {
	cinc = 0;
	for (;;)
	{
	    if (isalpha(c))
	    {
		if (isupper(c))
		    c = tolower(c);
		cachec(c);
	    }
	    else if (isdigit(c)
		     || c == '-'
		     || c == '_'
		     || c == '.'
		     || c == '$')
	    {
		cachec(c);
	    }
	    else
	    {
		break;
	    }
	    c = *++cptr;
	}
	cachec(NUL);

	if (matchec("token") || matchec("term"))
	    return (TOKEN);
	if (matchec("type"))
	    return (TYPE);
	if (matchec("left"))
	    return (LEFT);
	if (matchec("right"))
	    return (RIGHT);
	if (matchec("nonassoc") || matchec("binary"))
	    return (NONASSOC);
	if (matchec("start"))
	    return (START);
	if (matchec("union"))
	    return (UNION);
	if (matchec("ident"))
	    return (IDENT);
	if (matchec("expect"))
	    return (EXPECT);
	if (matchec("expect-rr"))
	    return (EXPECT_RR);
	if (matchec("pure-parser"))
	    return (PURE_PARSER);
	if (matchec("parse-param"))
	    return (PARSE_PARAM);
	if (matchec("lex-param"))
	    return (LEX_PARAM);
	if (matchec("yacc"))
	    return (POSIX_YACC);
    }
    else
    {
	++cptr;
	if (c == L_CURL)
	    return (TEXT);
	if (c == '%' || c == '\\')
	    return (MARK);
	if (c == '<')
	    return (LEFT);
	if (c == '>')
	    return (RIGHT);
	if (c == '0')
	    return (TOKEN);
	if (c == '2')
	    return (NONASSOC);
    }
    syntax_error(lineno, line, t_cptr);
    /*NOTREACHED */
    return (-1);
}

static void
copy_ident(void)
{
    int c;
    FILE *f = output_file;

    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (c != '"')
	syntax_error(lineno, line, cptr);
    ++outline;
    fprintf(f, "#ident \"");
    for (;;)
    {
	c = *++cptr;
	if (c == '\n')
	{
	    fprintf(f, "\"\n");
	    return;
	}
	putc(c, f);
	if (c == '"')
	{
	    putc('\n', f);
	    ++cptr;
	    return;
	}
    }
}

static void
copy_text(void)
{
    int c;
    int quote;
    FILE *f = text_file;
    int need_newline = 0;
    int t_lineno = lineno;
    char *t_line = dup_line();
    char *t_cptr = t_line + (cptr - line - 2);

    if (*cptr == '\n')
    {
	get_line();
	if (line == 0)
	    unterminated_text(t_lineno, t_line, t_cptr);
    }
    if (!lflag)
	fprintf(f, line_format, lineno, input_file_name);

  loop:
    c = *cptr++;
    switch (c)
    {
    case '\n':
      next_line:
	putc('\n', f);
	need_newline = 0;
	get_line();
	if (line)
	    goto loop;
	unterminated_text(t_lineno, t_line, t_cptr);

    case '\'':
    case '"':
	{
	    int s_lineno = lineno;
	    char *s_line = dup_line();
	    char *s_cptr = s_line + (cptr - line - 1);

	    quote = c;
	    putc(c, f);
	    for (;;)
	    {
		c = *cptr++;
		putc(c, f);
		if (c == quote)
		{
		    need_newline = 1;
		    FREE(s_line);
		    goto loop;
		}
		if (c == '\n')
		    unterminated_string(s_lineno, s_line, s_cptr);
		if (c == '\\')
		{
		    c = *cptr++;
		    putc(c, f);
		    if (c == '\n')
		    {
			get_line();
			if (line == 0)
			    unterminated_string(s_lineno, s_line, s_cptr);
		    }
		}
	    }
	}

    case '/':
	putc(c, f);
	need_newline = 1;
	c = *cptr;
	if (c == '/')
	{
	    putc('*', f);
	    while ((c = *++cptr) != '\n')
	    {
		if (c == '*' && cptr[1] == '/')
		    fprintf(f, "* ");
		else
		    putc(c, f);
	    }
	    fprintf(f, "*/");
	    goto next_line;
	}
	if (c == '*')
	{
	    int c_lineno = lineno;
	    char *c_line = dup_line();
	    char *c_cptr = c_line + (cptr - line - 1);

	    putc('*', f);
	    ++cptr;
	    for (;;)
	    {
		c = *cptr++;
		putc(c, f);
		if (c == '*' && *cptr == '/')
		{
		    putc('/', f);
		    ++cptr;
		    FREE(c_line);
		    goto loop;
		}
		if (c == '\n')
		{
		    get_line();
		    if (line == 0)
			unterminated_comment(c_lineno, c_line, c_cptr);
		}
	    }
	}
	need_newline = 1;
	goto loop;

    case '%':
    case '\\':
	if (*cptr == R_CURL)
	{
	    if (need_newline)
		putc('\n', f);
	    ++cptr;
	    FREE(t_line);
	    return;
	}
	/* FALLTHRU */

    default:
	putc(c, f);
	need_newline = 1;
	goto loop;
    }
}

static void
puts_both(const char *s)
{
    fputs(s, text_file);
    if (dflag)
	fputs(s, union_file);
}

static void
putc_both(int c)
{
    putc(c, text_file);
    if (dflag)
	putc(c, union_file);
}

static void
copy_union(void)
{
    int c;
    int quote;
    int depth;
    int u_lineno = lineno;
    char *u_line = dup_line();
    char *u_cptr = u_line + (cptr - line - 6);

    if (unionized)
	over_unionized(cptr - 6);
    unionized = 1;

    if (!lflag)
	fprintf(text_file, line_format, lineno, input_file_name);

    puts_both("#ifdef YYSTYPE\n");
    puts_both("#undef  YYSTYPE_IS_DECLARED\n");
    puts_both("#define YYSTYPE_IS_DECLARED 1\n");
    puts_both("#endif\n");
    puts_both("#ifndef YYSTYPE_IS_DECLARED\n");
    puts_both("#define YYSTYPE_IS_DECLARED 1\n");
    puts_both("typedef union");

    depth = 0;
  loop:
    c = *cptr++;
    putc_both(c);
    switch (c)
    {
    case '\n':
      next_line:
	get_line();
	if (line == 0)
	    unterminated_union(u_lineno, u_line, u_cptr);
	goto loop;

    case L_CURL:
	++depth;
	goto loop;

    case R_CURL:
	if (--depth == 0)
	{
	    puts_both(" YYSTYPE;\n");
	    puts_both("#endif /* !YYSTYPE_IS_DECLARED */\n");
	    FREE(u_line);
	    return;
	}
	goto loop;

    case '\'':
    case '"':
	{
	    int s_lineno = lineno;
	    char *s_line = dup_line();
	    char *s_cptr = s_line + (cptr - line - 1);

	    quote = c;
	    for (;;)
	    {
		c = *cptr++;
		putc_both(c);
		if (c == quote)
		{
		    FREE(s_line);
		    goto loop;
		}
		if (c == '\n')
		    unterminated_string(s_lineno, s_line, s_cptr);
		if (c == '\\')
		{
		    c = *cptr++;
		    putc_both(c);
		    if (c == '\n')
		    {
			get_line();
			if (line == 0)
			    unterminated_string(s_lineno, s_line, s_cptr);
		    }
		}
	    }
	}

    case '/':
	c = *cptr;
	if (c == '/')
	{
	    putc_both('*');
	    while ((c = *++cptr) != '\n')
	    {
		if (c == '*' && cptr[1] == '/')
		{
		    puts_both("* ");
		}
		else
		{
		    putc_both(c);
		}
	    }
	    puts_both("*/\n");
	    goto next_line;
	}
	if (c == '*')
	{
	    int c_lineno = lineno;
	    char *c_line = dup_line();
	    char *c_cptr = c_line + (cptr - line - 1);

	    putc_both('*');
	    ++cptr;
	    for (;;)
	    {
		c = *cptr++;
		putc_both(c);
		if (c == '*' && *cptr == '/')
		{
		    putc_both('/');
		    ++cptr;
		    FREE(c_line);
		    goto loop;
		}
		if (c == '\n')
		{
		    get_line();
		    if (line == 0)
			unterminated_comment(c_lineno, c_line, c_cptr);
		}
	    }
	}
	goto loop;

    default:
	goto loop;
    }
}

/*
 * Keep a linked list of parameters
 */
static void
copy_param(int k)
{
    char *buf;
    int c;
    param *head, *p;
    int i;
    int name, type2;

    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (c != '{')
	goto out;
    cptr++;

    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (c == '}')
	goto out;

    buf = TMALLOC(char, linesize);
    NO_SPACE(buf);

    for (i = 0; (c = *cptr++) != '}'; i++)
    {
	if (c == '\0')
	    missing_brace();
	if (c == EOF)
	    unexpected_EOF();
	buf[i] = (char)c;
    }

    if (i == 0)
	goto out;

    buf[i--] = '\0';
    while (i >= 0 && isspace(UCH(buf[i])))
	buf[i--] = '\0';

    if (buf[i] == ']')
    {
	int level = 1;
	while (i >= 0 && level > 0 && buf[i] != '[')
	{
	    if (buf[i] == ']')
		++level;
	    else if (buf[i] == '[')
		--level;
	    i--;
	}
	if (i <= 0)
	    unexpected_EOF();
	type2 = i--;
    }
    else
    {
	type2 = i + 1;
    }

    while (i >= 0 && (isalnum(UCH(buf[i])) ||
		      UCH(buf[i]) == '_'))
	i--;

    if (!isspace(UCH(buf[i])) && buf[i] != '*')
	goto out;

    name = i + 1;

    p = TMALLOC(param, 1);
    NO_SPACE(p);

    p->type2 = strdup(buf + type2);
    NO_SPACE(p->type2);

    buf[type2] = '\0';

    p->name = strdup(buf + name);
    NO_SPACE(p->name);

    buf[name] = '\0';
    p->type = buf;

    if (k == LEX_PARAM)
	head = lex_param;
    else
	head = parse_param;

    if (head != NULL)
    {
	while (head->next)
	    head = head->next;
	head->next = p;
    }
    else
    {
	if (k == LEX_PARAM)
	    lex_param = p;
	else
	    parse_param = p;
    }
    p->next = NULL;
    return;

  out:
    syntax_error(lineno, line, cptr);
}

static int
hexval(int c)
{
    if (c >= '0' && c <= '9')
	return (c - '0');
    if (c >= 'A' && c <= 'F')
	return (c - 'A' + 10);
    if (c >= 'a' && c <= 'f')
	return (c - 'a' + 10);
    return (-1);
}

static bucket *
get_literal(void)
{
    int c, quote;
    int i;
    int n;
    char *s;
    bucket *bp;
    int s_lineno = lineno;
    char *s_line = dup_line();
    char *s_cptr = s_line + (cptr - line);

    quote = *cptr++;
    cinc = 0;
    for (;;)
    {
	c = *cptr++;
	if (c == quote)
	    break;
	if (c == '\n')
	    unterminated_string(s_lineno, s_line, s_cptr);
	if (c == '\\')
	{
	    char *c_cptr = cptr - 1;

	    c = *cptr++;
	    switch (c)
	    {
	    case '\n':
		get_line();
		if (line == 0)
		    unterminated_string(s_lineno, s_line, s_cptr);
		continue;

	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
		n = c - '0';
		c = *cptr;
		if (IS_OCTAL(c))
		{
		    n = (n << 3) + (c - '0');
		    c = *++cptr;
		    if (IS_OCTAL(c))
		    {
			n = (n << 3) + (c - '0');
			++cptr;
		    }
		}
		if (n > MAXCHAR)
		    illegal_character(c_cptr);
		c = n;
		break;

	    case 'x':
		c = *cptr++;
		n = hexval(c);
		if (n < 0 || n >= 16)
		    illegal_character(c_cptr);
		for (;;)
		{
		    c = *cptr;
		    i = hexval(c);
		    if (i < 0 || i >= 16)
			break;
		    ++cptr;
		    n = (n << 4) + i;
		    if (n > MAXCHAR)
			illegal_character(c_cptr);
		}
		c = n;
		break;

	    case 'a':
		c = 7;
		break;
	    case 'b':
		c = '\b';
		break;
	    case 'f':
		c = '\f';
		break;
	    case 'n':
		c = '\n';
		break;
	    case 'r':
		c = '\r';
		break;
	    case 't':
		c = '\t';
		break;
	    case 'v':
		c = '\v';
		break;
	    }
	}
	cachec(c);
    }
    FREE(s_line);

    n = cinc;
    s = TMALLOC(char, n);
    NO_SPACE(s);

    for (i = 0; i < n; ++i)
	s[i] = cache[i];

    cinc = 0;
    if (n == 1)
	cachec('\'');
    else
	cachec('"');

    for (i = 0; i < n; ++i)
    {
	c = UCH(s[i]);
	if (c == '\\' || c == cache[0])
	{
	    cachec('\\');
	    cachec(c);
	}
	else if (isprint(c))
	    cachec(c);
	else
	{
	    cachec('\\');
	    switch (c)
	    {
	    case 7:
		cachec('a');
		break;
	    case '\b':
		cachec('b');
		break;
	    case '\f':
		cachec('f');
		break;
	    case '\n':
		cachec('n');
		break;
	    case '\r':
		cachec('r');
		break;
	    case '\t':
		cachec('t');
		break;
	    case '\v':
		cachec('v');
		break;
	    default:
		cachec(((c >> 6) & 7) + '0');
		cachec(((c >> 3) & 7) + '0');
		cachec((c & 7) + '0');
		break;
	    }
	}
    }

    if (n == 1)
	cachec('\'');
    else
	cachec('"');

    cachec(NUL);
    bp = lookup(cache);
    bp->class = TERM;
    if (n == 1 && bp->value == UNDEFINED)
	bp->value = UCH(*s);
    FREE(s);

    return (bp);
}

static int
is_reserved(char *name)
{
    char *s;

    if (strcmp(name, ".") == 0 ||
	strcmp(name, "$accept") == 0 ||
	strcmp(name, "$end") == 0)
	return (1);

    if (name[0] == '$' && name[1] == '$' && isdigit(UCH(name[2])))
    {
	s = name + 3;
	while (isdigit(UCH(*s)))
	    ++s;
	if (*s == NUL)
	    return (1);
    }

    return (0);
}

static bucket *
get_name(void)
{
    int c;

    cinc = 0;
    for (c = *cptr; IS_IDENT(c); c = *++cptr)
	cachec(c);
    cachec(NUL);

    if (is_reserved(cache))
	used_reserved(cache);

    return (lookup(cache));
}

static Value_t
get_number(void)
{
    int c;
    Value_t n;

    n = 0;
    for (c = *cptr; isdigit(c); c = *++cptr)
	n = (Value_t) (10 * n + (c - '0'));

    return (n);
}

static char *
get_tag(void)
{
    int c;
    int i;
    char *s;
    int t_lineno = lineno;
    char *t_line = dup_line();
    char *t_cptr = t_line + (cptr - line);

    ++cptr;
    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (!isalpha(c) && c != '_' && c != '$')
	illegal_tag(t_lineno, t_line, t_cptr);

    cinc = 0;
    do
    {
	cachec(c);
	c = *++cptr;
    }
    while (IS_IDENT(c));
    cachec(NUL);

    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (c != '>')
	illegal_tag(t_lineno, t_line, t_cptr);
    ++cptr;

    for (i = 0; i < ntags; ++i)
    {
	if (strcmp(cache, tag_table[i]) == 0)
	{
	    FREE(t_line);
	    return (tag_table[i]);
	}
    }

    if (ntags >= tagmax)
    {
	tagmax += 16;
	tag_table =
	    (tag_table
	     ? TREALLOC(char *, tag_table, tagmax)
	     : TMALLOC(char *, tagmax));
	NO_SPACE(tag_table);
    }

    s = TMALLOC(char, cinc);
    NO_SPACE(s);

    strcpy(s, cache);
    tag_table[ntags] = s;
    ++ntags;
    FREE(t_line);
    return (s);
}

static void
declare_tokens(int assoc)
{
    int c;
    bucket *bp;
    Value_t value;
    char *tag = 0;

    if (assoc != TOKEN)
	++prec;

    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (c == '<')
    {
	tag = get_tag();
	c = nextc();
	if (c == EOF)
	    unexpected_EOF();
    }

    for (;;)
    {
	if (isalpha(c) || c == '_' || c == '.' || c == '$')
	    bp = get_name();
	else if (c == '\'' || c == '"')
	    bp = get_literal();
	else
	    return;

	if (bp == goal)
	    tokenized_start(bp->name);
	bp->class = TERM;

	if (tag)
	{
	    if (bp->tag && tag != bp->tag)
		retyped_warning(bp->name);
	    bp->tag = tag;
	}

	if (assoc != TOKEN)
	{
	    if (bp->prec && prec != bp->prec)
		reprec_warning(bp->name);
	    bp->assoc = (Assoc_t) assoc;
	    bp->prec = prec;
	}

	c = nextc();
	if (c == EOF)
	    unexpected_EOF();

	if (isdigit(c))
	{
	    value = get_number();
	    if (bp->value != UNDEFINED && value != bp->value)
		revalued_warning(bp->name);
	    bp->value = value;
	    c = nextc();
	    if (c == EOF)
		unexpected_EOF();
	}
    }
}

/*
 * %expect requires special handling
 * as it really isn't part of the yacc
 * grammar only a flag for yacc proper.
 */
static void
declare_expect(int assoc)
{
    int c;

    if (assoc != EXPECT && assoc != EXPECT_RR)
	++prec;

    /*
     * Stay away from nextc - doesn't
     * detect EOL and will read to EOF.
     */
    c = *++cptr;
    if (c == EOF)
	unexpected_EOF();

    for (;;)
    {
	if (isdigit(c))
	{
	    if (assoc == EXPECT)
		SRexpect = get_number();
	    else
		RRexpect = get_number();
	    break;
	}
	/*
	 * Looking for number before EOL.
	 * Spaces, tabs, and numbers are ok,
	 * words, punc., etc. are syntax errors.
	 */
	else if (c == '\n' || isalpha(c) || !isspace(c))
	{
	    syntax_error(lineno, line, cptr);
	}
	else
	{
	    c = *++cptr;
	    if (c == EOF)
		unexpected_EOF();
	}
    }
}

static void
declare_types(void)
{
    int c;
    bucket *bp;
    char *tag;

    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (c != '<')
	syntax_error(lineno, line, cptr);
    tag = get_tag();

    for (;;)
    {
	c = nextc();
	if (isalpha(c) || c == '_' || c == '.' || c == '$')
	    bp = get_name();
	else if (c == '\'' || c == '"')
	    bp = get_literal();
	else
	    return;

	if (bp->tag && tag != bp->tag)
	    retyped_warning(bp->name);
	bp->tag = tag;
    }
}

static void
declare_start(void)
{
    int c;
    bucket *bp;

    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (!isalpha(c) && c != '_' && c != '.' && c != '$')
	syntax_error(lineno, line, cptr);
    bp = get_name();
    if (bp->class == TERM)
	terminal_start(bp->name);
    if (goal && goal != bp)
	restarted_warning();
    goal = bp;
}

static void
read_declarations(void)
{
    int c, k;

    cache_size = 256;
    cache = TMALLOC(char, cache_size);
    NO_SPACE(cache);

    for (;;)
    {
	c = nextc();
	if (c == EOF)
	    unexpected_EOF();
	if (c != '%')
	    syntax_error(lineno, line, cptr);
	switch (k = keyword())
	{
	case MARK:
	    return;

	case IDENT:
	    copy_ident();
	    break;

	case TEXT:
	    copy_text();
	    break;

	case UNION:
	    copy_union();
	    break;

	case TOKEN:
	case LEFT:
	case RIGHT:
	case NONASSOC:
	    declare_tokens(k);
	    break;

	case EXPECT:
	case EXPECT_RR:
	    declare_expect(k);
	    break;

	case TYPE:
	    declare_types();
	    break;

	case START:
	    declare_start();
	    break;

	case PURE_PARSER:
	    pure_parser = 1;
	    break;

	case PARSE_PARAM:
	case LEX_PARAM:
	    copy_param(k);
	    break;

	case POSIX_YACC:
	    /* noop for bison compatibility. byacc is already designed to be posix
	     * yacc compatible. */
	    break;
	}
    }
}

static void
initialize_grammar(void)
{
    nitems = 4;
    maxitems = 300;

    pitem = TMALLOC(bucket *, maxitems);
    NO_SPACE(pitem);

    pitem[0] = 0;
    pitem[1] = 0;
    pitem[2] = 0;
    pitem[3] = 0;

    nrules = 3;
    maxrules = 100;

    plhs = TMALLOC(bucket *, maxrules);
    NO_SPACE(plhs);

    plhs[0] = 0;
    plhs[1] = 0;
    plhs[2] = 0;

    rprec = TMALLOC(Value_t, maxrules);
    NO_SPACE(rprec);

    rprec[0] = 0;
    rprec[1] = 0;
    rprec[2] = 0;

    rassoc = TMALLOC(Assoc_t, maxrules);
    NO_SPACE(rassoc);

    rassoc[0] = TOKEN;
    rassoc[1] = TOKEN;
    rassoc[2] = TOKEN;
}

static void
expand_items(void)
{
    maxitems += 300;
    pitem = TREALLOC(bucket *, pitem, maxitems);
    NO_SPACE(pitem);
}

static void
expand_rules(void)
{
    maxrules += 100;

    plhs = TREALLOC(bucket *, plhs, maxrules);
    NO_SPACE(plhs);

    rprec = TREALLOC(Value_t, rprec, maxrules);
    NO_SPACE(rprec);

    rassoc = TREALLOC(Assoc_t, rassoc, maxrules);
    NO_SPACE(rassoc);
}

static void
advance_to_start(void)
{
    int c;
    bucket *bp;
    char *s_cptr;
    int s_lineno;

    for (;;)
    {
	c = nextc();
	if (c != '%')
	    break;
	s_cptr = cptr;
	switch (keyword())
	{
	case MARK:
	    no_grammar();

	case TEXT:
	    copy_text();
	    break;

	case START:
	    declare_start();
	    break;

	default:
	    syntax_error(lineno, line, s_cptr);
	}
    }

    c = nextc();
    if (!isalpha(c) && c != '_' && c != '.' && c != '_')
	syntax_error(lineno, line, cptr);
    bp = get_name();
    if (goal == 0)
    {
	if (bp->class == TERM)
	    terminal_start(bp->name);
	goal = bp;
    }

    s_lineno = lineno;
    c = nextc();
    if (c == EOF)
	unexpected_EOF();
    if (c != ':')
	syntax_error(lineno, line, cptr);
    start_rule(bp, s_lineno);
    ++cptr;
}

static void
start_rule(bucket *bp, int s_lineno)
{
    if (bp->class == TERM)
	terminal_lhs(s_lineno);
    bp->class = NONTERM;
    if (nrules >= maxrules)
	expand_rules();
    plhs[nrules] = bp;
    rprec[nrules] = UNDEFINED;
    rassoc[nrules] = TOKEN;
}

static void
end_rule(void)
{
    int i;

    if (!last_was_action && plhs[nrules]->tag)
    {
	if (pitem[nitems - 1])
	{
	    for (i = nitems - 1; (i > 0) && pitem[i]; --i)
		continue;
	    if (pitem[i + 1] == 0 || pitem[i + 1]->tag != plhs[nrules]->tag)
		default_action_warning();
	}
	else
	{
	    default_action_warning();
	}
    }

    last_was_action = 0;
    if (nitems >= maxitems)
	expand_items();
    pitem[nitems] = 0;
    ++nitems;
    ++nrules;
}

static void
insert_empty_rule(void)
{
    bucket *bp, **bpp;

    assert(cache);
    sprintf(cache, "$$%d", ++gensym);
    bp = make_bucket(cache);
    last_symbol->next = bp;
    last_symbol = bp;
    bp->tag = plhs[nrules]->tag;
    bp->class = NONTERM;

    if ((nitems += 2) > maxitems)
	expand_items();
    bpp = pitem + nitems - 1;
    *bpp-- = bp;
    while ((bpp[0] = bpp[-1]) != 0)
	--bpp;

    if (++nrules >= maxrules)
	expand_rules();
    plhs[nrules] = plhs[nrules - 1];
    plhs[nrules - 1] = bp;
    rprec[nrules] = rprec[nrules - 1];
    rprec[nrules - 1] = 0;
    rassoc[nrules] = rassoc[nrules - 1];
    rassoc[nrules - 1] = TOKEN;
}

static void
add_symbol(void)
{
    int c;
    bucket *bp;
    int s_lineno = lineno;

    c = *cptr;
    if (c == '\'' || c == '"')
	bp = get_literal();
    else
	bp = get_name();

    c = nextc();
    if (c == ':')
    {
	end_rule();
	start_rule(bp, s_lineno);
	++cptr;
	return;
    }

    if (last_was_action)
	insert_empty_rule();
    last_was_action = 0;

    if (++nitems > maxitems)
	expand_items();
    pitem[nitems - 1] = bp;
}

static char *
after_blanks(char *s)
{
    while (*s != '\0' && isspace(UCH(*s)))
	++s;
    return s;
}

static void
copy_action(void)
{
    int c;
    int i, n;
    int depth;
    int quote;
    char *tag;
    FILE *f = action_file;
    int a_lineno = lineno;
    char *a_line = dup_line();
    char *a_cptr = a_line + (cptr - line);

    if (last_was_action)
	insert_empty_rule();
    last_was_action = 1;

    fprintf(f, "case %d:\n", nrules - 2);
    if (!lflag)
	fprintf(f, line_format, lineno, input_file_name);
    if (*cptr == '=')
	++cptr;

    /* avoid putting curly-braces in first column, to ease editing */
    if (*after_blanks(cptr) == L_CURL)
    {
	putc('\t', f);
	cptr = after_blanks(cptr);
    }

    n = 0;
    for (i = nitems - 1; pitem[i]; --i)
	++n;

    depth = 0;
  loop:
    c = *cptr;
    if (c == '$')
    {
	if (cptr[1] == '<')
	{
	    int d_lineno = lineno;
	    char *d_line = dup_line();
	    char *d_cptr = d_line + (cptr - line);

	    ++cptr;
	    tag = get_tag();
	    c = *cptr;
	    if (c == '$')
	    {
		fprintf(f, "yyval.%s", tag);
		++cptr;
		FREE(d_line);
		goto loop;
	    }
	    else if (isdigit(c))
	    {
		i = get_number();
		if (i > n)
		    dollar_warning(d_lineno, i);
		fprintf(f, "yystack.l_mark[%d].%s", i - n, tag);
		FREE(d_line);
		goto loop;
	    }
	    else if (c == '-' && isdigit(UCH(cptr[1])))
	    {
		++cptr;
		i = -get_number() - n;
		fprintf(f, "yystack.l_mark[%d].%s", i, tag);
		FREE(d_line);
		goto loop;
	    }
	    else
		dollar_error(d_lineno, d_line, d_cptr);
	}
	else if (cptr[1] == '$')
	{
	    if (ntags)
	    {
		tag = plhs[nrules]->tag;
		if (tag == 0)
		    untyped_lhs();
		fprintf(f, "yyval.%s", tag);
	    }
	    else
		fprintf(f, "yyval");
	    cptr += 2;
	    goto loop;
	}
	else if (isdigit(UCH(cptr[1])))
	{
	    ++cptr;
	    i = get_number();
	    if (ntags)
	    {
		if (i <= 0 || i > n)
		    unknown_rhs(i);
		tag = pitem[nitems + i - n - 1]->tag;
		if (tag == 0)
		    untyped_rhs(i, pitem[nitems + i - n - 1]->name);
		fprintf(f, "yystack.l_mark[%d].%s", i - n, tag);
	    }
	    else
	    {
		if (i > n)
		    dollar_warning(lineno, i);
		fprintf(f, "yystack.l_mark[%d]", i - n);
	    }
	    goto loop;
	}
	else if (cptr[1] == '-')
	{
	    cptr += 2;
	    i = get_number();
	    if (ntags)
		unknown_rhs(-i);
	    fprintf(f, "yystack.l_mark[%d]", -i - n);
	    goto loop;
	}
    }
    if (isalpha(c) || c == '_' || c == '$')
    {
	do
	{
	    putc(c, f);
	    c = *++cptr;
	}
	while (isalnum(c) || c == '_' || c == '$');
	goto loop;
    }
    putc(c, f);
    ++cptr;
    switch (c)
    {
    case '\n':
      next_line:
	get_line();
	if (line)
	    goto loop;
	unterminated_action(a_lineno, a_line, a_cptr);

    case ';':
	if (depth > 0)
	    goto loop;
	fprintf(f, "\nbreak;\n");
	free(a_line);
	return;

    case L_CURL:
	++depth;
	goto loop;

    case R_CURL:
	if (--depth > 0)
	    goto loop;
	fprintf(f, "\nbreak;\n");
	free(a_line);
	return;

    case '\'':
    case '"':
	{
	    int s_lineno = lineno;
	    char *s_line = dup_line();
	    char *s_cptr = s_line + (cptr - line - 1);

	    quote = c;
	    for (;;)
	    {
		c = *cptr++;
		putc(c, f);
		if (c == quote)
		{
		    FREE(s_line);
		    goto loop;
		}
		if (c == '\n')
		    unterminated_string(s_lineno, s_line, s_cptr);
		if (c == '\\')
		{
		    c = *cptr++;
		    putc(c, f);
		    if (c == '\n')
		    {
			get_line();
			if (line == 0)
			    unterminated_string(s_lineno, s_line, s_cptr);
		    }
		}
	    }
	}

    case '/':
	c = *cptr;
	if (c == '/')
	{
	    putc('*', f);
	    while ((c = *++cptr) != '\n')
	    {
		if (c == '*' && cptr[1] == '/')
		    fprintf(f, "* ");
		else
		    putc(c, f);
	    }
	    fprintf(f, "*/\n");
	    goto next_line;
	}
	if (c == '*')
	{
	    int c_lineno = lineno;
	    char *c_line = dup_line();
	    char *c_cptr = c_line + (cptr - line - 1);

	    putc('*', f);
	    ++cptr;
	    for (;;)
	    {
		c = *cptr++;
		putc(c, f);
		if (c == '*' && *cptr == '/')
		{
		    putc('/', f);
		    ++cptr;
		    FREE(c_line);
		    goto loop;
		}
		if (c == '\n')
		{
		    get_line();
		    if (line == 0)
			unterminated_comment(c_lineno, c_line, c_cptr);
		}
	    }
	}
	goto loop;

    default:
	goto loop;
    }
}

static int
mark_symbol(void)
{
    int c;
    bucket *bp = NULL;

    c = cptr[1];
    if (c == '%' || c == '\\')
    {
	cptr += 2;
	return (1);
    }

    if (c == '=')
	cptr += 2;
    else if ((c == 'p' || c == 'P') &&
	     ((c = cptr[2]) == 'r' || c == 'R') &&
	     ((c = cptr[3]) == 'e' || c == 'E') &&
	     ((c = cptr[4]) == 'c' || c == 'C') &&
	     ((c = cptr[5], !IS_IDENT(c))))
	cptr += 5;
    else
	syntax_error(lineno, line, cptr);

    c = nextc();
    if (isalpha(c) || c == '_' || c == '.' || c == '$')
	bp = get_name();
    else if (c == '\'' || c == '"')
	bp = get_literal();
    else
    {
	syntax_error(lineno, line, cptr);
	/*NOTREACHED */
    }

    if (rprec[nrules] != UNDEFINED && bp->prec != rprec[nrules])
	prec_redeclared();

    rprec[nrules] = bp->prec;
    rassoc[nrules] = bp->assoc;
    return (0);
}

static void
read_grammar(void)
{
    int c;

    initialize_grammar();
    advance_to_start();

    for (;;)
    {
	c = nextc();
	if (c == EOF)
	    break;
	if (isalpha(c)
	    || c == '_'
	    || c == '.'
	    || c == '$'
	    || c == '\''
	    || c == '"')
	    add_symbol();
	else if (c == L_CURL || c == '=')
	    copy_action();
	else if (c == '|')
	{
	    end_rule();
	    start_rule(plhs[nrules - 1], 0);
	    ++cptr;
	}
	else if (c == '%')
	{
	    if (mark_symbol())
		break;
	}
	else
	    syntax_error(lineno, line, cptr);
    }
    end_rule();
}

static void
free_tags(void)
{
    int i;

    if (tag_table == 0)
	return;

    for (i = 0; i < ntags; ++i)
    {
	assert(tag_table[i]);
	FREE(tag_table[i]);
    }
    FREE(tag_table);
}

static void
pack_names(void)
{
    bucket *bp;
    char *p, *s, *t;

    name_pool_size = 13;	/* 13 == sizeof("$end") + sizeof("$accept") */
    for (bp = first_symbol; bp; bp = bp->next)
	name_pool_size += strlen(bp->name) + 1;

    name_pool = TMALLOC(char, name_pool_size);
    NO_SPACE(name_pool);

    strcpy(name_pool, "$accept");
    strcpy(name_pool + 8, "$end");
    t = name_pool + 13;
    for (bp = first_symbol; bp; bp = bp->next)
    {
	p = t;
	s = bp->name;
	while ((*t++ = *s++) != 0)
	    continue;
	FREE(bp->name);
	bp->name = p;
    }
}

static void
check_symbols(void)
{
    bucket *bp;

    if (goal->class == UNKNOWN)
	undefined_goal(goal->name);

    for (bp = first_symbol; bp; bp = bp->next)
    {
	if (bp->class == UNKNOWN)
	{
	    undefined_symbol_warning(bp->name);
	    bp->class = TERM;
	}
    }
}

static void
protect_string(char *src, char **des)
{
    unsigned len;
    char *s;
    char *d;

    *des = src;
    if (src)
    {
	len = 1;
	s = src;
	while (*s)
	{
	    if ('\\' == *s || '"' == *s)
		len++;
	    s++;
	    len++;
	}

	*des = d = TMALLOC(char, len);
	NO_SPACE(d);

	s = src;
	while (*s)
	{
	    if ('\\' == *s || '"' == *s)
		*d++ = '\\';
	    *d++ = *s++;
	}
	*d = '\0';
    }
}

static void
pack_symbols(void)
{
    bucket *bp;
    bucket **v;
    Value_t i, j, k, n;

    nsyms = 2;
    ntokens = 1;
    for (bp = first_symbol; bp; bp = bp->next)
    {
	++nsyms;
	if (bp->class == TERM)
	    ++ntokens;
    }
    start_symbol = (Value_t) ntokens;
    nvars = nsyms - ntokens;

    symbol_name = TMALLOC(char *, nsyms);
    NO_SPACE(symbol_name);

    symbol_value = TMALLOC(Value_t, nsyms);
    NO_SPACE(symbol_value);

    symbol_prec = TMALLOC(short, nsyms);
    NO_SPACE(symbol_prec);

    symbol_assoc = TMALLOC(char, nsyms);
    NO_SPACE(symbol_assoc);

    v = TMALLOC(bucket *, nsyms);
    NO_SPACE(v);

    v[0] = 0;
    v[start_symbol] = 0;

    i = 1;
    j = (Value_t) (start_symbol + 1);
    for (bp = first_symbol; bp; bp = bp->next)
    {
	if (bp->class == TERM)
	    v[i++] = bp;
	else
	    v[j++] = bp;
    }
    assert(i == ntokens && j == nsyms);

    for (i = 1; i < ntokens; ++i)
	v[i]->index = i;

    goal->index = (Index_t) (start_symbol + 1);
    k = (Value_t) (start_symbol + 2);
    while (++i < nsyms)
	if (v[i] != goal)
	{
	    v[i]->index = k;
	    ++k;
	}

    goal->value = 0;
    k = 1;
    for (i = (Value_t) (start_symbol + 1); i < nsyms; ++i)
    {
	if (v[i] != goal)
	{
	    v[i]->value = k;
	    ++k;
	}
    }

    k = 0;
    for (i = 1; i < ntokens; ++i)
    {
	n = v[i]->value;
	if (n > 256)
	{
	    for (j = k++; j > 0 && symbol_value[j - 1] > n; --j)
		symbol_value[j] = symbol_value[j - 1];
	    symbol_value[j] = n;
	}
    }

    assert(v[1] != 0);

    if (v[1]->value == UNDEFINED)
	v[1]->value = 256;

    j = 0;
    n = 257;
    for (i = 2; i < ntokens; ++i)
    {
	if (v[i]->value == UNDEFINED)
	{
	    while (j < k && n == symbol_value[j])
	    {
		while (++j < k && n == symbol_value[j])
		    continue;
		++n;
	    }
	    v[i]->value = n;
	    ++n;
	}
    }

    symbol_name[0] = name_pool + 8;
    symbol_value[0] = 0;
    symbol_prec[0] = 0;
    symbol_assoc[0] = TOKEN;
    for (i = 1; i < ntokens; ++i)
    {
	symbol_name[i] = v[i]->name;
	symbol_value[i] = v[i]->value;
	symbol_prec[i] = v[i]->prec;
	symbol_assoc[i] = v[i]->assoc;
    }
    symbol_name[start_symbol] = name_pool;
    symbol_value[start_symbol] = -1;
    symbol_prec[start_symbol] = 0;
    symbol_assoc[start_symbol] = TOKEN;
    for (++i; i < nsyms; ++i)
    {
	k = v[i]->index;
	symbol_name[k] = v[i]->name;
	symbol_value[k] = v[i]->value;
	symbol_prec[k] = v[i]->prec;
	symbol_assoc[k] = v[i]->assoc;
    }

    if (gflag)
    {
	symbol_pname = TMALLOC(char *, nsyms);
	NO_SPACE(symbol_pname);

	for (i = 0; i < nsyms; ++i)
	    protect_string(symbol_name[i], &(symbol_pname[i]));
    }

    FREE(v);
}

static void
pack_grammar(void)
{
    int i;
    Value_t j;
    Assoc_t assoc;
    Value_t prec2;

    ritem = TMALLOC(Value_t, nitems);
    NO_SPACE(ritem);

    rlhs = TMALLOC(Value_t, nrules);
    NO_SPACE(rlhs);

    rrhs = TMALLOC(Value_t, nrules + 1);
    NO_SPACE(rrhs);

    rprec = TREALLOC(Value_t, rprec, nrules);
    NO_SPACE(rprec);

    rassoc = TREALLOC(Assoc_t, rassoc, nrules);
    NO_SPACE(rassoc);

    ritem[0] = -1;
    ritem[1] = goal->index;
    ritem[2] = 0;
    ritem[3] = -2;
    rlhs[0] = 0;
    rlhs[1] = 0;
    rlhs[2] = start_symbol;
    rrhs[0] = 0;
    rrhs[1] = 0;
    rrhs[2] = 1;

    j = 4;
    for (i = 3; i < nrules; ++i)
    {
	rlhs[i] = plhs[i]->index;
	rrhs[i] = j;
	assoc = TOKEN;
	prec2 = 0;
	while (pitem[j])
	{
	    ritem[j] = pitem[j]->index;
	    if (pitem[j]->class == TERM)
	    {
		prec2 = pitem[j]->prec;
		assoc = pitem[j]->assoc;
	    }
	    ++j;
	}
	ritem[j] = (Value_t) - i;
	++j;
	if (rprec[i] == UNDEFINED)
	{
	    rprec[i] = prec2;
	    rassoc[i] = assoc;
	}
    }
    rrhs[i] = j;

    FREE(plhs);
    FREE(pitem);
}

static void
print_grammar(void)
{
    int i, k;
    size_t j, spacing = 0;
    FILE *f = verbose_file;

    if (!vflag)
	return;

    k = 1;
    for (i = 2; i < nrules; ++i)
    {
	if (rlhs[i] != rlhs[i - 1])
	{
	    if (i != 2)
		fprintf(f, "\n");
	    fprintf(f, "%4d  %s :", i - 2, symbol_name[rlhs[i]]);
	    spacing = strlen(symbol_name[rlhs[i]]) + 1;
	}
	else
	{
	    fprintf(f, "%4d  ", i - 2);
	    j = spacing;
	    while (j-- != 0)
		putc(' ', f);
	    putc('|', f);
	}

	while (ritem[k] >= 0)
	{
	    fprintf(f, " %s", symbol_name[ritem[k]]);
	    ++k;
	}
	++k;
	putc('\n', f);
    }
}

void
reader(void)
{
    write_section(code_file, banner);
    create_symbol_table();
    read_declarations();
    read_grammar();
    free_symbol_table();
    free_tags();
    pack_names();
    check_symbols();
    pack_symbols();
    pack_grammar();
    free_symbols();
    print_grammar();
}

#ifdef NO_LEAKS
static param *
free_declarations(param * list)
{
    while (list != 0)
    {
	param *next = list->next;
	free(list->type);
	free(list->name);
	free(list->type2);
	free(list);
	list = next;
    }
    return list;
}

void
reader_leaks(void)
{
    lex_param = free_declarations(lex_param);
    parse_param = free_declarations(parse_param);

    DO_FREE(line);
    DO_FREE(rrhs);
    DO_FREE(rlhs);
    DO_FREE(rprec);
    DO_FREE(ritem);
    DO_FREE(rassoc);
    DO_FREE(cache);
    DO_FREE(name_pool);
    DO_FREE(symbol_name);
    DO_FREE(symbol_prec);
    DO_FREE(symbol_assoc);
    DO_FREE(symbol_value);
}
#endif
