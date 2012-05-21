/* $Id: error.c,v 1.9 2011/09/05 23:27:43 tom Exp $ */

/* routines for printing error messages  */

#include "defs.h"

void
fatal(const char *msg)
{
    fprintf(stderr, "%s: f - %s\n", myname, msg);
    done(2);
}

void
no_space(void)
{
    fprintf(stderr, "%s: f - out of space\n", myname);
    done(2);
}

void
open_error(const char *filename)
{
    fprintf(stderr, "%s: f - cannot open \"%s\"\n", myname, filename);
    done(2);
}

void
missing_brace(void)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", missing '}'\n",
	    myname, lineno, input_file_name);
    done(1);
}

void
unexpected_EOF(void)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unexpected end-of-file\n",
	    myname, lineno, input_file_name);
    done(1);
}

static void
print_pos(char *st_line, char *st_cptr)
{
    char *s;

    if (st_line == 0)
	return;
    for (s = st_line; *s != '\n'; ++s)
    {
	if (isprint(UCH(*s)) || *s == '\t')
	    putc(*s, stderr);
	else
	    putc('?', stderr);
    }
    putc('\n', stderr);
    for (s = st_line; s < st_cptr; ++s)
    {
	if (*s == '\t')
	    putc('\t', stderr);
	else
	    putc(' ', stderr);
    }
    putc('^', stderr);
    putc('\n', stderr);
}

void
syntax_error(int st_lineno, char *st_line, char *st_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", syntax error\n",
	    myname, st_lineno, input_file_name);
    print_pos(st_line, st_cptr);
    done(1);
}

void
unterminated_comment(int c_lineno, char *c_line, char *c_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unmatched /*\n",
	    myname, c_lineno, input_file_name);
    print_pos(c_line, c_cptr);
    done(1);
}

void
unterminated_string(int s_lineno, char *s_line, char *s_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unterminated string\n",
	    myname, s_lineno, input_file_name);
    print_pos(s_line, s_cptr);
    done(1);
}

void
unterminated_text(int t_lineno, char *t_line, char *t_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unmatched %%{\n",
	    myname, t_lineno, input_file_name);
    print_pos(t_line, t_cptr);
    done(1);
}

void
unterminated_union(int u_lineno, char *u_line, char *u_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unterminated %%union \
declaration\n", myname, u_lineno, input_file_name);
    print_pos(u_line, u_cptr);
    done(1);
}

void
over_unionized(char *u_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", too many %%union \
declarations\n", myname, lineno, input_file_name);
    print_pos(line, u_cptr);
    done(1);
}

void
illegal_tag(int t_lineno, char *t_line, char *t_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", illegal tag\n",
	    myname, t_lineno, input_file_name);
    print_pos(t_line, t_cptr);
    done(1);
}

void
illegal_character(char *c_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", illegal character\n",
	    myname, lineno, input_file_name);
    print_pos(line, c_cptr);
    done(1);
}

void
used_reserved(char *s)
{
    fprintf(stderr,
	    "%s: e - line %d of \"%s\", illegal use of reserved symbol \
%s\n", myname, lineno, input_file_name, s);
    done(1);
}

void
tokenized_start(char *s)
{
    fprintf(stderr,
	    "%s: e - line %d of \"%s\", the start symbol %s cannot be \
declared to be a token\n", myname, lineno, input_file_name, s);
    done(1);
}

void
retyped_warning(char *s)
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the type of %s has been \
redeclared\n", myname, lineno, input_file_name, s);
}

void
reprec_warning(char *s)
{
    fprintf(stderr,
	    "%s: w - line %d of \"%s\", the precedence of %s has been \
redeclared\n", myname, lineno, input_file_name, s);
}

void
revalued_warning(char *s)
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the value of %s has been \
redeclared\n", myname, lineno, input_file_name, s);
}

void
terminal_start(char *s)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", the start symbol %s is a \
token\n", myname, lineno, input_file_name, s);
    done(1);
}

void
restarted_warning(void)
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the start symbol has been \
redeclared\n", myname, lineno, input_file_name);
}

void
no_grammar(void)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", no grammar has been \
specified\n", myname, lineno, input_file_name);
    done(1);
}

void
terminal_lhs(int s_lineno)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", a token appears on the lhs \
of a production\n", myname, s_lineno, input_file_name);
    done(1);
}

void
prec_redeclared(void)
{
    fprintf(stderr, "%s: w - line %d of  \"%s\", conflicting %%prec \
specifiers\n", myname, lineno, input_file_name);
}

void
unterminated_action(int a_lineno, char *a_line, char *a_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unterminated action\n",
	    myname, a_lineno, input_file_name);
    print_pos(a_line, a_cptr);
    done(1);
}

void
dollar_warning(int a_lineno, int i)
{
    fprintf(stderr, "%s: w - line %d of \"%s\", $%d references beyond the \
end of the current rule\n", myname, a_lineno, input_file_name, i);
}

void
dollar_error(int a_lineno, char *a_line, char *a_cptr)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", illegal $-name\n",
	    myname, a_lineno, input_file_name);
    print_pos(a_line, a_cptr);
    done(1);
}

void
untyped_lhs(void)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", $$ is untyped\n",
	    myname, lineno, input_file_name);
    done(1);
}

void
untyped_rhs(int i, char *s)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", $%d (%s) is untyped\n",
	    myname, lineno, input_file_name, i, s);
    done(1);
}

void
unknown_rhs(int i)
{
    fprintf(stderr, "%s: e - line %d of \"%s\", $%d is untyped\n",
	    myname, lineno, input_file_name, i);
    done(1);
}

void
default_action_warning(void)
{
    fprintf(stderr,
	    "%s: w - line %d of \"%s\", the default action assigns an \
undefined value to $$\n", myname, lineno, input_file_name);
}

void
undefined_goal(char *s)
{
    fprintf(stderr, "%s: e - the start symbol %s is undefined\n", myname, s);
    done(1);
}

void
undefined_symbol_warning(char *s)
{
    fprintf(stderr, "%s: w - the symbol %s is undefined\n", myname, s);
}
