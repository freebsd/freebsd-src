/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)output.c	5.7 (Berkeley) 5/24/93";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <string.h>
#include "defs.h"

static int nvectors;
static int nentries;
static short **froms;
static short **tos;
static short *tally;
static short *width;
static short *state_count;
static short *order;
static short *base;
static short *pos;
static int maxtable;
static short *table;
static short *check;
static int lowzero;
static int high;

static int default_goto(int);
static void free_itemsets(void);
static void free_reductions(void);
static void free_shifts(void);
static void goto_actions(void);
static int is_C_identifier(char *);
static int matching_vector(int);
static void output_actions(void);
static void output_base(void);
static void output_check(void);
static void output_debug(void);
static void output_defines(void);
static void output_prefix(void);
static void output_rule_data(void);
static void output_semantic_actions(void);
static void output_stored_text(void);
static void output_stype(void);
static void output_table(void);
static void output_trailing_text(void);
static void output_yydefred(void);
static void pack_table(void);
static int pack_vector(int);
static void save_column(int, int);
static void sort_actions(void);
static void token_actions(void);
static int increase_maxtable(int);

static const char line_format[] = "#line %d \"%s\"\n";


void
output()
{
    free_itemsets();
    free_shifts();
    free_reductions();
    output_prefix();
    output_stored_text();
    output_defines();
    output_rule_data();
    output_yydefred();
    output_actions();
    free_parser();
    output_debug();
    output_stype();
    if (rflag) write_section(tables);
    write_section(header);
    output_trailing_text();
    write_section(body);
    output_semantic_actions();
    write_section(trailer);
}


static void
output_prefix()
{
    if (symbol_prefix == NULL)
	symbol_prefix = "yy";
    else
    {
	++outline;
	fprintf(code_file, "#define yyparse %sparse\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yylex %slex\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyerror %serror\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yychar %schar\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyval %sval\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yylval %slval\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yydebug %sdebug\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yynerrs %snerrs\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyerrflag %serrflag\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyss %sss\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyssp %sssp\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyvs %svs\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyvsp %svsp\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yylhs %slhs\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yylen %slen\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yydefred %sdefred\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yydgoto %sdgoto\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yysindex %ssindex\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyrindex %srindex\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yygindex %sgindex\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yytable %stable\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yycheck %scheck\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyname %sname\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yyrule %srule\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yysslim %ssslim\n", symbol_prefix);
	++outline;
	fprintf(code_file, "#define yystacksize %sstacksize\n", symbol_prefix);
    }
    ++outline;
    fprintf(code_file, "#define YYPREFIX \"%s\"\n", symbol_prefix);
}


static void
output_rule_data()
{
    int i;
    int j;


    fprintf(output_file, "const short %slhs[] = {%42d,", symbol_prefix,
	    symbol_value[start_symbol]);

    j = 10;
    for (i = 3; i < nrules; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
        else
	    ++j;

        fprintf(output_file, "%5d,", symbol_value[rlhs[i]]);
    }
    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");

    fprintf(output_file, "const short %slen[] = {%42d,", symbol_prefix, 2);

    j = 10;
    for (i = 3; i < nrules; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
	else
	  j++;

        fprintf(output_file, "%5d,", rrhs[i + 1] - rrhs[i] - 1);
    }
    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");
}


static void
output_yydefred()
{
    int i, j;

    fprintf(output_file, "const short %sdefred[] = {%39d,", symbol_prefix,
	    (defred[0] ? defred[0] - 2 : 0));

    j = 10;
    for (i = 1; i < nstates; i++)
    {
	if (j < 10)
	    ++j;
	else
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}

	fprintf(output_file, "%5d,", (defred[i] ? defred[i] - 2 : 0));
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");
}


static void
output_actions()
{
    nvectors = 2*nstates + nvars;

    froms = NEW2(nvectors, short *);
    tos = NEW2(nvectors, short *);
    tally = NEW2(nvectors, short);
    width = NEW2(nvectors, short);

    token_actions();
    FREE(lookaheads);
    FREE(LA);
    FREE(LAruleno);
    FREE(accessing_symbol);

    goto_actions();
    FREE(goto_map + ntokens);
    FREE(from_state);
    FREE(to_state);

    sort_actions();
    pack_table();
    output_base();
    output_table();
    output_check();
}


