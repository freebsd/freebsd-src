/* routines for printing error messages  */

#include "defs.h"


fatal(msg)
char *msg;
{
    fprintf(stderr, "%s: f - %s\n", myname, msg);
    done(2);
}


no_space()
{
    fprintf(stderr, "%s: f - out of space\n", myname);
    done(2);
}


open_error(filename)
char *filename;
{
    fprintf(stderr, "%s: f - cannot open \"%s\"\n", myname, filename);
    done(2);
}


unexpected_EOF()
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unexpected end-of-file\n",
	    myname, lineno, input_file_name);
    done(1);
}


print_pos(st_line, st_cptr)
char *st_line;
char *st_cptr;
{
    register char *s;

    if (st_line == 0) return;
    for (s = st_line; *s != '\n'; ++s)
    {
	if (isprint(*s) || *s == '\t')
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


syntax_error(st_lineno, st_line, st_cptr)
int st_lineno;
char *st_line;
char *st_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", syntax error\n",
	    myname, st_lineno, input_file_name);
    print_pos(st_line, st_cptr);
    done(1);
}


unterminated_comment(c_lineno, c_line, c_cptr)
int c_lineno;
char *c_line;
char *c_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unmatched /*\n",
	    myname, c_lineno, input_file_name);
    print_pos(c_line, c_cptr);
    done(1);
}


unterminated_string(s_lineno, s_line, s_cptr)
int s_lineno;
char *s_line;
char *s_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unterminated string\n",
	    myname, s_lineno, input_file_name);
    print_pos(s_line, s_cptr);
    done(1);
}


unterminated_text(t_lineno, t_line, t_cptr)
int t_lineno;
char *t_line;
char *t_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unmatched %%{\n",
	    myname, t_lineno, input_file_name);
    print_pos(t_line, t_cptr);
    done(1);
}


unterminated_union(u_lineno, u_line, u_cptr)
int u_lineno;
char *u_line;
char *u_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unterminated %%union \
declaration\n", myname, u_lineno, input_file_name);
    print_pos(u_line, u_cptr);
    done(1);
}


over_unionized(u_cptr)
char *u_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", too many %%union \
declarations\n", myname, lineno, input_file_name);
    print_pos(line, u_cptr);
    done(1);
}


illegal_tag(t_lineno, t_line, t_cptr)
int t_lineno;
char *t_line;
char *t_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", illegal tag\n",
	    myname, t_lineno, input_file_name);
    print_pos(t_line, t_cptr);
    done(1);
}


illegal_character(c_cptr)
char *c_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", illegal character\n",
	    myname, lineno, input_file_name);
    print_pos(line, c_cptr);
    done(1);
}


used_reserved(s)
char *s;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", illegal use of reserved symbol \
%s\n", myname, lineno, input_file_name, s);
    done(1);
}


tokenized_start(s)
char *s;
{
     fprintf(stderr, "%s: e - line %d of \"%s\", the start symbol %s cannot be \
declared to be a token\n", myname, lineno, input_file_name, s);
     done(1);
}


retyped_warning(s)
char *s;
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the type of %s has been \
redeclared\n", myname, lineno, input_file_name, s);
}


reprec_warning(s)
char *s;
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the precedence of %s has been \
redeclared\n", myname, lineno, input_file_name, s);
}


revalued_warning(s)
char *s;
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the value of %s has been \
redeclared\n", myname, lineno, input_file_name, s);
}


terminal_start(s)
char *s;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", the start symbol %s is a \
token\n", myname, lineno, input_file_name, s);
    done(1);
}


restarted_warning()
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the start symbol has been \
redeclared\n", myname, lineno, input_file_name);
}


no_grammar()
{
    fprintf(stderr, "%s: e - line %d of \"%s\", no grammar has been \
specified\n", myname, lineno, input_file_name);
    done(1);
}


terminal_lhs(s_lineno)
int s_lineno;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", a token appears on the lhs \
of a production\n", myname, s_lineno, input_file_name);
    done(1);
}


prec_redeclared()
{
    fprintf(stderr, "%s: w - line %d of  \"%s\", conflicting %%prec \
specifiers\n", myname, lineno, input_file_name);
}


unterminated_action(a_lineno, a_line, a_cptr)
int a_lineno;
char *a_line;
char *a_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", unterminated action\n",
	    myname, a_lineno, input_file_name);
    print_pos(a_line, a_cptr);
    done(1);
}


dollar_warning(a_lineno, i)
int a_lineno;
int i;
{
    fprintf(stderr, "%s: w - line %d of \"%s\", $%d references beyond the \
end of the current rule\n", myname, a_lineno, input_file_name, i);
}


dollar_error(a_lineno, a_line, a_cptr)
int a_lineno;
char *a_line;
char *a_cptr;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", illegal $-name\n",
	    myname, a_lineno, input_file_name);
    print_pos(a_line, a_cptr);
    done(1);
}


untyped_lhs()
{
    fprintf(stderr, "%s: e - line %d of \"%s\", $$ is untyped\n",
	    myname, lineno, input_file_name);
    done(1);
}


untyped_rhs(i, s)
int i;
char *s;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", $%d (%s) is untyped\n",
	    myname, lineno, input_file_name, i, s);
    done(1);
}


unknown_rhs(i)
int i;
{
    fprintf(stderr, "%s: e - line %d of \"%s\", $%d is untyped\n",
	    myname, lineno, input_file_name, i);
    done(1);
}


default_action_warning()
{
    fprintf(stderr, "%s: w - line %d of \"%s\", the default action assigns an \
undefined value to $$\n", myname, lineno, input_file_name);
}


undefined_goal(s)
char *s;
{
    fprintf(stderr, "%s: e - the start symbol %s is undefined\n", myname, s);
    done(1);
}


undefined_symbol_warning(s)
char *s;
{
    fprintf(stderr, "%s: w - the symbol %s is undefined\n", myname, s);
}