static void
token_actions()
{
    int i, j;
    int shiftcount, reducecount;
    int max, min;
    short *actionrow, *r, *s;
    action *p;

    actionrow = NEW2(2*ntokens, short);
    for (i = 0; i < nstates; ++i)
    {
	if (parser[i])
	{
	    for (j = 0; j < 2*ntokens; ++j)
	    actionrow[j] = 0;

	    shiftcount = 0;
	    reducecount = 0;
	    for (p = parser[i]; p; p = p->next)
	    {
		if (p->suppressed == 0)
		{
		    if (p->action_code == SHIFT)
		    {
			++shiftcount;
			actionrow[p->symbol] = p->number;
		    }
		    else if (p->action_code == REDUCE && p->number != defred[i])
		    {
			++reducecount;
			actionrow[p->symbol + ntokens] = p->number;
		    }
		}
	    }

	    tally[i] = shiftcount;
	    tally[nstates+i] = reducecount;
	    width[i] = 0;
	    width[nstates+i] = 0;
	    if (shiftcount > 0)
	    {
		froms[i] = r = NEW2(shiftcount, short);
		tos[i] = s = NEW2(shiftcount, short);
		min = MAXSHORT;
		max = 0;
		for (j = 0; j < ntokens; ++j)
		{
		    if (actionrow[j])
		    {
			if (min > symbol_value[j])
			    min = symbol_value[j];
			if (max < symbol_value[j])
			    max = symbol_value[j];
			*r++ = symbol_value[j];
			*s++ = actionrow[j];
		    }
		}
		width[i] = max - min + 1;
	    }
	    if (reducecount > 0)
	    {
		froms[nstates+i] = r = NEW2(reducecount, short);
		tos[nstates+i] = s = NEW2(reducecount, short);
		min = MAXSHORT;
		max = 0;
		for (j = 0; j < ntokens; ++j)
		{
		    if (actionrow[ntokens+j])
		    {
			if (min > symbol_value[j])
			    min = symbol_value[j];
			if (max < symbol_value[j])
			    max = symbol_value[j];
			*r++ = symbol_value[j];
			*s++ = actionrow[ntokens+j] - 2;
		    }
		}
		width[nstates+i] = max - min + 1;
	    }
	}
    }
    FREE(actionrow);
}

static void
goto_actions()
{
    int i, j, k;

    state_count = NEW2(nstates, short);

    k = default_goto(start_symbol + 1);
    fprintf(output_file, "const short %sdgoto[] = {%40d,", symbol_prefix, k);
    save_column(start_symbol + 1, k);

    j = 10;
    for (i = start_symbol + 2; i < nsyms; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
	else
	    ++j;

	k = default_goto(i);
	fprintf(output_file, "%5d,", k);
	save_column(i, k);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");
    FREE(state_count);
}

static int
default_goto(symbol)
int symbol;
{
    int i;
    int m;
    int n;
    int default_state;
    int max;

    m = goto_map[symbol];
    n = goto_map[symbol + 1];

    if (m == n) return (0);

    for (i = 0; i < nstates; i++)
	state_count[i] = 0;

    for (i = m; i < n; i++)
	state_count[to_state[i]]++;

    max = 0;
    default_state = 0;
    for (i = 0; i < nstates; i++)
    {
	if (state_count[i] > max)
	{
	    max = state_count[i];
	    default_state = i;
	}
    }

    return (default_state);
}



static void
save_column(symbol, default_state)
int symbol;
int default_state;
{
    int i;
    int m;
    int n;
    short *sp;
    short *sp1;
    short *sp2;
    int count;
    int symno;

    m = goto_map[symbol];
    n = goto_map[symbol + 1];

    count = 0;
    for (i = m; i < n; i++)
    {
	if (to_state[i] != default_state)
	    ++count;
    }
    if (count == 0) return;

    symno = symbol_value[symbol] + 2*nstates;

    froms[symno] = sp1 = sp = NEW2(count, short);
    tos[symno] = sp2 = NEW2(count, short);

    for (i = m; i < n; i++)
    {
	if (to_state[i] != default_state)
	{
	    *sp1++ = from_state[i];
	    *sp2++ = to_state[i];
	}
    }

    tally[symno] = count;
    width[symno] = sp1[-1] - sp[0] + 1;
}

static void
sort_actions()
{
  int i;
  int j;
  int k;
  int t;
  int w;

  order = NEW2(nvectors, short);
  nentries = 0;

  for (i = 0; i < nvectors; i++)
    {
      if (tally[i] > 0)
	{
	  t = tally[i];
	  w = width[i];
	  j = nentries - 1;

	  while (j >= 0 && (width[order[j]] < w))
	    j--;

	  while (j >= 0 && (width[order[j]] == w) && (tally[order[j]] < t))
	    j--;

	  for (k = nentries - 1; k > j; k--)
	    order[k + 1] = order[k];

	  order[j + 1] = i;
	  nentries++;
	}
    }
}


static void
pack_table()
{
    int i;
    int place;
    int state;

    base = NEW2(nvectors, short);
    pos = NEW2(nentries, short);

    maxtable = 10000;
    table = NEW2(maxtable, short);
    check = NEW2(maxtable, short);

    lowzero = 0;
    high = 0;

    for (i = 0; i < maxtable; i++)
	check[i] = -1;

    for (i = 0; i < nentries; i++)
    {
	state = matching_vector(i);

	if (state < 0)
	    place = pack_vector(i);
	else
	    place = base[state];

	pos[i] = place;
	base[order[i]] = place;
    }

    for (i = 0; i < nvectors; i++)
    {
	if (froms[i])
	    FREE(froms[i]);
	if (tos[i])
	    FREE(tos[i]);
    }

    FREE(froms);
    FREE(tos);
    FREE(pos);
}


/*  The function matching_vector determines if the vector specified by	*/
/*  the input parameter matches a previously considered	vector.  The	*/
/*  test at the start of the function checks if the vector represents	*/
/*  a row of shifts over terminal symbols or a row of reductions, or a	*/
/*  column of shifts over a nonterminal symbol.  Berkeley Yacc does not	*/
/*  check if a column of shifts over a nonterminal symbols matches a	*/
/*  previously considered vector.  Because of the nature of LR parsing	*/
/*  tables, no two columns can match.  Therefore, the only possible	*/
/*  match would be between a row and a column.  Such matches are	*/
/*  unlikely.  Therefore, to save time, no attempt is made to see if a	*/
/*  column matches a previously considered vector.			*/
/*									*/
/*  Matching_vector is poorly designed.  The test could easily be made	*/
/*  faster.  Also, it depends on the vectors being in a specific	*/
/*  order.								*/

static int
matching_vector(vector)
int vector;
{
    int i;
    int j;
    int k;
    int t;
    int w;
    int match;
    int prev;

    i = order[vector];
    if (i >= 2*nstates)
	return (-1);

    t = tally[i];
    w = width[i];

    for (prev = vector - 1; prev >= 0; prev--)
    {
	j = order[prev];
	if (width[j] != w || tally[j] != t)
	    return (-1);

	match = 1;
	for (k = 0; match && k < t; k++)
	{
	    if (tos[j][k] != tos[i][k] || froms[j][k] != froms[i][k])
		match = 0;
	}

	if (match)
	    return (j);
    }

    return (-1);
}



static int
pack_vector(vector)
int vector;
{
    int i, j, k;
    int t;
    int loc;
    int ok;
    short *from;
    short *to;

    loc = 0;
    i = order[vector];
    t = tally[i];
    assert(t);

    from = froms[i];
    to = tos[i];

    j = lowzero - from[0];
    for (k = 1; k < t; ++k)
	if (lowzero - from[k] > j)
	    j = lowzero - from[k];
    for (;; ++j)
    {
	if (j == 0)
	    continue;
	ok = 1;
	for (k = 0; ok && k < t; k++)
	{
	    loc = j + from[k];
	    if (loc >= maxtable)
	    {
		if (loc >= MAXTABLE)
		    fatal("maximum table size exceeded");
		maxtable = increase_maxtable(loc);
	    }

	    if (check[loc] != -1)
		ok = 0;
	}
	for (k = 0; ok && k < vector; k++)
	{
	    if (pos[k] == j)
		ok = 0;
	}
	if (ok)
	{
	    for (k = 0; k < t; k++)
	    {
		loc = j + from[k];
		table[loc] = to[k];
		check[loc] = from[k];
		if (loc > high) high = loc;
	    }

	    while (check[lowzero] != -1)
	    {
		if (lowzero >= maxtable)
		{
		    if (lowzero >= MAXTABLE)
		    {
		      fatal("maximum table size exceeded in check\n");
		    }

		    maxtable = increase_maxtable(loc);
		}

		++lowzero;
	    }

	    return (j);
	}
    }
}



static void
output_base()
{
    int i, j;

    fprintf(output_file, "const short %ssindex[] = {%39d,", symbol_prefix,
	    base[0]);

    j = 10;
    for (i = 1; i < nstates; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
	else
	    ++j;

	fprintf(output_file, "%5d,", base[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\nconst short %srindex[] = {%39d,", symbol_prefix,
	    base[nstates]);

    j = 10;
    for (i = nstates + 1; i < 2*nstates; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
	else
	    ++j;

	fprintf(output_file, "%5d,", base[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\nconst short %sgindex[] = {%39d,", symbol_prefix,
	    base[2*nstates]);

    j = 10;
    for (i = 2*nstates + 1; i < nvectors - 1; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
	else
	    ++j;

	fprintf(output_file, "%5d,", base[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");
    FREE(base);
}



static void
output_table()
{
    int i;
    int j;

    ++outline;
    fprintf(code_file, "#define YYTABLESIZE %d\n", high);
    fprintf(output_file, "const short %stable[] = {%40d,", symbol_prefix,
	    table[0]);

    j = 10;
    for (i = 1; i <= high; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
	else
	    ++j;

	fprintf(output_file, "%5d,", table[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");
    FREE(table);
}



static void
output_check()
{
    int i;
    int j;

    fprintf(output_file, "const short %scheck[] = {%40d,", symbol_prefix,
	    check[0]);

    j = 10;
    for (i = 1; i <= high; i++)
    {
	if (j >= 10)
	{
	    if (!rflag) ++outline;
	    putc('\n', output_file);
	    j = 1;
	}
	else
	    ++j;

	fprintf(output_file, "%5d,", check[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");
    FREE(check);
}


static int
is_C_identifier(name)
char *name;
{
    char *s;
    int c;

    s = name;
    c = *s;
    if (c == '"')
    {
	c = *++s;
	if (!isalpha(c) && c != '_' && c != '$')
	    return (0);
	while ((c = *++s) != '"')
	{
	    if (!isalnum(c) && c != '_' && c != '$')
		return (0);
	}
	return (1);
    }

    if (!isalpha(c) && c != '_' && c != '$')
	return (0);
    while ((c = *++s))
    {
	if (!isalnum(c) && c != '_' && c != '$')
	    return (0);
    }
    return (1);
}


static void
output_defines()
{
    int c, i;
    char *s;

    ++outline;
    fprintf(code_file, "#define YYERRCODE %d\n", symbol_value[1]);

    if(dflag)
    {
        fprintf(defines_file, "#ifndef YYERRCODE\n");
        fprintf(defines_file, "#define YYERRCODE %d\n", symbol_value[1]);
        fprintf(defines_file, "#endif\n\n");
    }
    for (i = 2; i < ntokens; ++i)
    {
	s = symbol_name[i];
	if (is_C_identifier(s))
	{
	    fprintf(code_file, "#define ");
	    if (dflag) fprintf(defines_file, "#define ");
	    c = *s;
	    if (c == '"')
	    {
		while ((c = *++s) != '"')
		{
		    putc(c, code_file);
		    if (dflag) putc(c, defines_file);
		}
	    }
	    else
	    {
		do
		{
		    putc(c, code_file);
		    if (dflag) putc(c, defines_file);
		}
		while ((c = *++s));
	    }
	    ++outline;
	    fprintf(code_file, " %d\n", symbol_value[i]);
	    if (dflag) fprintf(defines_file, " %d\n", symbol_value[i]);
	}
    }

    if (dflag && unionized)
    {
	fclose(union_file);
	union_file = fopen(union_file_name, "r");
	if (union_file == NULL) open_error(union_file_name);
	while ((c = getc(union_file)) != EOF)
	    putc(c, defines_file);
	fprintf(defines_file, " YYSTYPE;\nextern YYSTYPE %slval;\n",
		symbol_prefix);
    }
}


static void
output_stored_text()
{
    int c;
    FILE *in, *out;

    fclose(text_file);
    text_file = fopen(text_file_name, "r");
    if (text_file == NULL)
	open_error(text_file_name);
    in = text_file;
    if ((c = getc(in)) == EOF)
	return;
    out = code_file;
    if (c ==  '\n')
	++outline;
    putc(c, out);
    while ((c = getc(in)) != EOF)
    {
	if (c == '\n')
	    ++outline;
	putc(c, out);
    }
    if (!lflag)
	fprintf(out, line_format, ++outline + 1, code_file_name);
}


static void
output_debug()
{
    int i, j, k, max;
    char **symnam, *s;
    static char eof[] = "end-of-file";

    ++outline;
    fprintf(code_file, "#define YYFINAL %d\n", final_state);
    outline += 3;
    fprintf(code_file, "#ifndef YYDEBUG\n#define YYDEBUG %d\n#endif\n",
	    tflag);
    if (rflag)
	fprintf(output_file, "#ifndef YYDEBUG\n#define YYDEBUG %d\n#endif\n",
		tflag);

    max = 0;
    for (i = 2; i < ntokens; ++i)
	if (symbol_value[i] > max)
	    max = symbol_value[i];
    ++outline;
    fprintf(code_file, "#define YYMAXTOKEN %d\n", max);

    symnam = (char **) MALLOC((max+1)*sizeof(char *));
    if (symnam == 0) no_space();

    /* Note that it is  not necessary to initialize the element		*/
    /* symnam[max].							*/
    for (i = 0; i < max; ++i)
	symnam[i] = 0;
    for (i = ntokens - 1; i >= 2; --i)
	symnam[symbol_value[i]] = symbol_name[i];
    symnam[0] = eof;

    if (!rflag) ++outline;
    fprintf(output_file, "#if YYDEBUG\n");
    fprintf(output_file, "const char * const %sname[] = {", symbol_prefix);
    j = 80;
    for (i = 0; i <= max; ++i)
    {
	if ((s = symnam[i]))
	{
	    if (s[0] == '"')
	    {
		k = 7;
		while (*++s != '"')
		{
		    ++k;
		    if (*s == '\\')
		    {
			k += 2;
			if (*++s == '\\')
			    ++k;
		    }
		}
		j += k;
		if (j > 80)
		{
		    if (!rflag) ++outline;
		    putc('\n', output_file);
		    j = k;
		}
		fprintf(output_file, "\"\\\"");
		s = symnam[i];
		while (*++s != '"')
		{
		    if (*s == '\\')
		    {
			fprintf(output_file, "\\\\");
			if (*++s == '\\')
			    fprintf(output_file, "\\\\");
			else
			    putc(*s, output_file);
		    }
		    else
			putc(*s, output_file);
		}
		fprintf(output_file, "\\\"\",");
	    }
	    else if (s[0] == '\'')
	    {
		if (s[1] == '"')
		{
		    j += 7;
		    if (j > 80)
		    {
			if (!rflag) ++outline;
			putc('\n', output_file);
			j = 7;
		    }
		    fprintf(output_file, "\"'\\\"'\",");
		}
		else
		{
		    k = 5;
		    while (*++s != '\'')
		    {
			++k;
			if (*s == '\\')
			{
			    k += 2;
			    if (*++s == '\\')
				++k;
			}
		    }
		    j += k;
		    if (j > 80)
		    {
			if (!rflag) ++outline;
			putc('\n', output_file);
			j = k;
		    }
		    fprintf(output_file, "\"'");
		    s = symnam[i];
		    while (*++s != '\'')
		    {
			if (*s == '\\')
			{
			    fprintf(output_file, "\\\\");
			    if (*++s == '\\')
				fprintf(output_file, "\\\\");
			    else
				putc(*s, output_file);
			}
			else
			    putc(*s, output_file);
		    }
		    fprintf(output_file, "'\",");
		}
	    }
	    else
	    {
		k = strlen(s) + 3;
		j += k;
		if (j > 80)
		{
		    if (!rflag) ++outline;
		    putc('\n', output_file);
		    j = k;
		}
		putc('"', output_file);
		do { putc(*s, output_file); } while (*++s);
		fprintf(output_file, "\",");
	    }
	}
	else
	{
	    j += 2;
	    if (j > 80)
	    {
		if (!rflag) ++outline;
		putc('\n', output_file);
		j = 2;
	    }
	    fprintf(output_file, "0,");
	}
    }
    if (!rflag) outline += 2;
    fprintf(output_file, "\n};\n");
    FREE(symnam);

    if (!rflag) ++outline;
    fprintf(output_file, "const char * const %srule[] = {\n", symbol_prefix);
    for (i = 2; i < nrules; ++i)
    {
	fprintf(output_file, "\"%s :", symbol_name[rlhs[i]]);
	for (j = rrhs[i]; ritem[j] > 0; ++j)
	{
	    s = symbol_name[ritem[j]];
	    if (s[0] == '"')
	    {
		fprintf(output_file, " \\\"");
		while (*++s != '"')
		{
		    if (*s == '\\')
		    {
			if (s[1] == '\\')
			    fprintf(output_file, "\\\\\\\\");
			else
			    fprintf(output_file, "\\\\%c", s[1]);
			++s;
		    }
		    else
			putc(*s, output_file);
		}
		fprintf(output_file, "\\\"");
	    }
	    else if (s[0] == '\'')
	    {
		if (s[1] == '"')
		    fprintf(output_file, " '\\\"'");
		else if (s[1] == '\\')
		{
		    if (s[2] == '\\')
			fprintf(output_file, " '\\\\\\\\");
		    else
			fprintf(output_file, " '\\\\%c", s[2]);
		    s += 2;
		    while (*++s != '\'')
			putc(*s, output_file);
		    putc('\'', output_file);
		}
		else
		    fprintf(output_file, " '%c'", s[1]);
	    }
	    else
		fprintf(output_file, " %s", s);
	}
	if (!rflag) ++outline;
	fprintf(output_file, "\",\n");
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "};\n#endif\n");
}


static void
output_stype()
{
    if (!unionized && ntags == 0)
    {
	outline += 3;
	fprintf(code_file, "#ifndef YYSTYPE\ntypedef int YYSTYPE;\n#endif\n");
    }
}


static void
output_trailing_text()
{
    int c, last;
    FILE *in, *out;

    if (line == 0)
	return;

    in = input_file;
    out = code_file;
    c = *cptr;
    if (c == '\n')
    {
	++lineno;
	if ((c = getc(in)) == EOF)
	    return;
	if (!lflag)
	{
	    ++outline;
	    fprintf(out, line_format, lineno, input_file_name);
	}
	if (c == '\n')
	    ++outline;
	putc(c, out);
	last = c;
    }
    else
    {
	if (!lflag)
	{
	    ++outline;
	    fprintf(out, line_format, lineno, input_file_name);
	}
	do { putc(c, out); } while ((c = *++cptr) != '\n');
	++outline;
	putc('\n', out);
	last = '\n';
    }

    while ((c = getc(in)) != EOF)
    {
	if (c == '\n')
	    ++outline;
	putc(c, out);
	last = c;
    }

    if (last != '\n')
    {
	++outline;
	putc('\n', out);
    }
    if (!lflag)
	fprintf(out, line_format, ++outline + 1, code_file_name);
}


static void
output_semantic_actions()
{
    int c, last;
    FILE *out;

    fclose(action_file);
    action_file = fopen(action_file_name, "r");
    if (action_file == NULL)
	open_error(action_file_name);

    if ((c = getc(action_file)) == EOF)
	return;

    out = code_file;
    last = c;
    if (c == '\n')
	++outline;
    putc(c, out);
    while ((c = getc(action_file)) != EOF)
    {
	if (c == '\n')
	    ++outline;
	putc(c, out);
	last = c;
    }

    if (last != '\n')
    {
	++outline;
	putc('\n', out);
    }

    if (!lflag)
	fprintf(out, line_format, ++outline + 1, code_file_name);
}


static void
free_itemsets()
{
    core *cp, *next;

    FREE(state_table);
    for (cp = first_state; cp; cp = next)
    {
	next = cp->next;
	FREE(cp);
    }
}


static void
free_shifts()
{
    shifts *sp, *next;

    FREE(shift_table);
    for (sp = first_shift; sp; sp = next)
    {
	next = sp->next;
	FREE(sp);
    }
}



static void
free_reductions()
{
    reductions *rp, *next;

    FREE(reduction_table);
    for (rp = first_reduction; rp; rp = next)
    {
	next = rp->next;
	FREE(rp);
    }
}

/*
 * increase_maxtable
 *
 * inputs	- loc location in table
 * output	- size increased to
 * side effects	- table is increase by at least 200 short words
 */

static int
increase_maxtable(int loc)
{
    int newmax;
    int l;

    newmax = maxtable;

    do { newmax += 200; } while (newmax <= loc);
    table = (short *) REALLOC(table, newmax*sizeof(short));
	if (table == 0) no_space();
	check = (short *) REALLOC(check, newmax*sizeof(short));
	if (check == 0) no_space();
	for (l  = maxtable; l < newmax; ++l)
	{
	    table[l] = 0;
	    check[l] = -1;
	}

    return(newmax);
}
